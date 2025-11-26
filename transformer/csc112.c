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
    RIGHT_PAREN, INT_KEYWORD, UNKNOWN_TOKEN
} TokenType;

typedef enum { 
    PROGRAM_NODE, ASSIGNMENT_NODE, VARIABLE_NODE, 
    NUMBER_NODE, OPERATION_NODE 
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
} Symbol;

typedef struct ASTNode {
    ASTNodeType node_type;
    Token token_info;
    struct ASTNode *left_child;
    struct ASTNode *right_child;
} ASTNode;

typedef struct {
    int error_count;
    char error_messages[MAX_ERRORS][256];
} ErrorList;

typedef struct {
    const char* available_registers[32];
    int next_register_index;
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
    unsigned int function_code; 
} Instruction;

Instruction supported_instructions[] = {
    {"daddiu", 0b011001, 1, 0b000000},
    {"daddu",   0b000000, 0, 0b101101},
    {"dsubu",   0b000000, 0, 0b101111},
    {"dmult",  0b000000, 0, 0b011100},
    {"ddiv",   0b000000, 0, 0b011110},
    {"dmul",   0b011100, 0, 0b000010},
    {"mflo",   0b000000, 0, 0b010010},
    {"mfhi",   0b000000, 0, 0b010000},
    {"lb",     0b100000, 1, 0b000000},
    {"sb",     0b101000, 1, 0b000000},
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
    
    if (inst.instruction_format == 0) {
        if (strcmp(instruction_name, "mflo") == 0 || strcmp(instruction_name, "mfhi") == 0) {
            return (inst.opcode_value << 26) | (dest_reg << 11) | inst.function_code;
        } else if (strcmp(instruction_name, "dmult") == 0 || strcmp(instruction_name, "ddiv") == 0) {
            return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) | inst.function_code;
        } else {
            return (inst.opcode_value << 26) | (source_reg << 21) | (target_reg << 16) | 
                   (dest_reg << 11) | inst.function_code;
        }
    } else {
        return (inst.opcode_value << 26) | (source_reg << 21) | 
               (target_reg << 16) | (immediate_value & 0xFFFF);
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
            return atoi(register_name + 1);
        }
    }
    return 0;
}

void free_program_tree(ASTNode* node) {
    if (!node) return;
    free_program_tree(node->left_child);
    free_program_tree(node->right_child);
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
        "r1","r2","r3","r4","r5","r6","r7","r8",
        "r9","r10","r11","r12","r13","r14","r15","r16",
        "r17","r18","r19","r20","r21","r22","r23","r24",
        "r25","r26","r27","r28","r29","r30","r31"
    };
    
    for (int i = 0; i < 16; i++) {
        register_pool.available_registers[i] = register_names[i];
    }
    for (int i = 16; i < 32; i++) {
        register_pool.available_registers[i] = "r16";
    }
    register_pool.next_register_index = 0;
}

const char* get_register() {
    return (register_pool.next_register_index < 32) ? 
           register_pool.available_registers[register_pool.next_register_index++] : "r1";
}

void release_register() {
    if (register_pool.next_register_index > 0) register_pool.next_register_index--;
}

void clear_registers() {
    register_pool.next_register_index = 0;
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
    return strcmp(word, "int") == 0 ? INT_KEYWORD : IDENTIFIER;
}

void save_token(TokenType type, const char* text_value, int line_number) {
    if (current_token_count < MAX_TOKENS) {
        all_tokens[current_token_count] = (Token){type, "", line_number};
        strncpy(all_tokens[current_token_count].text, text_value, MAX_NAME_LENGTH - 1);
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
            while (source_code[*position] != '\0') {
                if (source_code[*position] == '\n') {
                    (*current_line)++;
                } else if (source_code[*position] == '*' && source_code[*position + 1] == '/') {
                    *position += 2; 
                    break;
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
        
        switch (source_code[position]) {
            case '+': save_token(PLUS, "+", current_line); break;
            case '-': save_token(MINUS, "-", current_line); break;
            case '*': save_token(MULTIPLY, "*", current_line); break;
            case '/': save_token(DIVIDE, "/", current_line); break;
            case '=': save_token(ASSIGN, "=", current_line); break;
            case ';': save_token(SEMICOLON, ";", current_line); break;
            case '(': save_token(LEFT_PAREN, "(", current_line); break;
            case ')': save_token(RIGHT_PAREN, ")", current_line); break;
            default: 
                record_error(current_line, "Unexpected character '%c'", source_code[position]);
                break;
        }
        position++;
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
    
    symbol_table[symbols_found] = (Symbol){{0}, 0, 0, next_memory_location};
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
    *new_node = (ASTNode){node_type, token_data, left_child, right_child};
    return new_node;
}

ASTNode* parse_expression();
ASTNode* parse_term();
ASTNode* parse_factor();

ASTNode* parse_factor() {
    Token current_token = get_next_token();
    
    if (current_token.type == NUMBER) {
        return create_tree_node(NUMBER_NODE, current_token, NULL, NULL);
    }
    
    if (current_token.type == IDENTIFIER) {
        mark_variable_used(current_token.text);
        return create_tree_node(VARIABLE_NODE, current_token, NULL, NULL);
    }
    
    if (current_token.type == LEFT_PAREN) {
        ASTNode* expression = parse_expression();
        return (expression && expect_token(RIGHT_PAREN, ")")) ? expression : 
               (free_program_tree(expression), NULL);
    }
    
    record_error(current_token.line_number, "Unexpected token in expression");
    return NULL;
}

ASTNode* parse_term() {
    ASTNode* left_side = parse_factor();
    if (!left_side) return NULL;
    
    while (peek_next_token().type == MULTIPLY || peek_next_token().type == DIVIDE) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_factor();
        
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
    ASTNode* left_side = parse_term();
    if (!left_side) return NULL;
    
    while (peek_next_token().type == PLUS || peek_next_token().type == MINUS) {
        Token operator_token = get_next_token();
        ASTNode* right_side = parse_term();
        
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

ASTNode* parse_assignment() {
    Token variable_token = get_next_token();
    if (variable_token.type != IDENTIFIER) {
        record_error(variable_token.line_number, "Expected variable name");
        return NULL;
    }
    
    if (!expect_token(ASSIGN, "=")) return NULL;
    
    ASTNode* expression = parse_expression();
    if (!expression) return NULL;
    
    if (!expect_token(SEMICOLON, ";")) {
        free_program_tree(expression);
        return NULL;
    }
    
    mark_variable_initialized(variable_token.text);
    return create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
}

ASTNode* parse_declaration() {
    if (!expect_token(INT_KEYWORD, "int")) return NULL;
    
    Token variable_token = get_next_token();
    if (variable_token.type != IDENTIFIER) {
        record_error(variable_token.line_number, "Expected variable name");
        return NULL;
    }
    
    if (!add_variable(variable_token.text, variable_token.line_number)) {
        return NULL;
    }
    
    if (peek_next_token().type == ASSIGN) {
        get_next_token();
        ASTNode* expression = parse_expression();
        if (!expression) {
            return create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL);
        }
        if (!expect_token(SEMICOLON, ";")) {
            free_program_tree(expression);
            return create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL);
        }
        mark_variable_initialized(variable_token.text);
        return create_tree_node(ASSIGNMENT_NODE, variable_token, expression, NULL);
    }
    
    return expect_token(SEMICOLON, ";") ? 
           create_tree_node(VARIABLE_NODE, variable_token, NULL, NULL) : NULL;
}

ASTNode* parse_statement() {
    if (peek_next_token().type == INT_KEYWORD) return parse_declaration();
    
    if (peek_next_token().type == IDENTIFIER && 
        all_tokens[current_token_position + 1].type == ASSIGN) {
        return parse_assignment();
    }
    
    record_error(peek_next_token().line_number, "Invalid statement");
    
    while (peek_next_token().type != SEMICOLON && peek_next_token().type != END_OF_FILE) {
        get_next_token();
    }
    if (peek_next_token().type == SEMICOLON) get_next_token();
    
    return NULL;
}

ASTNode* parse_program() {
    ASTNode *program_start = NULL;
    ASTNode *current_statement = NULL;
    
    while (peek_next_token().type != END_OF_FILE) {
        ASTNode* statement = parse_statement();
        if (statement) {
            if (!program_start) {
                program_start = current_statement = create_tree_node(PROGRAM_NODE, all_tokens[0], statement, NULL);
            } else {
                current_statement = current_statement->right_child = 
                    create_tree_node(PROGRAM_NODE, all_tokens[0], statement, NULL);
            }
        }
    }
    return program_start;
}

void check_program_semantics(ASTNode* node) {
    if (!node) return;
    switch (node->node_type) {
        case VARIABLE_NODE: 
            Symbol* variable = find_variable(node->token_info.text);
            if (!variable) {
                record_error(node->token_info.line_number, 
                           "Variable '%s' was not declared", node->token_info.text);
            } else if (!variable->is_initialized) {
                fprintf(stderr, "Warning at line %d: Variable '%s' might not have a value\n", 
                       node->token_info.line_number, node->token_info.text);
            }
            break;
        
            
        case ASSIGNMENT_NODE: 
            if (!find_variable(node->token_info.text)) {
                record_error(node->token_info.line_number, 
                           "Variable '%s' was not declared", node->token_info.text);
            }
            check_program_semantics(node->left_child);
            break;
        
            
        case OPERATION_NODE:
            check_program_semantics(node->left_child);
            check_program_semantics(node->right_child);
            break;
            
        case PROGRAM_NODE:
            check_program_semantics(node->left_child);
            check_program_semantics(node->right_child);
            break;
            
        default:
            break;
    }
}

void check_for_unused_variables() {
    for (int i = 0; i < symbols_found; i++) {
        if (!symbol_table[i].is_used) {
            fprintf(stderr, "Warning: Variable '%s' was declared but never used\n", 
                   symbol_table[i].name);
        }
    }
}

void generate_declaration_code(const char* variable_name, FILE* output) {
    Symbol* variable = find_variable(variable_name);
    if (!variable) return;
    
    fprintf(output, "    sb r0, %s(r0)\n", variable->name);
    produce_machine_code("sb", 0, 0, -1, variable->memory_location, output);
}

void generate_expression_code(ASTNode* node, FILE* output, const char* result_register) {
    if (!node) return;
    
    switch (node->node_type) {
        case NUMBER_NODE:
            fprintf(output, "    daddiu %s, r0, %s\n", result_register, node->token_info.text);
            produce_machine_code("daddiu", 0, get_register_number(result_register), 
                               -1, atoi(node->token_info.text), output);
            break;
            
        case VARIABLE_NODE: 
            Symbol* variable = find_variable(node->token_info.text);
            if (variable) {
                fprintf(output, "    lb %s, %s(r0)\n", result_register, variable->name);
                produce_machine_code("lb", 0, get_register_number(result_register), 
                                   -1, variable->memory_location, output);
            }
            break;
        
            
        case OPERATION_NODE: 
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
                fprintf(output, "    dmult %s, %s\n", left_register, right_register);
                produce_machine_code("dmult", get_register_number(left_register), 
                                   get_register_number(right_register), -1, -1, output);
                fprintf(output, "    mflo %s\n", result_register);
                produce_machine_code("mflo", -1, -1, get_register_number(result_register), -1, output);
            } else if (strcmp(node->token_info.text, "/") == 0) {
                fprintf(output, "    ddiv %s, %s\n", left_register, right_register);
                produce_machine_code("ddiv", get_register_number(left_register), 
                                   get_register_number(right_register), -1, -1, output);
                fprintf(output, "    mflo %s\n", result_register);
                produce_machine_code("mflo", -1, -1, get_register_number(result_register), -1, output);
            }
            
            release_register();
            release_register();
            break;
        
        default:
            break;
    }
}

void generate_assignment_code(const char* variable_name, ASTNode* expression, FILE* output) {
    Symbol* variable = find_variable(variable_name);
    if (!variable) return;
    
    clear_registers();

    switch (expression->node_type) {
        case NUMBER_NODE:
            const char* temp_register = get_register();
                fprintf(output, "    daddiu %s, r0, %s\n", temp_register, expression->token_info.text);
                produce_machine_code("daddiu", 0, get_register_number(temp_register), 
                                -1, atoi(expression->token_info.text), output);
                fprintf(output, "    sb %s, %s(r0)\n", temp_register, variable->name);
                produce_machine_code("sb", 0, get_register_number(temp_register), 
                                -1, variable->memory_location, output);
                release_register();
            break;
        
        case VARIABLE_NODE:
            Symbol* source_variable = find_variable(expression->token_info.text);
            if (source_variable) {
                const char* temp_register = get_register();
                fprintf(output, "    lb %s, %s(r0)\n", temp_register, source_variable->name);
                produce_machine_code("lb", 0, get_register_number(temp_register), 
                                -1, source_variable->memory_location, output);
                fprintf(output, "    sb %s, %s(r0)\n", temp_register, variable->name);
                produce_machine_code("sb", 0, get_register_number(temp_register), 
                                -1, variable->memory_location, output);
                release_register();
            }
            break;

        default:
            const char* result_register = get_register();
            generate_expression_code(expression, output, result_register);
            fprintf(output, "    sb %s, %s(r0)\n", result_register, variable->name);
            produce_machine_code("sb", 0, get_register_number(result_register), 
                            -1, variable->memory_location, output);
            release_register();
            break;
    }
    
   
}

void generate_assembly_code(ASTNode* node, FILE* output) {
    if (!node) return;
    
    switch (node->node_type) {
        case PROGRAM_NODE:
            fprintf(output, "\n.code\n");
            ASTNode* current = node;
            while (current) {
                if (current->left_child) {
                    generate_assembly_code(current->left_child, output);
                }
                current = current->right_child;
            }
            break;
            
        case ASSIGNMENT_NODE:
            generate_assignment_code(node->token_info.text, node->left_child, output);
            break;
            
        case VARIABLE_NODE:
            generate_declaration_code(node->token_info.text, output);
            break;
            
        default:
            generate_assembly_code(node->left_child, output);
            generate_assembly_code(node->right_child, output);
            break;
    }
}

void show_generated_code(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Could not open the generated file\n");
        return;
    }
    
    char line[256];
    printf("\nGenerated Assembly and Machine Code:\n");
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line);
    }
    fclose(file);
}

const char* get_token_type_name(TokenType type) {
    switch (type) {
        case NUMBER: return "NUMBER";
        case IDENTIFIER: return "IDENTIFIER";
        case PLUS: return "PLUS";
        case MINUS: return "MINUS";
        case MULTIPLY: return "MULTIPLY";
        case DIVIDE: return "DIVIDE";
        case ASSIGN: return "ASSIGN";
        case INT_KEYWORD: return "INT";
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
        default: return "UNKNOWN";
    }
}

void display_program_structure(ASTNode *node, int depth) {
    if (node == NULL) return;

    for (int i = 0; i < depth; i++) printf("  ");

    printf("Node Type: %-12s | Token: %-12s | Value: %-8s | Line: %d\n",
           get_node_type_name(node->node_type),
           get_token_type_name(node->token_info.type),
           node->token_info.text,
           node->token_info.line_number);

    display_program_structure(node->left_child, depth + 1);
    display_program_structure(node->right_child, depth + 1);
}

void print_indent(int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");   
    }
}

void print_ast(ASTNode *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->node_type) {
        case PROGRAM_NODE:    printf("PROGRAM\n"); break;
        case ASSIGNMENT_NODE: printf("ASSIGNMENT (%s)\n", node->token_info.text); break;
        case VARIABLE_NODE:   printf("VARIABLE (%s)\n", node->token_info.text); break;
        case NUMBER_NODE:     printf("NUMBER (%s)\n", node->token_info.text); break;
        case OPERATION_NODE:  printf("OPERATION (%s)\n", node->token_info.text); break;
        default:              printf("UNKNOWN\n"); break;
    }

    print_ast(node->left_child,  indent + 1);
    print_ast(node->right_child, indent + 1);
}



void compile_program(const char* source_code, const char* output_filename) {
    break_into_tokens(source_code);
    if (error_log.error_count) {
        printf("\nLexical errors found:\n");
        display_errors();
        return;
    }
/*    
    printf("Tokens found in program:\n");
    for (int i = 0; i < current_token_count; i++) {
        printf("Token: type=%s, value='%s', line=%d\n", 
               get_token_type_name(all_tokens[i].type), 
               all_tokens[i].text, all_tokens[i].line_number);
    }
*/    
    ASTNode* program_structure = parse_program();
    if (error_log.error_count || !program_structure) {
        printf("Syntax errors found:\n");
        display_errors();
        free_program_tree(program_structure);
        return;
    }
    
    check_program_semantics(program_structure);
    check_for_unused_variables();
    if (error_log.error_count) {
        printf("Semantic errors found:\n");
        display_errors();
        free_program_tree(program_structure);
        return;
    }
/*
    printf("\nProgram Structure:\n");
    display_program_structure(program_structure, 0);
    printf("\n");
    print_ast(program_structure, 0);
*/    
    FILE* output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Cannot create output file: %s\n", output_filename);
        free_program_tree(program_structure);
        return;
    }
    
    setup_registers();
    generate_assembly_code(program_structure, output_file);
    fclose(output_file);
    
    printf("Compilation successful! Output file: %s\n", output_filename);
    show_generated_code(output_filename);
    free_program_tree(program_structure);
}

char* read_source_code() {
    FILE* source_file = fopen("code.b", "r");
    if (!source_file) {
        printf("Could not open source file: code.b\n");
        return NULL;
    }
    
    char* code = malloc(50000);
    if (!code) {
        printf("Not enough memory to read source code\n");
        fclose(source_file);
        return NULL;
    }
    
    code[0] = '\0';
    char line[250];
    
    while (fgets(line, sizeof(line), source_file)) {
        strcat(code, line);
    }
    
    fclose(source_file);
    return code;
}

int main() {
    printf("Submitted by Kian and Charles\n");
    
    char* source_code = read_source_code();
    if (!source_code) return 1;
    
    printf("Source Code:\n%s\n\n", source_code);
    compile_program(source_code, "output.s");
    
    free(source_code);
    return 0;
}