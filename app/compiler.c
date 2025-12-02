#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

#define MAX_NAME_LENGTH 32
#define MAX_TOKENS 1000
#define MAX_SYMBOLS 100
#define MAX_ERRORS 100

typedef enum {
    END_OF_FILE, NUMBER, IDENTIFIER, PLUS, MINUS,
    MULTIPLY, DIVIDE, ASSIGN, SEMICOLON, LEFT_PAREN, 
    RIGHT_PAREN, INT_KEYWORD, CHAR_KEYWORD, UNKNOWN_TOKEN,
    INCREMENT, DECREMENT,
    PLUS_ASSIGN, MINUS_ASSIGN,
    MULTIPLY_ASSIGN, DIVIDE_ASSIGN,
    COMMA, CHAR_LITERAL
} TokenType;

typedef enum { 
    PROGRAM_NODE, ASSIGNMENT_NODE, VARIABLE_NODE, 
    NUMBER_NODE, OPERATION_NODE, UNARY_NODE,
    COMPOUND_ASSIGN_NODE, CHAR_NODE, DECLARATION_NODE
} ASTNodeType;

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

Token all_tokens[MAX_TOKENS];
int current_token_count = 0;
int current_token_position = 0;
Symbol symbol_table[MAX_SYMBOLS];
int symbols_found = 0;
int next_memory_location = 0;
ErrorList error_log = {0};
RegisterPool register_pool = {0};

typedef struct {
    const char* instruction_name;
    unsigned int opcode_value;
    unsigned int instruction_format;  
    unsigned int sub_code;
    unsigned int function_code;
} Instruction;

Instruction supported_instructions[] = {
    {"daddiu", 0b011001, 1, 0, 0b000000},
    {"lb",     0b100000, 1, 0, 0b000000},
    {"sb",     0b101000, 1, 0, 0b000000},
    {"daddu",  0b000000, 0, 0b00000, 0b101101},
    {"dsubu",  0b000000, 0, 0b00000, 0b101111},
    {"dmul",   0b000000, 0, 0b00010, 0b011100},
    {"dmulu",  0b000000, 0, 0b00010, 0b011101},
    {"ddiv",   0b000000, 0, 0b00010, 0b011110},
    {"ddivu",  0b000000, 0, 0b00010, 0b011111}
};

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
    if (instruction_index == -1) return 0;
    
    Instruction inst = supported_instructions[instruction_index];
    
    if (inst.instruction_format == 1) {  
        unsigned int immediate_16bit = immediate_value & 0xFFFF;
        return (inst.opcode_value << 26) | (source_reg << 21) | 
               (target_reg << 16) | immediate_16bit;
    } else {  
        return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) |
               (dest_reg << 11) | inst.function_code;
    }
}

void display_binary_code(unsigned int value, FILE* output) {
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
        fprintf(output, "# ");
        display_binary_code(machine_instruction, output);
        fprintf(output, "\n");
    }
}

int get_register_number(const char* register_name) {
    if (register_name[0] == 'r' || register_name[0] == 'R') {
        if (register_name[1] != '\0') {
            int reg_num = atoi(register_name + 1);
            if (reg_num >= 0 && reg_num <= 31) return reg_num;
        }
    }
    return 0;
}

void free_program_tree(ASTNode* node) {
    if (!node) return;
    free_program_tree(node->left_child);
    free_program_tree(node->right_child);
    free_program_tree(node->next);
    free(node);
}

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

const char* get_register() {
    for (int i = 1; i < register_pool.register_count; i++) {
        if (!register_pool.used_registers[i]) {
            register_pool.used_registers[i] = 1;
            return register_pool.available_registers[i];
        }
    }
    return "r31";
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
    return IDENTIFIER;
}

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
                if (source_code[*position] == '\n') {
                    (*current_line)++;
                } else if (source_code[*position] == '/' && source_code[*position + 1] == '*') {
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
            
            if (comment_depth > 0) {
                record_error(*current_line, "Unterminated multi-line comment");
                break;
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
                    default: 
                        char_value = source_code[position]; 
                        record_error(current_line, "Unknown escape sequence '\\%c'", source_code[position]);
                        break;
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
            } else {
                record_error(current_line, "Unterminated character literal");
                while (source_code[position] != '\0' && source_code[position] != '\'' && source_code[position] != '\n') {
                    position++;
                }
                if (source_code[position] == '\'') position++;
            }
            continue;
        }

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
                save_token(NUMBER, number_buffer, current_line);
                continue;
            }
        }

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
                save_token(NUMBER, number_buffer, current_line);
                continue;
            }
        }
        
        if (IS_DIGIT(source_code[position])) {
            char number_buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;
            
            while (IS_DIGIT(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                number_buffer[buffer_index++] = source_code[position++];
            }
            save_token(NUMBER, number_buffer, current_line);
            continue;
        }
        
        if (IS_LETTER(source_code[position])) {
            char word_buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;
            
            while (IS_ALPHANUMERIC(source_code[position]) && buffer_index < MAX_NAME_LENGTH - 1) {
                word_buffer[buffer_index++] = source_code[position++];
            }
            save_token(identify_keyword(word_buffer), word_buffer, current_line);
            continue;
        }
        
        if (source_code[position] == '+' && source_code[position + 1] == '+') {
            save_token(INCREMENT, "++", current_line);
            position += 2;
            continue;
        }
        if (source_code[position] == '-' && source_code[position + 1] == '-') {
            save_token(DECREMENT, "--", current_line);
            position += 2;
            continue;
        }
        if (source_code[position] == '+' && source_code[position + 1] == '=') {
            save_token(PLUS_ASSIGN, "+=", current_line);
            position += 2;
            continue;
        }
        if (source_code[position] == '-' && source_code[position + 1] == '=') {
            save_token(MINUS_ASSIGN, "-=", current_line);
            position += 2;
            continue;
        }
        if (source_code[position] == '*' && source_code[position + 1] == '=') {
            save_token(MULTIPLY_ASSIGN, "*=", current_line);
            position += 2;
            continue;
        }
        if (source_code[position] == '/' && source_code[position + 1] == '=') {
            save_token(DIVIDE_ASSIGN, "/=", current_line);
            position += 2;
            continue;
        }
        
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
            default: 
                char unknown_char[2] = {source_code[position], '\0'};
                save_token(UNKNOWN_TOKEN, unknown_char, current_line);
                record_error(current_line, "Unexpected character '%c'", source_code[position]);
                position++;
                break;
        }
    }
    save_token(END_OF_FILE, "", current_line);
}

Symbol* find_variable(const char* variable_name) {
    for (int i = 0; i < symbols_found; i++) {
        if (strcmp(symbol_table[i].name, variable_name) == 0) {
            return &symbol_table[i];
        }
    }
    return NULL;
}

bool add_variable(const char* variable_name, int line_number) {
    if (symbols_found >= MAX_SYMBOLS) {
        record_error(line_number, "Too many variables declared");
        return false;
    }
    
    if (find_variable(variable_name) != NULL) {
        record_error(line_number, "Variable '%s' is already declared", variable_name);
        return false;
    }
    
    symbol_table[symbols_found] = (Symbol){{0}, 0, 0, next_memory_location, 4};
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
    if (next_token.type != expected_type) {
        record_error(next_token.line_number, "Expected '%s'", expected_text);
        return false;
    }
    get_next_token();
    return true;
}

ASTNode* create_tree_node(ASTNodeType node_type, Token token_data, 
                         ASTNode* left_child, ASTNode* right_child) {
    ASTNode* new_node = malloc(sizeof(ASTNode));
    if (!new_node) {
        record_error(token_data.line_number, "Memory allocation failed");
        return NULL;
    }
    *new_node = (ASTNode){node_type, token_data, left_child, right_child, NULL};
    return new_node;
}

ASTNode* parse_expression();
ASTNode* parse_term();
ASTNode* parse_factor();
ASTNode* parse_unary_expression();
ASTNode* parse_postfix_expression();
ASTNode* parse_primary_expression();
ASTNode* parse_multiplicative_expression();
ASTNode* parse_additive_expression();

ASTNode* parse_unary_expression() {
    Token current_token = peek_next_token();
    
    if (current_token.type == PLUS || current_token.type == MINUS) {
        Token operator_token = get_next_token();
        ASTNode* operand = parse_unary_expression();
        
        if (!operand) {
            record_error(operator_token.line_number, "Expected expression after unary operator");
            return NULL;
        }
        
        return create_tree_node(UNARY_NODE, operator_token, operand, NULL);
    }
    
    if (current_token.type == INCREMENT || current_token.type == DECREMENT) {
        Token operator_token = get_next_token();
        
        ASTNode* operand = parse_primary_expression();
        
        if (!operand) {
            record_error(operator_token.line_number, "Expected variable after prefix operator");
            return NULL;
        }
        
        if (operand->node_type != VARIABLE_NODE) {
            record_error(operator_token.line_number, "Prefix operator requires a variable");
            free_program_tree(operand);
            return NULL;
        }
        
        return create_tree_node(UNARY_NODE, operator_token, operand, NULL);
    }
    
    return parse_postfix_expression();
}

ASTNode* parse_primary_expression() {
    Token current_token = peek_next_token();
    
    if (current_token.type == NUMBER) {
        Token token = get_next_token();
        return create_tree_node(NUMBER_NODE, token, NULL, NULL);
    } else if (current_token.type == CHAR_LITERAL) {
        Token token = get_next_token();
        return create_tree_node(CHAR_NODE, token, NULL, NULL);
    } else if (current_token.type == IDENTIFIER) {
        Token token = get_next_token();
        mark_variable_used(token.text);
        return create_tree_node(VARIABLE_NODE, token, NULL, NULL);
    }
    
    if (current_token.type == LEFT_PAREN) {
        get_next_token();
        ASTNode* expression = parse_expression();
        if (!expression) return NULL;
        
        if (!expect_token(RIGHT_PAREN, ")")) {
            free_program_tree(expression);
            return NULL;
        }
        return expression;
    }
    
    record_error(current_token.line_number, "Expected expression");
    return NULL;
}

ASTNode* parse_postfix_expression() {
    ASTNode* left_side = parse_primary_expression();
    if (!left_side) return NULL;
    
    Token next_token = peek_next_token();
    if ((next_token.type == INCREMENT || next_token.type == DECREMENT) && 
        left_side->node_type == VARIABLE_NODE) {
        Token operator_token = get_next_token();
        
        ASTNode* operation_node = create_tree_node(UNARY_NODE, operator_token, left_side, NULL);
        return operation_node;
    }
    
    return left_side;
}

ASTNode* parse_multiplicative_expression() {
    ASTNode* left_side = parse_unary_expression();
    if (!left_side) return NULL;
    
    while (peek_next_token().type == MULTIPLY || peek_next_token().type == DIVIDE) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_unary_expression();
        
        if (!right_side) {
            free_program_tree(left_side);
            return NULL;
        }
        
        left_side = create_tree_node(OPERATION_NODE, operator_token, left_side, right_side);
        if (!left_side) {
            free_program_tree(right_side);
            return NULL;
        }
    }
    return left_side;
}

ASTNode* parse_additive_expression() {
    ASTNode* left_side = parse_multiplicative_expression();
    if (!left_side) return NULL;
    
    while (peek_next_token().type == PLUS || peek_next_token().type == MINUS) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_multiplicative_expression();
        
        if (!right_side) {
            free_program_tree(left_side);
            return NULL;
        }
        
        left_side = create_tree_node(OPERATION_NODE, operator_token, left_side, right_side);
        if (!left_side) {
            free_program_tree(right_side);
            return NULL;
        }
    }
    return left_side;
}

ASTNode* parse_expression() {
    return parse_additive_expression();
}

ASTNode* parse_assignment() {
    Token variable_token = get_next_token();
    if (variable_token.type != IDENTIFIER) {
        record_error(variable_token.line_number, "Expected variable name");
        return NULL;
    }
    
    Token operator_token = peek_next_token();
    
    if (operator_token.type == PLUS_ASSIGN || operator_token.type == MINUS_ASSIGN ||
        operator_token.type == MULTIPLY_ASSIGN || operator_token.type == DIVIDE_ASSIGN) {
        get_next_token();
        
        ASTNode* expression = parse_expression();
        if (!expression) return NULL;
        
        mark_variable_initialized(variable_token.text);
        return create_tree_node(COMPOUND_ASSIGN_NODE, operator_token, 
                              create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL), 
                              expression);
    }
    
    if (!expect_token(ASSIGN, "=")) return NULL;
    
    Token next_token = peek_next_token();
    if (next_token.type == IDENTIFIER) {
        Token lookahead = (current_token_position + 1 < current_token_count) ? 
                         all_tokens[current_token_position + 1] : all_tokens[current_token_count - 1];
        
        if (lookahead.type == ASSIGN) {
            ASTNode* nested_assignment = parse_assignment();
            if (!nested_assignment) return NULL;
            
            mark_variable_initialized(variable_token.text);
            return create_tree_node(ASSIGNMENT_NODE, variable_token, nested_assignment, NULL);
        }
    }
    
    ASTNode* expression = parse_expression();
    if (!expression) return NULL;
    
    mark_variable_initialized(variable_token.text);
    return create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
}

ASTNode* parse_declaration() {
    Token type_token = get_next_token();
    
    if (type_token.type != INT_KEYWORD && type_token.type != CHAR_KEYWORD) {
        record_error(type_token.line_number, "Expected 'int' or 'char'");
        return NULL;
    }
    
    ASTNode* declaration_list = NULL;
    ASTNode* last_declaration = NULL;
    
    while (1) {
        Token variable_token = get_next_token();
        if (variable_token.type != IDENTIFIER) {
            record_error(variable_token.line_number, "Expected variable name");
            return NULL;
        }
        
        if (!add_variable(variable_token.text, variable_token.line_number)) {
            return NULL;
        }
        
        ASTNode* assignment_node = NULL;
        
        if (peek_next_token().type == ASSIGN) {
            get_next_token();
            ASTNode* expression = parse_expression();
            if (!expression) {
                record_error(variable_token.line_number, "Expected expression after '='");
                return NULL;
            }
            mark_variable_initialized(variable_token.text);
            assignment_node = create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
        } else {
            assignment_node = create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL);
        }
        
        ASTNode* decl_node = create_tree_node(DECLARATION_NODE, type_token, assignment_node, NULL);
        
        if (!declaration_list) {
            declaration_list = decl_node;
        } else {
            last_declaration->next = decl_node;
        }
        last_declaration = decl_node;
        
        if (peek_next_token().type == COMMA) {
            get_next_token();
        } else {
            break;
        }
    }
    
    if (!expect_token(SEMICOLON, ";")) {
        free_program_tree(declaration_list);
        return NULL;
    }
    
    return declaration_list;
}

ASTNode* parse_statement() {
    if (peek_next_token().type == SEMICOLON) {
        get_next_token();
        return NULL;
    }

    if (peek_next_token().type == INT_KEYWORD || peek_next_token().type == CHAR_KEYWORD) 
        return parse_declaration();
    
    if (peek_next_token().type == INCREMENT || peek_next_token().type == DECREMENT) {
        Token op_token = get_next_token();
        
        if (peek_next_token().type != IDENTIFIER) {
            record_error(op_token.line_number, "Expected variable after prefix operator");
            while (peek_next_token().type != SEMICOLON && peek_next_token().type != END_OF_FILE) {
                get_next_token();
            }
            if (peek_next_token().type == SEMICOLON) {
                get_next_token();
            }
            return NULL;
        }
        
        Token var_token = get_next_token();
        mark_variable_used(var_token.text);
        mark_variable_initialized(var_token.text);
        
        ASTNode* node = create_tree_node(UNARY_NODE, op_token, 
                              create_tree_node(VARIABLE_NODE, var_token, NULL, NULL), NULL);
        
        if (!expect_token(SEMICOLON, ";")) {
            free_program_tree(node);
            return NULL;
        }
        return node;
    }
    
    if (peek_next_token().type == IDENTIFIER) {
        Token next_token = peek_next_token();
        Token lookahead = (current_token_position + 1 < current_token_count) ? 
                         all_tokens[current_token_position + 1] : all_tokens[current_token_count - 1];
        
        if (lookahead.type == ASSIGN || lookahead.type == PLUS_ASSIGN || 
            lookahead.type == MINUS_ASSIGN || lookahead.type == MULTIPLY_ASSIGN || 
            lookahead.type == DIVIDE_ASSIGN) {
            ASTNode* assignment = parse_assignment();
            if (assignment && !expect_token(SEMICOLON, ";")) {
                free_program_tree(assignment);
                return NULL;
            }
            return assignment;
        } else if (lookahead.type == INCREMENT || lookahead.type == DECREMENT) {
            Token var_token = get_next_token();
            Token op_token = get_next_token();
            
            mark_variable_used(var_token.text);
            mark_variable_initialized(var_token.text);
            
            ASTNode* node = create_tree_node(UNARY_NODE, op_token, 
                                  create_tree_node(VARIABLE_NODE, var_token, NULL, NULL), NULL);
            
            if (!expect_token(SEMICOLON, ";")) {
                free_program_tree(node);
                return NULL;
            }
            return node;
        } else {
            ASTNode* expr = parse_expression();
            if (expr) {
                if (!expect_token(SEMICOLON, ";")) {
                    free_program_tree(expr);
                    return NULL;
                }
                return expr;
            }
        }
    }
    
    ASTNode* expr = parse_expression();
    if (expr) {
        if (!expect_token(SEMICOLON, ";")) {
            free_program_tree(expr);
            return NULL;
        }
        return expr;
    }
    
    Token error_token = peek_next_token();
    if (error_token.type != END_OF_FILE) {
        record_error(error_token.line_number, "Invalid statement starting with '%s'", error_token.text);
        
        while (peek_next_token().type != SEMICOLON && peek_next_token().type != END_OF_FILE) {
            get_next_token();
        }
        if (peek_next_token().type == SEMICOLON) {
            get_next_token();
        }
    }
    
    return NULL;
}

ASTNode* parse_program() {
    ASTNode *program_start = NULL;
    ASTNode *current_statement = NULL;
    
    while (peek_next_token().type != END_OF_FILE) {
        ASTNode* statement = parse_statement();
        if (statement) {
            if (!program_start) {
                program_start = current_statement = statement;
            } else {
                current_statement->next = statement;
                current_statement = statement;
            }
        }
    }
    return program_start;
}

void check_program_semantics(ASTNode* node) {
    if (!node) return;
    
    switch (node->node_type) {
        case VARIABLE_NODE: 
            {
                Symbol* variable = find_variable(node->token_info.text);
                if (!variable) {
                    record_error(node->token_info.line_number, 
                               "Variable '%s' was not declared", node->token_info.text);
                } else if (!variable->is_initialized) {
                    fprintf(stderr, "Warning at line %d: Variable '%s' might not have a value\n", 
                           node->token_info.line_number, node->token_info.text);
                }
            }
            break;
        
        case UNARY_NODE:
            if (node->left_child && node->left_child->node_type == VARIABLE_NODE) {
                Symbol* variable = find_variable(node->left_child->token_info.text);
                if (!variable) {
                    record_error(node->token_info.line_number, 
                               "Variable '%s' was not declared", node->left_child->token_info.text);
                } else if (!variable->is_initialized) {
                    fprintf(stderr, "Warning at line %d: Variable '%s' might not have a value\n", 
                           node->token_info.line_number, node->left_child->token_info.text);
                }
                mark_variable_used(node->left_child->token_info.text);
            }
            break;
            
        case ASSIGNMENT_NODE: 
            {
                Symbol* variable = find_variable(node->token_info.text);
                if (!variable) {
                    record_error(node->token_info.line_number, 
                               "Variable '%s' was not declared", node->token_info.text);
                }
                check_program_semantics(node->left_child);
            }
            break;
            
        case COMPOUND_ASSIGN_NODE:
            if (node->left_child && node->left_child->node_type == VARIABLE_NODE) {
                Symbol* variable = find_variable(node->left_child->token_info.text);
                if (!variable) {
                    record_error(node->token_info.line_number, 
                               "Variable '%s' was not declared", node->left_child->token_info.text);
                }
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
            
        default:
            break;
    }
    
    check_program_semantics(node->next);
}

void check_for_unused_variables() {
    for (int i = 0; i < symbols_found; i++) {
        if (!symbol_table[i].is_used) {
            fprintf(stderr, "Warning: Variable '%s' was declared but never used\n", 
                   symbol_table[i].name);
        }
    }
}

void generate_expression_code(ASTNode* node, FILE* output, const char* result_register) {
    if (!node) return;
    
    switch (node->node_type) {
        case NUMBER_NODE:
            fprintf(output, "    daddiu %s, r0, %s\n", result_register, node->token_info.text);
            produce_machine_code("daddiu", 0, get_register_number(result_register), 
                               -1, atoi(node->token_info.text), output);
            break;
            
        case CHAR_NODE:
            fprintf(output, "    daddiu %s, r0, %s\n", result_register, node->token_info.text);
            produce_machine_code("daddiu", 0, get_register_number(result_register), 
                               -1, atoi(node->token_info.text), output);
            break;
            
        case VARIABLE_NODE: 
            {
                Symbol* variable = find_variable(node->token_info.text);
                if (variable) {
                    fprintf(output, "    lb %s, %d(r0)\n", result_register, variable->memory_location);
                    produce_machine_code("lb", 0, get_register_number(result_register), 
                                       -1, variable->memory_location, output);
                } else {
                    // fprintf(output, "    # error: variable %s not found\n", node->token_info.text);
                }
            }
            break;
        
        case UNARY_NODE:
            {
                if (strcmp(node->token_info.text, "+") == 0 || strcmp(node->token_info.text, "-") == 0) {
                    generate_expression_code(node->left_child, output, result_register);
                    
                    if (strcmp(node->token_info.text, "-") == 0) {
                        fprintf(output, "    dsubu %s, r0, %s\n", result_register, result_register);
                        produce_machine_code("dsubu", 0, get_register_number(result_register), 
                                           get_register_number(result_register), -1, output);
                    }
                }
                else if (node->left_child && node->left_child->node_type == VARIABLE_NODE) {
                    const char* var_name = node->left_child->token_info.text;
                    Symbol* variable = find_variable(var_name);









                    if (variable) {
                        const char* temp_reg = get_register();
                        
                        fprintf(output, "    lb %s, %d(r0)\n", result_register, variable->memory_location);
                        produce_machine_code("lb", 0, get_register_number(result_register), 
                                           -1, variable->memory_location, output);
                        
                        fprintf(output, "    lb %s, %d(r0)\n", temp_reg, variable->memory_location);
                        produce_machine_code("lb", 0, get_register_number(temp_reg), 
                                           -1, variable->memory_location, output);
                        
                        if (strcmp(node->token_info.text, "++") == 0) {
                            fprintf(output, "    daddiu %s, %s, 1\n", temp_reg, temp_reg);
                            produce_machine_code("daddiu", get_register_number(temp_reg), 
                                               get_register_number(temp_reg), -1, 1, output);
                        } else if (strcmp(node->token_info.text, "--") == 0) {
                            fprintf(output, "    daddiu %s, %s, -1\n", temp_reg, temp_reg);
                            produce_machine_code("daddiu", get_register_number(temp_reg), 
                                               get_register_number(temp_reg), -1, -1, output);
                        }
                        
                        fprintf(output, "    sb %s, %d(r0)\n", temp_reg, variable->memory_location);
                        produce_machine_code("sb", 0, get_register_number(temp_reg), 
                                           -1, variable->memory_location, output);
                        
                        release_register_by_name(temp_reg);
                    }
                }
            }
            break;
            
        case OPERATION_NODE: 
            {
                const char* left_register = get_register();
                const char* right_register = get_register();
                
                generate_expression_code(node->left_child, output, left_register);
                generate_expression_code(node->right_child, output, right_register);
                
                if (strcmp(node->token_info.text, "+") == 0) {
                    fprintf(output, "    daddu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("daddu", get_register_number(left_register), 
                                       get_register_number(right_register), 
                                       get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "-") == 0) {
                    fprintf(output, "    dsubu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("dsubu", get_register_number(left_register), 
                                       get_register_number(right_register), 
                                       get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "*") == 0) {
                    fprintf(output, "    dmulu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("dmulu", get_register_number(left_register), 
                                       get_register_number(right_register), 
                                       get_register_number(result_register), -1, output);
                } else if (strcmp(node->token_info.text, "/") == 0) {
                    fprintf(output, "    ddivu %s, %s, %s\n", result_register, left_register, right_register);
                    produce_machine_code("ddivu", get_register_number(left_register), 
                                       get_register_number(right_register), 
                                       get_register_number(result_register), -1, output);
                }
                
                release_register_by_name(left_register);
                release_register_by_name(right_register);
            }
            break;
        
        default:
            break;
    }
}

void generate_unary_operation_code(ASTNode* node, FILE* output) {
    if (!node || !node->left_child) return;
    
    if (node->left_child->node_type == VARIABLE_NODE) {
        const char* variable_name = node->left_child->token_info.text;
        Symbol* variable = find_variable(variable_name);
        if (!variable) return;
        
        const char* temp_reg = get_register();
        
        fprintf(output, "    lb %s, %d(r0)\n", temp_reg, variable->memory_location);
        produce_machine_code("lb", 0, get_register_number(temp_reg), 
                           -1, variable->memory_location, output);
        
        if (strcmp(node->token_info.text, "++") == 0) {
            fprintf(output, "    daddiu %s, %s, 1\n", temp_reg, temp_reg);
            produce_machine_code("daddiu", get_register_number(temp_reg), 
                               get_register_number(temp_reg), -1, 1, output);
        } else if (strcmp(node->token_info.text, "--") == 0) {
            fprintf(output, "    daddiu %s, %s, -1\n", temp_reg, temp_reg);
            produce_machine_code("daddiu", get_register_number(temp_reg), 
                               get_register_number(temp_reg), -1, -1, output);
        } else if (strcmp(node->token_info.text, "+") == 0) {
        } else if (strcmp(node->token_info.text, "-") == 0) {
            fprintf(output, "    dsubu %s, r0, %s\n", temp_reg, temp_reg);
            produce_machine_code("dsubu", 0, get_register_number(temp_reg), 
                               get_register_number(temp_reg), -1, output);
        }
        
        fprintf(output, "    sb %s, %d(r0)\n", temp_reg, variable->memory_location);
        produce_machine_code("sb", 0, get_register_number(temp_reg), 
                           -1, variable->memory_location, output);
        
        release_register_by_name(temp_reg);
    }
}

void generate_compound_assignment_code(const char* variable_name, ASTNode* expression, 
                                      const char* operator, FILE* output) {
    Symbol* variable = find_variable(variable_name);
    if (!variable) return;
    
    const char* result_reg = get_register();
    const char* temp_reg = get_register();
    
    fprintf(output, "    lb %s, %d(r0)\n", temp_reg, variable->memory_location);
    produce_machine_code("lb", 0, get_register_number(temp_reg), 
                       -1, variable->memory_location, output);
    
    generate_expression_code(expression, output, result_reg);
    
    if (strcmp(operator, "+=") == 0) {
        fprintf(output, "    daddu %s, %s, %s\n", temp_reg, temp_reg, result_reg);
        produce_machine_code("daddu", get_register_number(temp_reg), 
                           get_register_number(result_reg), 
                           get_register_number(temp_reg), -1, output);
    } else if (strcmp(operator, "-=") == 0) {
        fprintf(output, "    dsubu %s, %s, %s\n", temp_reg, temp_reg, result_reg);
        produce_machine_code("dsubu", get_register_number(temp_reg), 
                           get_register_number(result_reg), 
                           get_register_number(temp_reg), -1, output);
    } else if (strcmp(operator, "*=") == 0) {
        fprintf(output, "    dmulu %s, %s, %s\n", temp_reg, temp_reg, result_reg);
        produce_machine_code("dmulu", get_register_number(temp_reg), 
                           get_register_number(result_reg), 
                           get_register_number(temp_reg), -1, output);
    } else if (strcmp(operator, "/=") == 0) {
        fprintf(output, "    ddivu %s, %s, %s\n", temp_reg, temp_reg, result_reg);
        produce_machine_code("ddivu", get_register_number(temp_reg), 
                           get_register_number(result_reg), 
                           get_register_number(temp_reg), -1, output);
    }
    
    fprintf(output, "    sb %s, %d(r0)\n", temp_reg, variable->memory_location);
    produce_machine_code("sb", 0, get_register_number(temp_reg), 
                       -1, variable->memory_location, output);
    
    release_register_by_name(result_reg);
    release_register_by_name(temp_reg);
}

void generate_assignment_code(const char* variable_name, ASTNode* expression, FILE* output) {
    Symbol* variable = find_variable(variable_name);
    if (!variable) {
        fprintf(output, "    # error: variable %s not found\n", variable_name);
        return;
    }
    
    const char* result_register = get_register();
    
    generate_expression_code(expression, output, result_register);
    
    fprintf(output, "    sb %s, %d(r0)\n", result_register, variable->memory_location);
    produce_machine_code("sb", 0, get_register_number(result_register), 
                        -1, variable->memory_location, output);
    
    release_register_by_name(result_register);
}

void generate_assembly_code(ASTNode* node, FILE* output) {
    if (!node) return;
    
    static int code_section_emitted = 0;
    if (!code_section_emitted) {
        fprintf(output, ".code\n");
        code_section_emitted = 1;
    }
    
    ASTNode* current = node;
    while (current) {
        if (current->node_type == DECLARATION_NODE) {
            if (current->left_child) {
                if (current->left_child->node_type == VARIABLE_NODE) {
                    Symbol* variable = find_variable(current->left_child->token_info.text);
                    if (variable) {
                        fprintf(output, "    sb r0, %d(r0)\n", variable->memory_location);
                        produce_machine_code("sb", 0, 0, -1, variable->memory_location, output);
                    }
                }
            }
        }
        current = current->next;
    }
    
    current = node;
    while (current) {
        switch (current->node_type) {
            case DECLARATION_NODE:
                if (current->left_child) {
                    if (current->left_child->node_type == ASSIGNMENT_NODE) {
                        generate_assignment_code(current->left_child->token_info.text, 
                                                current->left_child->left_child, output);
                    }
                }
                break;
                
            case ASSIGNMENT_NODE:
                generate_assignment_code(current->token_info.text, current->left_child, output);
                break;
                
            case COMPOUND_ASSIGN_NODE:
                if (current->left_child && current->left_child->node_type == VARIABLE_NODE) {
                    generate_compound_assignment_code(current->left_child->token_info.text, 
                                                    current->right_child, current->token_info.text, output);
                }
                break;
                
            case UNARY_NODE:
                generate_unary_operation_code(current, output);
                break;
                
            default:
                break;
        }
        
        current = current->next;
    }
}

void show_generated_code(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("could not open the generated file\n");
        return;
    }
    
    char line[256];
    printf("\ngenerated assembly and machine code:\n");
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    fclose(file);
}

const char* get_token_type_name(TokenType type) {
    switch (type) {
        case END_OF_FILE: return "END_OF_FILE";
        case NUMBER: return "NUMBER";
        case IDENTIFIER: return "IDENTIFIER";
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        case MULTIPLY: return "MULTIPLY";
        case DIVIDE: return "DIVIDE";
        case ASSIGN: return "ASSIGN";
        case SEMICOLON: return "SEMICOLON";
        case LEFT_PAREN: return "LEFT_PAREN";
        case RIGHT_PAREN: return "RIGHT_PAREN";
        case INT_KEYWORD: return "INT_KEYWORD";
        case CHAR_KEYWORD: return "CHAR_KEYWORD";
        case UNKNOWN_TOKEN: return "UNKNOWN_TOKEN";
        case INCREMENT: return "INCREMENT";
        case DECREMENT: return "DECREMENT";
        case PLUS_ASSIGN: return "PLUS_ASSIGN";
        case MINUS_ASSIGN: return "MINUS_ASSIGN";
        case MULTIPLY_ASSIGN: return "MULTIPLY_ASSIGN";
        case DIVIDE_ASSIGN: return "DIVIDE_ASSIGN";        
        case COMMA: return "COMMA";  
        case CHAR_LITERAL: return "CHAR_LITERAL";
        default: return "UNKNOWN";
    }
}

const char* get_node_type_name(ASTNodeType type) {
    switch (type) {
        case PROGRAM_NODE: return "PROGRAM";
        case ASSIGNMENT_NODE: return "ASSIGNMENT";
        case VARIABLE_NODE: return "VARIABLE";
        case NUMBER_NODE: return "NUMBER";
        case OPERATION_NODE: return "BINARY_OP";
        case UNARY_NODE: return "UNARY_OP";
        case COMPOUND_ASSIGN_NODE: return "COMPOUND_ASSIGN";
        case CHAR_NODE: return "CHAR";
        case DECLARATION_NODE: return "DECLARATION";
        default: return "UNKNOWN";
    }
}

void display_program_structure(ASTNode *node, int depth) {
    if (node == NULL) return;

    for (int i = 0; i < depth; i++) printf("  ");

    printf("node type: %-12s | token: %-12s | value: %-8s | line: %d\n",
           get_node_type_name(node->node_type),
           get_token_type_name(node->token_info.type),
           node->token_info.text,
           node->token_info.line_number);

    display_program_structure(node->left_child, depth + 1);
    display_program_structure(node->right_child, depth + 1);
    
    if (node->next) {
        for (int i = 0; i < depth; i++) printf("  ");
        printf("next statement:\n");
        display_program_structure(node->next, depth + 1);
    }
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
    
    printf("compilation successful! output file: %s\n", output_filename);
    show_generated_code(output_filename);
    free_program_tree(program_structure);
}

int main(int argc, char *argv[]) {
    printf("submitted by kian and charls\n");
    
    if (argc > 1) {
        // Use the first command line argument as source code
        const char* source_code = argv[1];
        printf("source code:\n%s\n\n", source_code);
        compile_program(source_code, "output.s");
    } else {
        printf("No input received.\n");
        printf("Usage: %s \"source_code_here\"\n", argv[0]);
        printf("Example: %s \"int x = 5; x++;\"\n", argv[0]);
        return 1;
    }
    
    return 0;
}