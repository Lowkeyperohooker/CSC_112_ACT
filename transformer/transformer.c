#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

#define MAX_NAME_LENGTH 32
#define MAX_TOKENS 1000
#define MAX_SYMBOLS 100
#define MAX_ERRORS 100

// --- Enumerations ---

typedef enum {
    END_OF_FILE, NUMBER, IDENTIFIER, PLUS, MINUS,
    MULTIPLY, DIVIDE, ASSIGN, SEMICOLON, LEFT_PAREN,
    RIGHT_PAREN, INT_KEYWORD, CHAR_KEYWORD, FLOAT_KEYWORD,
    UNKNOWN_TOKEN, INCREMENT, DECREMENT,
    PLUS_ASSIGN, MINUS_ASSIGN,
    MULTIPLY_ASSIGN, DIVIDE_ASSIGN,
    COMMA, CHAR_LITERAL, FLOAT_LITERAL
} TokenType;

typedef enum {
    PROGRAM_NODE, ASSIGNMENT_NODE, VARIABLE_NODE,
    NUMBER_NODE, OPERATION_NODE, UNARY_NODE,
    COMPOUND_ASSIGN_NODE, CHAR_NODE, DECLARATION_NODE,
    FLOAT_NODE
} ASTNodeType;

// --- Structures ---

typedef struct {
    TokenType type;
    char text[MAX_NAME_LENGTH];
    int line_number;
} Token;

typedef struct {
    char name[MAX_NAME_LENGTH];
    int is_initialized;
    int is_used;
    int memory_location;
    int size;
    char type;  // 'i' for int, 'c' for char, 'f' for float (treated as double)
} Symbol;

typedef struct ASTNode {
    ASTNodeType node_type;
    Token token_info;
    struct ASTNode *left_child;
    struct ASTNode *right_child;
    struct ASTNode *next;
} ASTNode;

typedef struct {
    int error_count;
    char error_messages[MAX_ERRORS][256];
} ErrorList;

typedef struct {
    const char* available_registers[32];
    int next_register_index;
    int used_registers[32];
    int register_count;
} RegisterPool;

// --- Global Variables ---

Token all_tokens[MAX_TOKENS];
int current_token_count = 0;
int current_token_position = 0;
Symbol symbol_table[MAX_SYMBOLS];
int symbols_found = 0;
int next_memory_location = 0;
ErrorList error_log = {0};
RegisterPool register_pool = {0};

// --- Function Prototypes ---

char* get_register(void);
void release_register_by_name(const char* reg_name);
const char* get_float_register(void);
void release_float_register(const char* reg_name);

// --- Instruction Table Definitions ---

typedef struct {
    const char* instruction_name;
    unsigned int opcode_value;
    unsigned int instruction_format;
    unsigned int sub_code;
    unsigned int function_code;
} Instruction;

Instruction supported_instructions[] = {
    // Integer instructions (I-type / R-type)
    {"daddiu", 0b011001, 1, 0, 0},
    {"lb",     0b100000, 1, 0, 0},
    {"sb",     0b101000, 1, 0, 0},
    {"daddu",  0b000000, 0, 0, 0b101101},
    {"dsubu",  0b000000, 0, 0, 0b101111},
    {"dmulu",  0b000000, 0, 0b00010, 0b011101},
    {"ddivu",  0b000000, 0, 0b00010, 0b011111},
    {"mflo",   0b000000, 0, 0, 0b010010},

    // Logical & Shift
    {"or",     0b000000, 0, 0, 0b100101}, // R-Type OR
    {"dsll",   0b000000, 3, 0, 0b111000}, // R-Type Shift Left

    // Floating point load/store (Double Precision)
    {"l.d",    0b110101, 1, 0, 0},
    {"s.d",    0b111101, 1, 0, 0},

    // Floating point arithmetic (Double Precision)
    {"add.d",  0b010001, 2, 0b10001, 0b000000},
    {"sub.d",  0b010001, 2, 0b10001, 0b000001},
    {"mul.d",  0b010001, 2, 0b10001, 0b000010},
    {"div.d",  0b010001, 2, 0b10001, 0b000011},

    // Move instructions
    {"mfc1",   0b010001, 2, 0b00000, 0b000000}, // Move 32-bit
    {"mtc1",   0b010001, 2, 0b00100, 0b000000}, // Move 32-bit
    {"dmtc1",  0b010001, 2, 0b00101, 0b000000}, // Move 64-bit (Double)

    // Conversion instructions
    {"cvt.d.w",0b010001, 2, 0b10100, 0b100001}, // Convert Word to Double
    {"cvt.d.l",0b010001, 2, 0b10101, 0b100001}, // Convert Long to Double

    // Immediate instructions
    {"lui",    0b001111, 1, 0, 0},
    {"ori",    0b001101, 1, 0, 0}
};

// --- Machine Code Generation Functions ---

int find_instruction(const char* instruction_name) {
    int total_instructions = sizeof(supported_instructions) / sizeof(supported_instructions[0]);
    for (int i = 0; i < total_instructions; i++) {
        if (strcmp(supported_instructions[i].instruction_name, instruction_name) == 0) {
            return i;
        }
    }
    return -1;
}

unsigned int create_instruction_code(const char* instruction_name, int source_reg,
                                     int target_reg, int dest_reg, int immediate_value) {
    int instruction_index = find_instruction(instruction_name);
    if (instruction_index == -1) {
        printf("Error: Instruction '%s' not found\n", instruction_name);
        return 0;
    }

    Instruction inst = supported_instructions[instruction_index];

    if (inst.instruction_format == 1) {
        // I-type: opcode(6) | rs(5) | rt(5) | immediate(16)
        unsigned int immediate_16bit = immediate_value & 0xFFFF;
        unsigned int instruction = (inst.opcode_value << 26) |
                                   (source_reg << 21) |
                                   (target_reg << 16) |
                                   immediate_16bit;
        return instruction;

    } else if (inst.instruction_format == 2) {
        // COP1 Format
        if (strcmp(instruction_name, "mfc1") == 0) {
            return (inst.opcode_value << 26) | (inst.sub_code << 21) | (target_reg << 16) | (source_reg << 11) | inst.function_code;
        } else if (strcmp(instruction_name, "mtc1") == 0) {
            return (inst.opcode_value << 26) | (inst.sub_code << 21) | (target_reg << 16) | (source_reg << 11) | inst.function_code;
        } else if (strcmp(instruction_name, "dmtc1") == 0) {
            // dmtc1 rt, fs -> 010001 00101 rt fs 00000000000
            return (inst.opcode_value << 26) | (inst.sub_code << 21) | (target_reg << 16) | (source_reg << 11) | inst.function_code;
        } else if (strncmp(instruction_name, "cvt.", 4) == 0) {
            return (inst.opcode_value << 26) | (inst.sub_code << 21) | (0 << 16) | (source_reg << 11) | (dest_reg << 6) | inst.function_code;
        } else {
            // Arithmetic (add.d etc)
            return (inst.opcode_value << 26) | (inst.sub_code << 21) | (target_reg << 16) | (source_reg << 11) | (dest_reg << 6) | inst.function_code;
        }
    } else if (inst.instruction_format == 3) {
        // Shift Format (dsll): opcode(0) | rs(0) | rt(src) | rd(dest) | sa(shift) | func
        // Using args: source_reg as 'rt', dest_reg as 'rd', immediate as 'sa'
        return (inst.opcode_value << 26) | (0 << 21) | (source_reg << 16) | (dest_reg << 11) | ((immediate_value & 0x1F) << 6) | inst.function_code;
    } else {
        // R-type: opcode | rs | rt | rd | sa | func
        if (strcmp(instruction_name, "mflo") == 0) {
            return (inst.opcode_value << 26) | (0 << 21) | (0 << 16) | (dest_reg << 11) | (0 << 6) | inst.function_code;
        } else if (strcmp(instruction_name, "or") == 0) {
            return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) | (dest_reg << 11) | (0 << 6) | inst.function_code;
        } else if (strcmp(instruction_name, "dmulu") == 0 || strcmp(instruction_name, "ddivu") == 0) {
            return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) | (0 << 11) | (inst.sub_code << 6) | inst.function_code;
        } else {
            return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) | (dest_reg << 11) | (0 << 6) | inst.function_code;
        }
    }
    return 0;
}

void display_binary_code(unsigned int value, FILE* output) {
    fprintf(output, ";");
    for (int i = 31; i >= 0; i--) {
        fprintf(output, "%d", (value >> i) & 1);
        if (i % 4 == 0 && i != 0) fprintf(output, " ");
    }
}

void produce_machine_code(const char* instruction_name, int source_reg, int target_reg,
                          int dest_reg, int immediate_value, FILE* output) {
    unsigned int machine_instruction = create_instruction_code(instruction_name, source_reg,
                                       target_reg, dest_reg, immediate_value);
    if (machine_instruction != 0) {
        fprintf(output, " ");
        display_binary_code(machine_instruction, output);
        fprintf(output, "\n");
    } else {
        fprintf(output, " ; ERROR: Could not encode instruction '%s'\n", instruction_name);
    }
}

// --- Register Management ---

int get_register_number(const char* register_name) {
    if (register_name[0] == 'r' || register_name[0] == 'R') {
        if (register_name[1] != '\0') {
            int reg_num = atoi(register_name + 1);
            if (reg_num >= 0 && reg_num <= 31) return reg_num;
        }
    } else if (register_name[0] == 'f' || register_name[0] == 'F') {
        if (register_name[1] != '\0') {
            int reg_num = atoi(register_name + 1);
            if (reg_num >= 0 && reg_num <= 31) return reg_num;
        }
    }
    return 0;
}

const char* get_float_register() {
    static char float_reg[8];
    for (int i = 1; i < 32; i++) {
        if (!register_pool.used_registers[i]) {
            register_pool.used_registers[i] = 1;
            snprintf(float_reg, sizeof(float_reg), "f%d", i);
            return float_reg;
        }
    }
    return "f31";
}

void release_float_register(const char* reg_name) {
    if (reg_name[0] == 'f' || reg_name[0] == 'F') {
        int reg_num = atoi(reg_name + 1);
        if (reg_num >= 1 && reg_num < 32) {
            register_pool.used_registers[reg_num] = 0;
        }
    }
}

// Helper to construct 64-bit float constants
void load_float_constant(double value, const char* float_reg, FILE* output) {
    unsigned long long bits;
    memcpy(&bits, &value, sizeof(double));

    // Split into high and low 32-bit chunks
    unsigned int high_32 = (bits >> 32) & 0xFFFFFFFF;
    unsigned int low_32 = bits & 0xFFFFFFFF;

    char* r_high = get_register();
    char* r_low = get_register();

    // 1. Construct High 32-bit part in r_high
    fprintf(output, "    lui %s, 0x%X\n", r_high, (high_32 >> 16) & 0xFFFF);
    produce_machine_code("lui", 0, get_register_number(r_high), -1, (high_32 >> 16) & 0xFFFF, output);

    fprintf(output, "    ori %s, %s, 0x%X\n", r_high, r_high, high_32 & 0xFFFF);
    produce_machine_code("ori", get_register_number(r_high), get_register_number(r_high), -1, high_32 & 0xFFFF, output);

    // 2. Shift r_high left by 32 bits (Two 16-bit shifts because dsll only accepts 0-31)
    fprintf(output, "    dsll %s, %s, 16\n", r_high, r_high);
    produce_machine_code("dsll", 0, get_register_number(r_high), get_register_number(r_high), 16, output);

    fprintf(output, "    dsll %s, %s, 16\n", r_high, r_high);
    produce_machine_code("dsll", 0, get_register_number(r_high), get_register_number(r_high), 16, output);

    // 3. Construct Low 32-bit part in r_low
    fprintf(output, "    lui %s, 0x%X\n", r_low, (low_32 >> 16) & 0xFFFF);
    produce_machine_code("lui", 0, get_register_number(r_low), -1, (low_32 >> 16) & 0xFFFF, output);

    fprintf(output, "    ori %s, %s, 0x%X\n", r_low, r_low, low_32 & 0xFFFF);
    produce_machine_code("ori", get_register_number(r_low), get_register_number(r_low), -1, low_32 & 0xFFFF, output);

    // 4. Combine: r_high = r_high | r_low
    fprintf(output, "    or %s, %s, %s\n", r_high, r_high, r_low);
    produce_machine_code("or", get_register_number(r_high), get_register_number(r_low), get_register_number(r_high), 0, output);

    // 5. Move 64-bit value to FPU (dmtc1) - NO CONVERSION NEEDED, IT IS ALREADY A DOUBLE
    fprintf(output, "    dmtc1 %s, %s\n", r_high, float_reg);
    produce_machine_code("dmtc1", get_register_number(r_high), get_register_number(float_reg), -1, 0, output);

    release_register_by_name(r_high);
    release_register_by_name(r_low);
}

void free_program_tree(ASTNode* node) {
    if (!node) return;
    free_program_tree(node->left_child);
    free_program_tree(node->right_child);
    free_program_tree(node->next);
    free(node);
}

// --- Error Handling ---

void record_error(int line_number, const char* message_format, ...) {
    if (error_log.error_count < MAX_ERRORS) {
        va_list args;
        va_start(args, message_format);
        vsnprintf(error_log.error_messages[error_log.error_count], 256, message_format, args);
        snprintf(error_log.error_messages[error_log.error_count] +
                 strlen(error_log.error_messages[error_log.error_count]),
                 256 - strlen(error_log.error_messages[error_log.error_count]),
                 " at line %d", line_number);
        va_end(args);
        error_log.error_count++;
    }
}

void display_errors() {
    for (int i = 0; i < error_log.error_count; i++) {
        fprintf(stderr, "Error: %s\n", error_log.error_messages[i]);
        printf("Error: %s\n", error_log.error_messages[i]);
    }
}

// --- Register Setup and Tokenizer Utilities ---

void setup_registers() {
    const char* register_names[] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
        "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
    };

    for (int i = 0; i < 32; i++) {
        register_pool.available_registers[i] = register_names[i];
    }

    register_pool.next_register_index = 1;
    register_pool.register_count = 32;
    memset(register_pool.used_registers, 0, sizeof(register_pool.used_registers));
    register_pool.used_registers[0] = 1;
}

char* get_register() {
    for (int i = 1; i < register_pool.register_count; i++) {
        if (!register_pool.used_registers[i]) {
            register_pool.used_registers[i] = 1;
            return (char*)register_pool.available_registers[i];
        }
    }
    return (char*)"r31";
}

void release_register_by_name(const char* reg_name) {
    for (int i = 0; i < register_pool.register_count; i++) {
        if (strcmp(register_pool.available_registers[i], reg_name) == 0) {
            register_pool.used_registers[i] = 0;
            break;
        }
    }
}

void clear_registers() {
    memset(register_pool.used_registers, 0, sizeof(register_pool.used_registers));
    register_pool.used_registers[0] = 1;
}

bool IS_WHITESPACE(char c) {
    return ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r');
}

bool IS_LETTER(char c) {
    return (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_');
}

bool IS_DIGIT(char c) {
    return ((c) >= '0' && (c) <= '9');
}

bool IS_ALPHANUMERIC(char c) {
    return (IS_LETTER(c) || IS_DIGIT(c));
}

TokenType identify_keyword(const char* word) {
    if (strcmp(word, "int") == 0) return INT_KEYWORD;
    if (strcmp(word, "char") == 0) return CHAR_KEYWORD;
    if (strcmp(word, "float") == 0) return FLOAT_KEYWORD;
    return IDENTIFIER;
}

// --- Lexer (Tokenizer) ---

void save_token(TokenType type, const char* text_value, int line_number) {
    if (current_token_count < MAX_TOKENS) {
        all_tokens[current_token_count].type = type;
        all_tokens[current_token_count].line_number = line_number;
        strncpy(all_tokens[current_token_count].text, text_value, MAX_NAME_LENGTH - 1);
        all_tokens[current_token_count].text[MAX_NAME_LENGTH - 1] = '\0';
        current_token_count++;
    } else {
        record_error(line_number, "Too many tokens in program");
    }
}

void skip_spaces_and_comments(const char* source_code, int* position, int* current_line) {
    while (source_code[*position] != '\0') {
        if (IS_WHITESPACE(source_code[*position])) {
            if (source_code[*position] == '\n') (*current_line)++;
            (*position)++;
        } else if (source_code[*position] == '/' && source_code[*position + 1] == '/') {
            *position += 2;
            while (source_code[*position] != '\0' && source_code[*position] != '\n') {
                (*position)++;
            }
            if (source_code[*position] == '\n') {
                (*current_line)++;
                (*position)++;
            }
        } else if (source_code[*position] == '/' && source_code[*position + 1] == '*') {
            *position += 2;
            int comment_depth = 1;
            while (source_code[*position] != '\0' && comment_depth > 0) {
                if (source_code[*position] == '\n') (*current_line)++;
                else if (source_code[*position] == '/' && source_code[*position + 1] == '*') {
                    comment_depth++;
                    *position += 2;
                    continue;
                } else if (source_code[*position] == '*' && source_code[*position + 1] == '/') {
                    comment_depth--;
                    *position += 2;
                    continue;
                }
                (*position)++;
            }
        } else {
            break;
        }
    }
}

void break_into_tokens(const char* source_code) {
    int position = 0;
    int current_line = 1;

    while (source_code[position] != '\0') {
        skip_spaces_and_comments(source_code, &position, &current_line);
        if (source_code[position] == '\0') break;

        // Handle Characters
        if (source_code[position] == '\'') {
            position++;
            char char_buffer[4] = {0};
            int char_value = 0;
            if (source_code[position] == '\\') {
                position++;
                switch (source_code[position]) {
                    case 'n': char_value = '\n'; break;
                    case 't': char_value = '\t'; break;
                    case 'r': char_value = '\r'; break;
                    case '0': char_value = '\0'; break;
                    case '\\': char_value = '\\'; break;
                    case '\'': char_value = '\''; break;
                    default: char_value = source_code[position]; break;
                }
                position++;
            } else {
                char_value = (unsigned char)source_code[position];
                position++;
            }
            if (source_code[position] == '\'') {
                position++;
                char num_str[16];
                snprintf(num_str, sizeof(num_str), "%d", char_value);
                save_token(CHAR_LITERAL, num_str, current_line);
            }
            continue;
        }

        // Handle Negative Numbers
        if (source_code[position] == '-' && IS_DIGIT(source_code[position + 1])) {
            bool is_unary_minus = false;
            if (position > 0) {
                char prev_char = source_code[position - 1];
                is_unary_minus = IS_LETTER(prev_char) || IS_DIGIT(prev_char) || prev_char == ')' || prev_char == ']';
            }
            if (!is_unary_minus) {
                char number_buffer[MAX_NAME_LENGTH] = {0};
                int buffer_index = 0;
                number_buffer[buffer_index++] = source_code[position++];
                while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                    number_buffer[buffer_index++] = source_code[position++];
                }
                if (source_code[position] == '.') {
                    number_buffer[buffer_index++] = source_code[position++];
                    while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                        number_buffer[buffer_index++] = source_code[position++];
                    }
                    save_token(FLOAT_LITERAL, number_buffer, current_line);
                } else {
                    save_token(NUMBER, number_buffer, current_line);
                }
                continue;
            }
        }

        // Handle Positive Numbers with explicit +
        if (source_code[position] == '+' && IS_DIGIT(source_code[position + 1])) {
            bool is_unary_plus = false;
            if (position > 0) {
                char prev_char = source_code[position - 1];
                is_unary_plus = IS_LETTER(prev_char) || IS_DIGIT(prev_char) || prev_char == ')' || prev_char == ']';
            }
            if (!is_unary_plus) {
                char number_buffer[MAX_NAME_LENGTH] = {0};
                int buffer_index = 0;
                position++;
                while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                    number_buffer[buffer_index++] = source_code[position++];
                }
                if (source_code[position] == '.') {
                    number_buffer[buffer_index++] = source_code[position++];
                    while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                        number_buffer[buffer_index++] = source_code[position++];
                    }
                    save_token(FLOAT_LITERAL, number_buffer, current_line);
                } else {
                    save_token(NUMBER, number_buffer, current_line);
                }
                continue;
            }
        }

        // Handle Digits and Floats
        if (IS_DIGIT(source_code[position])) {
            char number_buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;
            bool is_float = false;
            while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                number_buffer[buffer_index++] = source_code[position++];
            }
            if (source_code[position] == '.') {
                is_float = true;
                number_buffer[buffer_index++] = source_code[position++];
                while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                    number_buffer[buffer_index++] = source_code[position++];
                }
            }
            if (is_float) save_token(FLOAT_LITERAL, number_buffer, current_line);
            else save_token(NUMBER, number_buffer, current_line);
            continue;
        }

        // Handle Identifiers and Keywords
        if (IS_LETTER(source_code[position])) {
            char word_buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;
            while (IS_ALPHANUMERIC(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                word_buffer[buffer_index++] = source_code[position++];
            }
            save_token(identify_keyword(word_buffer), word_buffer, current_line);
            continue;
        }

        // Handle Operators
        if (source_code[position] == '+' && source_code[position + 1] == '+') { save_token(INCREMENT, "++", current_line); position += 2; continue; }
        if (source_code[position] == '-' && source_code[position + 1] == '-') { save_token(DECREMENT, "--", current_line); position += 2; continue; }
        if (source_code[position] == '+' && source_code[position + 1] == '=') { save_token(PLUS_ASSIGN, "+=", current_line); position += 2; continue; }
        if (source_code[position] == '-' && source_code[position + 1] == '=') { save_token(MINUS_ASSIGN, "-=", current_line); position += 2; continue; }
        if (source_code[position] == '*' && source_code[position + 1] == '=') { save_token(MULTIPLY_ASSIGN, "*=", current_line); position += 2; continue; }
        if (source_code[position] == '/' && source_code[position + 1] == '=') { save_token(DIVIDE_ASSIGN, "/=", current_line); position += 2; continue; }

        switch (source_code[position]) {
            case '+': save_token(PLUS, "+", current_line); position++; break;
            case '-': save_token(MINUS, "-", current_line); position++; break;
            case '*': save_token(MULTIPLY, "*", current_line); position++; break;
            case '/': save_token(DIVIDE, "/", current_line); position++; break;
            case '=': save_token(ASSIGN, "=", current_line); position++; break;
            case ';': save_token(SEMICOLON, ";", current_line); position++; break;
            case '(': save_token(LEFT_PAREN, "(", current_line); position++; break;
            case ')': save_token(RIGHT_PAREN, ")", current_line); position++; break;
            case ',': save_token(COMMA, ",", current_line); position++; break;
            default: position++; break;
        }
    }
    save_token(END_OF_FILE, "", current_line);
}

// --- Symbol Table ---

Symbol* find_variable(const char* variable_name) {
    for (int i = 0; i < symbols_found; i++) {
        if (strcmp(symbol_table[i].name, variable_name) == 0) {
            return &symbol_table[i];
        }
    }
    return NULL;
}

bool add_variable(const char* variable_name, int line_number, char type) {
    if (symbols_found >= MAX_SYMBOLS) return false;
    if (find_variable(variable_name) != NULL) return false;
    symbol_table[symbols_found] = (Symbol){{0}, 0, 0, next_memory_location, 8, type};
    strncpy(symbol_table[symbols_found].name, variable_name, MAX_NAME_LENGTH - 1);
    symbols_found++;
    next_memory_location += 8;
    return true;
}

void mark_variable_initialized(const char* variable_name) {
    Symbol* variable = find_variable(variable_name);
    if (variable) variable->is_initialized = 1;
}

void mark_variable_used(const char* variable_name) {
    Symbol* variable = find_variable(variable_name);
    if (variable) variable->is_used = 1;
}

// --- Parsing Utilities ---

Token get_next_token() {
    return (current_token_position < current_token_count) ?
           all_tokens[current_token_position++] : all_tokens[current_token_count - 1];
}

Token peek_next_token() {
    return (current_token_position < current_token_count) ?
           all_tokens[current_token_position] : all_tokens[current_token_count - 1];
}

bool expect_token(TokenType expected_type, const char* expected_text) {
    Token next_token = peek_next_token();
    if (next_token.type != expected_type) return false;
    get_next_token();
    return true;
}

ASTNode* create_tree_node(ASTNodeType node_type, Token token_data,
                          ASTNode* left_child, ASTNode* right_child) {
    ASTNode* new_node = malloc(sizeof(ASTNode));
    *new_node = (ASTNode){node_type, token_data, left_child, right_child, NULL};
    return new_node;
}

// --- Forward Declarations for Parser ---

ASTNode* parse_expression();
ASTNode* parse_unary_expression();
ASTNode* parse_postfix_expression();
ASTNode* parse_primary_expression();
ASTNode* parse_multiplicative_expression();
ASTNode* parse_additive_expression();

// --- Parser Implementation ---

ASTNode* parse_unary_expression() {
    Token current_token = peek_next_token();
    if (current_token.type == PLUS || current_token.type == MINUS) {
        Token operator_token = get_next_token();
        ASTNode* operand = parse_unary_expression();
        return create_tree_node(UNARY_NODE, operator_token, operand, NULL);
    }
    if (current_token.type == INCREMENT || current_token.type == DECREMENT) {
        Token operator_token = get_next_token();
        ASTNode* operand = parse_primary_expression();
        return create_tree_node(UNARY_NODE, operator_token, operand, NULL);
    }
    return parse_postfix_expression();
}

ASTNode* parse_primary_expression() {
    Token current_token = peek_next_token();
    if (current_token.type == NUMBER) return create_tree_node(NUMBER_NODE, get_next_token(), NULL, NULL);
    if (current_token.type == FLOAT_LITERAL) return create_tree_node(FLOAT_NODE, get_next_token(), NULL, NULL);
    if (current_token.type == CHAR_LITERAL) return create_tree_node(CHAR_NODE, get_next_token(), NULL, NULL);
    if (current_token.type == IDENTIFIER) {
        Token token = get_next_token();
        mark_variable_used(token.text);
        return create_tree_node(VARIABLE_NODE, token, NULL, NULL);
    }
    if (current_token.type == LEFT_PAREN) {
        get_next_token();
        ASTNode* expression = parse_expression();
        expect_token(RIGHT_PAREN, ")");
        return expression;
    }
    return NULL;
}

ASTNode* parse_postfix_expression() {
    ASTNode* left_side = parse_primary_expression();
    Token next_token = peek_next_token();
    if ((next_token.type == INCREMENT || next_token.type == DECREMENT) && left_side->node_type == VARIABLE_NODE) {
        Token operator_token = get_next_token();
        return create_tree_node(UNARY_NODE, operator_token, left_side, NULL);
    }
    return left_side;
}

ASTNode* parse_multiplicative_expression() {
    ASTNode* left_side = parse_unary_expression();
    while (peek_next_token().type == MULTIPLY || peek_next_token().type == DIVIDE) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_unary_expression();
        left_side = create_tree_node(OPERATION_NODE, operator_token, left_side, right_side);
    }
    return left_side;
}

ASTNode* parse_additive_expression() {
    ASTNode* left_side = parse_multiplicative_expression();
    while (peek_next_token().type == PLUS || peek_next_token().type == MINUS) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_multiplicative_expression();
        left_side = create_tree_node(OPERATION_NODE, operator_token, left_side, right_side);
    }
    return left_side;
}

ASTNode* parse_expression() {
    return parse_additive_expression();
}

ASTNode* parse_assignment() {
    Token variable_token = get_next_token();
    Token operator_token = peek_next_token();

    if (operator_token.type == PLUS_ASSIGN || operator_token.type == MINUS_ASSIGN ||
            operator_token.type == MULTIPLY_ASSIGN || operator_token.type == DIVIDE_ASSIGN) {
        get_next_token();
        ASTNode* expression = parse_expression();
        mark_variable_initialized(variable_token.text);
        return create_tree_node(COMPOUND_ASSIGN_NODE, operator_token,
                                create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL), expression);
    }
    expect_token(ASSIGN, "=");
    Token next_token = peek_next_token();
    if (next_token.type == IDENTIFIER) {
        Token lookahead = (current_token_position + 1 < current_token_count) ?
                          all_tokens[current_token_position + 1] : all_tokens[current_token_count - 1];
        if (lookahead.type == ASSIGN) {
            ASTNode* nested_assignment = parse_assignment();
            mark_variable_initialized(variable_token.text);
            return create_tree_node(ASSIGNMENT_NODE, variable_token, nested_assignment, NULL);
        }
    }
    ASTNode* expression = parse_expression();
    mark_variable_initialized(variable_token.text);
    return create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
}

ASTNode* parse_declaration() {
    Token type_token = get_next_token();
    char var_type = (type_token.type == INT_KEYWORD) ? 'i' : (type_token.type == CHAR_KEYWORD) ? 'c' : 'f';
    ASTNode* declaration_list = NULL;
    ASTNode* last_declaration = NULL;

    while (1) {
        Token variable_token = get_next_token();
        add_variable(variable_token.text, variable_token.line_number, var_type);
        ASTNode* assignment_node = NULL;
        Token next_token = peek_next_token();

        if (next_token.type == ASSIGN || next_token.type == PLUS_ASSIGN || next_token.type == MINUS_ASSIGN ||
                next_token.type == MULTIPLY_ASSIGN || next_token.type == DIVIDE_ASSIGN) {
            get_next_token();
            ASTNode* expression = parse_expression();
            mark_variable_initialized(variable_token.text);
            assignment_node = create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
        } else {
            assignment_node = create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL);
        }

        ASTNode* decl_node = create_tree_node(DECLARATION_NODE, type_token, assignment_node, NULL);
        if (!declaration_list) declaration_list = decl_node;
        else last_declaration->next = decl_node;
        last_declaration = decl_node;

        if (peek_next_token().type == COMMA) get_next_token();
        else break;
    }
    expect_token(SEMICOLON, ";");
    return declaration_list;
}

ASTNode* parse_statement() {
    if (peek_next_token().type == SEMICOLON) { get_next_token(); return NULL; }
    if (peek_next_token().type == INT_KEYWORD || peek_next_token().type == CHAR_KEYWORD || peek_next_token().type == FLOAT_KEYWORD)
        return parse_declaration();

    if (peek_next_token().type == INCREMENT || peek_next_token().type == DECREMENT) {
        Token op_token = get_next_token();
        Token var_token = get_next_token();
        mark_variable_used(var_token.text);
        mark_variable_initialized(var_token.text);
        ASTNode* node = create_tree_node(UNARY_NODE, op_token, create_tree_node(VARIABLE_NODE, var_token, NULL, NULL), NULL);
        expect_token(SEMICOLON, ";");
        return node;
    }

    if (peek_next_token().type == IDENTIFIER) {
        Token next_token = peek_next_token();
        Token lookahead = (current_token_position + 1 < current_token_count) ? all_tokens[current_token_position + 1] : all_tokens[current_token_count - 1];
        if (lookahead.type == PLUS_ASSIGN || lookahead.type == MINUS_ASSIGN || lookahead.type == MULTIPLY_ASSIGN || lookahead.type == DIVIDE_ASSIGN) {
            Token var_token = get_next_token();
            Token op_token = get_next_token();
            mark_variable_used(var_token.text);
            ASTNode* expression = parse_expression();
            ASTNode* compound_assign = create_tree_node(COMPOUND_ASSIGN_NODE, op_token, create_tree_node(VARIABLE_NODE, var_token, NULL, NULL), expression);
            expect_token(SEMICOLON, ";");
            return compound_assign;
        } else if (lookahead.type == ASSIGN) {
            ASTNode* assignment = parse_assignment();
            expect_token(SEMICOLON, ";");
            return assignment;
        } else if (lookahead.type == INCREMENT || lookahead.type == DECREMENT) {
            Token var_token = get_next_token();
            Token op_token = get_next_token();
            mark_variable_used(var_token.text);
            mark_variable_initialized(var_token.text);
            ASTNode* node = create_tree_node(UNARY_NODE, op_token, create_tree_node(VARIABLE_NODE, var_token, NULL, NULL), NULL);
            expect_token(SEMICOLON, ";");
            return node;
        }
    }
    ASTNode* expr = parse_expression();
    if (expr) { expect_token(SEMICOLON, ";"); return expr; }
    return NULL;
}

ASTNode* parse_program() {
    ASTNode *program_start = NULL;
    ASTNode *current_statement = NULL;
    while (peek_next_token().type != END_OF_FILE) {
        ASTNode* statement = parse_statement();
        if (statement) {
            if (!program_start) program_start = current_statement = statement;
            else { current_statement->next = statement; current_statement = statement; }
        }
    }
    return program_start;
}

char get_expression_type(ASTNode* node) {
    if (!node) return 'i';
    switch (node->node_type) {
        case NUMBER_NODE: return 'i';
        case FLOAT_NODE: return 'f';
        case CHAR_NODE: return 'c';
        case VARIABLE_NODE: { Symbol* var = find_variable(node->token_info.text); return var ? var->type : 'i'; }
        case OPERATION_NODE: {
            char left_type = get_expression_type(node->left_child);
            char right_type = get_expression_type(node->right_child);
            if (left_type == 'f' || right_type == 'f') return 'f';
            return 'i';
        }
        default: return 'i';
    }
}

// --- Semantic Analysis ---

void check_type_compatibility(ASTNode* assignment_node) {
    if (!assignment_node || assignment_node->node_type != ASSIGNMENT_NODE) return;
    Symbol* var = find_variable(assignment_node->token_info.text);
    if (!var || !assignment_node->left_child) return;
    char expr_type = get_expression_type(assignment_node->left_child);
    if (var->type != expr_type) {
        // Warning logic if needed
    }
}

void check_program_semantics(ASTNode* node) {
    if (!node) return;
    switch (node->node_type) {
        case VARIABLE_NODE: {
            Symbol* variable = find_variable(node->token_info.text);
            if (!variable) record_error(node->token_info.line_number, "Variable '%s' was not declared", node->token_info.text);
            break;
        }
        case UNARY_NODE:
            if (node->left_child && node->left_child->node_type == VARIABLE_NODE) {
                Symbol* variable = find_variable(node->left_child->token_info.text);
                if (!variable) record_error(node->token_info.line_number, "Variable '%s' was not declared", node->left_child->token_info.text);
                mark_variable_used(node->left_child->token_info.text);
            }
            break;
        case ASSIGNMENT_NODE: {
            Symbol* variable = find_variable(node->token_info.text);
            if (!variable) record_error(node->token_info.line_number, "Variable '%s' was not declared", node->token_info.text);
            check_type_compatibility(node);
            check_program_semantics(node->left_child);
            break;
        }
        case COMPOUND_ASSIGN_NODE:
            if (node->left_child && node->left_child->node_type == VARIABLE_NODE) {
                Symbol* variable = find_variable(node->left_child->token_info.text);
                if (!variable) record_error(node->token_info.line_number, "Variable '%s' was not declared", node->left_child->token_info.text);
                mark_variable_used(node->left_child->token_info.text);
            }
            check_program_semantics(node->right_child);
            break;
        case OPERATION_NODE:
            check_program_semantics(node->left_child);
            check_program_semantics(node->right_child);
            break;
        case DECLARATION_NODE:
            check_program_semantics(node->left_child);
            break;
        default: break;
    }
    check_program_semantics(node->next);
}

void check_for_unused_variables() {
    for (int i = 0; i < symbols_found; i++) {
        if (!symbol_table[i].is_used) {
            // Optional warning logic
        }
    }
}

// --- Code Generation (MIPS64) ---

void generate_expression_code(ASTNode* node, FILE* output, const char* result_register) {
    if (!node) return;
    char expr_type = get_expression_type(node);
    bool is_float = (expr_type == 'f');

    if (is_float) {
        const char* float_reg = get_float_register();
        switch (node->node_type) {
            case NUMBER_NODE:
                fprintf(output, "    daddiu r1, r0, %s\n", node->token_info.text);
                produce_machine_code("daddiu", 0, 1, -1, atoi(node->token_info.text), output);
                fprintf(output, "    dmtc1 r1, %s\n", float_reg);
                produce_machine_code("dmtc1", 1, get_register_number(float_reg), -1, 0, output);
                fprintf(output, "    cvt.d.l %s, %s\n", float_reg, float_reg);
                produce_machine_code("cvt.d.l", get_register_number(float_reg), get_register_number(float_reg), get_register_number(float_reg), 0, output);
                break;
            case FLOAT_NODE: {
                float float_val = atof(node->token_info.text);
                load_float_constant(float_val, float_reg, output);
                break;
            }
            case CHAR_NODE:
                fprintf(output, "    daddiu r1, r0, %s\n", node->token_info.text);
                produce_machine_code("daddiu", 0, 1, -1, atoi(node->token_info.text), output);
                fprintf(output, "    dmtc1 r1, %s\n", float_reg);
                produce_machine_code("dmtc1", 1, get_register_number(float_reg), -1, 0, output);
                fprintf(output, "    cvt.d.l %s, %s\n", float_reg, float_reg);
                produce_machine_code("cvt.d.l", get_register_number(float_reg), get_register_number(float_reg), get_register_number(float_reg), 0, output);
                break;
            case VARIABLE_NODE: {
                Symbol* variable = find_variable(node->token_info.text);
                if (variable) {
                    if (variable->type == 'f') {
                        fprintf(output, "    l.d %s, %d(r0)\n", float_reg, variable->memory_location);
                        produce_machine_code("l.d", 0, get_register_number(float_reg), -1, variable->memory_location, output);
                    } else {
                        char* int_reg = get_register();
                        fprintf(output, "    lb %s, %d(r0)\n", int_reg, variable->memory_location);
                        produce_machine_code("lb", 0, get_register_number(int_reg), -1, variable->memory_location, output);
                        fprintf(output, "    dmtc1 %s, %s\n", int_reg, float_reg);
                        produce_machine_code("dmtc1", get_register_number(int_reg), get_register_number(float_reg), -1, 0, output);
                        fprintf(output, "    cvt.d.l %s, %s\n", float_reg, float_reg);
                        produce_machine_code("cvt.d.l", get_register_number(float_reg), get_register_number(float_reg), get_register_number(float_reg), 0, output);
                        release_register_by_name(int_reg);
                    }
                }
                break;
            }
            case OPERATION_NODE: {
                const char* left_float_reg = get_float_register();
                const char* right_float_reg = get_float_register();
                generate_expression_code(node->left_child, output, left_float_reg);
                generate_expression_code(node->right_child, output, right_float_reg);
                if (strcmp(node->token_info.text, "+") == 0) {
                    fprintf(output, "    add.d %s, %s, %s\n", float_reg, left_float_reg, right_float_reg);
                    produce_machine_code("add.d", get_register_number(left_float_reg), get_register_number(right_float_reg), get_register_number(float_reg), 0, output);
                } else if (strcmp(node->token_info.text, "-") == 0) {
                    fprintf(output, "    sub.d %s, %s, %s\n", float_reg, left_float_reg, right_float_reg);
                    produce_machine_code("sub.d", get_register_number(left_float_reg), get_register_number(right_float_reg), get_register_number(float_reg), 0, output);
                } else if (strcmp(node->token_info.text, "*") == 0) {
                    fprintf(output, "    mul.d %s, %s, %s\n", float_reg, left_float_reg, right_float_reg);
                    produce_machine_code("mul.d", get_register_number(left_float_reg), get_register_number(right_float_reg), get_register_number(float_reg), 0, output);
                } else if (strcmp(node->token_info.text, "/") == 0) {
                    fprintf(output, "    div.d %s, %s, %s\n", float_reg, left_float_reg, right_float_reg);
                    produce_machine_code("div.d", get_register_number(left_float_reg), get_register_number(right_float_reg), get_register_number(float_reg), 0, output);
                }
                release_float_register(left_float_reg);
                release_float_register(right_float_reg);
                break;
            }
            default: break;
        }
        release_float_register(float_reg);
    } else {
        switch (node->node_type) {
            case NUMBER_NODE:
                fprintf(output, "    daddiu %s, r0, %s\n", result_register, node->token_info.text);
                produce_machine_code("daddiu", 0, get_register_number(result_register), -1, atoi(node->token_info.text), output);
                break;
            case CHAR_NODE:
                fprintf(output, "    daddiu %s, r0, %s\n", result_register, node->token_info.text);
                produce_machine_code("daddiu", 0, get_register_number(result_register), -1, atoi(node->token_info.text), output);
                break;
            case FLOAT_NODE: {
                float float_val = atof(node->token_info.text);
                int int_val = (int)float_val;
                fprintf(output, "    daddiu %s, r0, %d\n", result_register, int_val);
                produce_machine_code("daddiu", 0, get_register_number(result_register), -1, int_val, output);
                break;
            }
            case VARIABLE_NODE: {
                Symbol* variable = find_variable(node->token_info.text);
                if (variable) {
                    fprintf(output, "    lb %s, %d(r0)\n", result_register, variable->memory_location);
                    produce_machine_code("lb", 0, get_register_number(result_register), -1, variable->memory_location, output);
                }
                break;
            }
            case OPERATION_NODE: {
                char* left_register = get_register();
                char* right_register = get_register();
                generate_expression_code(node->left_child, output, left_register);
                generate_expression_code(node->right_child, output, right_register);
                if (strcmp(node->token_info.text, "+") == 0) {
                    fprintf(output, "    daddu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("daddu", get_register_number(left_register), get_register_number(right_register), get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "-") == 0) {
                    fprintf(output, "    dsubu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("dsubu", get_register_number(left_register), get_register_number(right_register), get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "*") == 0) {
                    fprintf(output, "    dmulu %s, %s\n", left_register, right_register);
                    produce_machine_code("dmulu", get_register_number(left_register), get_register_number(right_register), -1, -1, output);
                    fprintf(output, "    mflo %s\n", result_register);
                    produce_machine_code("mflo", -1, -1, get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "/") == 0) {
                    fprintf(output, "    ddivu %s, %s\n", left_register, right_register);
                    produce_machine_code("ddivu", get_register_number(left_register), get_register_number(right_register), -1, -1, output);
                    fprintf(output, "    mflo %s\n", result_register);
                    produce_machine_code("mflo", -1, -1, get_register_number(result_register), -1, output);
                }
                release_register_by_name(left_register);
                release_register_by_name(right_register);
                break;
            }
            default: break;
        }
    }
}

void generate_assignment_code(const char* variable_name, ASTNode* expression, FILE* output) {
    Symbol* variable = find_variable(variable_name);
    if (!variable) return;

    if (variable->type == 'f') {
        const char* float_reg = get_float_register();
        char* temp_reg = get_register();
        generate_expression_code(expression, output, temp_reg);
        fprintf(output, "    dmtc1 %s, %s\n", temp_reg, float_reg);
        produce_machine_code("dmtc1", get_register_number(temp_reg), get_register_number(float_reg), -1, 0, output);
        char expr_type = get_expression_type(expression);
        if (expr_type != 'f') {
            fprintf(output, "    cvt.d.l %s, %s\n", float_reg, float_reg);
            produce_machine_code("cvt.d.l", get_register_number(float_reg), get_register_number(float_reg), get_register_number(float_reg), 0, output);
        }
        fprintf(output, "    s.d %s, %d(r0)\n", float_reg, variable->memory_location);
        produce_machine_code("s.d", 0, get_register_number(float_reg), -1, variable->memory_location, output);
        release_register_by_name(temp_reg);
        release_float_register(float_reg);
    } else {
        char* result_register = get_register();
        generate_expression_code(expression, output, result_register);
        fprintf(output, "    sb %s, %d(r0)\n", result_register, variable->memory_location);
        produce_machine_code("sb", 0, get_register_number(result_register), -1, variable->memory_location, output);
        release_register_by_name(result_register);
    }
}

void generate_assembly_code(ASTNode* node, FILE* output) {
    if (!node) return;
    fprintf(output, ".code\n");
    ASTNode* current = node;

    // First pass: Handle declarations and variable initialization
    while (current) {
        if (current->node_type == DECLARATION_NODE && current->left_child && current->left_child->node_type == VARIABLE_NODE) {
            Symbol* variable = find_variable(current->left_child->token_info.text);
            if (variable && variable->type == 'f') {
                const char* float_reg = get_float_register();
                char* temp_reg = get_register();
                fprintf(output, "    daddiu %s, r0, 0\n", temp_reg);
                produce_machine_code("daddiu", 0, get_register_number(temp_reg), -1, 0, output);
                fprintf(output, "    dmtc1 %s, %s\n", temp_reg, float_reg);
                produce_machine_code("dmtc1", get_register_number(temp_reg), get_register_number(float_reg), -1, 0, output);
                fprintf(output, "    cvt.d.l %s, %s\n", float_reg, float_reg);
                produce_machine_code("cvt.d.l", get_register_number(float_reg), get_register_number(float_reg), get_register_number(float_reg), 0, output);
                fprintf(output, "    s.d %s, %d(r0)\n", float_reg, variable->memory_location);
                produce_machine_code("s.d", 0, get_register_number(float_reg), -1, variable->memory_location, output);
                release_register_by_name(temp_reg);
                release_float_register(float_reg);
            }
        }
        current = current->next;
    }

    // Second pass: Generate code for statements
    current = node;
    while (current) {
        switch (current->node_type) {
            case DECLARATION_NODE:
                if (current->left_child && current->left_child->node_type == ASSIGNMENT_NODE)
                    generate_assignment_code(current->left_child->token_info.text, current->left_child->left_child, output);
                break;
            case ASSIGNMENT_NODE:
                generate_assignment_code(current->token_info.text, current->left_child, output);
                break;
            case COMPOUND_ASSIGN_NODE:
                if (current->left_child && current->left_child->node_type == VARIABLE_NODE) {
                    Symbol* variable = find_variable(current->left_child->token_info.text);
                    if (variable && variable->type == 'f') {
                        const char* float_reg = get_float_register();
                        const char* expr_reg = get_float_register();
                        char* temp_reg = get_register();
                        fprintf(output, "    l.d %s, %d(r0)\n", float_reg, variable->memory_location);
                        produce_machine_code("l.d", 0, get_register_number(float_reg), -1, variable->memory_location, output);
                        generate_expression_code(current->right_child, output, temp_reg);
                        fprintf(output, "    dmtc1 %s, %s\n", temp_reg, expr_reg);
                        produce_machine_code("dmtc1", get_register_number(temp_reg), get_register_number(expr_reg), -1, 0, output);
                        char expr_type = get_expression_type(current->right_child);
                        if (expr_type != 'f') {
                            fprintf(output, "    cvt.d.l %s, %s\n", expr_reg, expr_reg);
                            produce_machine_code("cvt.d.l", get_register_number(expr_reg), get_register_number(expr_reg), get_register_number(expr_reg), 0, output);
                        }
                        if (strcmp(current->token_info.text, "+=") == 0) {
                            fprintf(output, "    add.d %s, %s, %s\n", float_reg, float_reg, expr_reg);
                            produce_machine_code("add.d", get_register_number(float_reg), get_register_number(expr_reg), get_register_number(float_reg), 0, output);
                        } else if (strcmp(current->token_info.text, "-=") == 0) {
                            fprintf(output, "    sub.d %s, %s, %s\n", float_reg, float_reg, expr_reg);
                            produce_machine_code("sub.d", get_register_number(float_reg), get_register_number(expr_reg), get_register_number(float_reg), 0, output);
                        } else if (strcmp(current->token_info.text, "*=") == 0) {
                            fprintf(output, "    mul.d %s, %s, %s\n", float_reg, float_reg, expr_reg);
                            produce_machine_code("mul.d", get_register_number(float_reg), get_register_number(expr_reg), get_register_number(float_reg), 0, output);
                        } else if (strcmp(current->token_info.text, "/=") == 0) {
                            fprintf(output, "    div.d %s, %s, %s\n", float_reg, float_reg, expr_reg);
                            produce_machine_code("div.d", get_register_number(float_reg), get_register_number(expr_reg), get_register_number(float_reg), 0, output);
                        }
                        fprintf(output, "    s.d %s, %d(r0)\n", float_reg, variable->memory_location);
                        produce_machine_code("s.d", 0, get_register_number(float_reg), -1, variable->memory_location, output);
                        release_float_register(float_reg);
                        release_float_register(expr_reg);
                        release_register_by_name(temp_reg);
                    }
                }
                break;
            default: break;
        }
        current = current->next;
    }
}

// --- Main Driver and File I/O ---

void show_generated_code(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("could not open the generated file\n");
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), file)) printf("%s", line);
    fclose(file);
}

void compile_program(const char* source_code, const char* output_filename) {
    current_token_count = 0;
    current_token_position = 0;
    symbols_found = 0;
    next_memory_location = 0;
    error_log.error_count = 0;
    clear_registers();

    break_into_tokens(source_code);

    if (error_log.error_count) {
        printf("\nlexical errors found:\n");
        display_errors();
        return;
    }

    ASTNode* program_structure = parse_program();

    if (error_log.error_count || !program_structure) {
        printf("syntax errors found:\n");
        display_errors();
        free_program_tree(program_structure);
        return;
    }

    check_program_semantics(program_structure);
    check_for_unused_variables();

    if (error_log.error_count) {
        printf("semantic errors found:\n");
        display_errors();
        free_program_tree(program_structure);
        return;
    }

    FILE* output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "cannot create output file: %s\n", output_filename);
        free_program_tree(program_structure);
        return;
    }

    setup_registers();
    generate_assembly_code(program_structure, output_file);
    fclose(output_file);

    show_generated_code(output_filename);
    free_program_tree(program_structure);
}

char* read_source_code() {
    char* code = malloc(50000);
    if (!code) {
        printf("Error: Not enough memory to read source code.\n");
        return NULL;
    }
    code[0] = '\0';
    FILE* source_file = fopen("code.b", "r");
    if (!source_file) source_file = stdin;
    char line[256];
    while (fgets(line, sizeof(line), source_file)) {
        if (strlen(code) + strlen(line) >= 50000) {
            printf("Error: Source code exceeds maximum size (50,000 characters).\n");
            break;
        }
        strcat(code, line);
    }
    if (source_file != stdin) fclose(source_file);
    return code;
}

int main() {
    char* source_code = read_source_code();
    if (!source_code) return 1;
    compile_program(source_code, "output.s");
    free(source_code);
    return 0;
}