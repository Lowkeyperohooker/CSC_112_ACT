#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_LINE 999999
#define MAX_NAME_LENGTH 40

typedef enum {
    IDENTIFIER, NUMBER, 
    ADDITION, DIFFERENCE, MULTIPLICATION, DIVISION, ASSIGN,
    DATA_TYPE, SEMI_COLON,
    L_PAR, R_PAR,
    END_OF_FILE,
}Token_Type;

typedef enum {
    PROGRAM, STATEMENT, 
    ASSIGNMENT, DECLARATION,
    EXPRESSION, TERM, FACTOR, 
    OPERATION, NUM, ID,
    EMPTY
}ParseNode;

struct Token{
    Token_Type type;
    char name[MAX_NAME_LENGTH];
    int line;
    struct Token *next;
};

struct ParseTree{
    ParseNode type;
    struct Token *token;
    struct ParseTree *left;
    struct ParseTree *right;
};

struct Symbol{
    char name[MAX_NAME_LENGTH];
    int is_initiallized; 
    int memory_address;
    struct Symbol *next;
};

struct Error{
    char *name;
    char *start;
    int line; 
    struct Error *next;
};


int memory_offset = 0;

struct Error *head_error = NULL;
struct Error *curr_error = NULL;
struct Symbol *head_symbol = NULL;
struct Symbol *curr_symbol = NULL;

char *get_token(Token_Type token);

void add_error(char *name, char *start, int line) {
    struct Error *error = malloc(sizeof(struct Error));
    if(!error) {
        printf("\n\nFAILED MEMORY ALLOCATION TOKEN!!");
        return;
    }
    error -> name=name; 
    error -> start=start;
    error -> line=line;
    error -> next = NULL;

    if(!head_error) 
        head_error = curr_error = error;
    else
        curr_error = curr_error -> next = error;
}

struct Token *create_token(Token_Type type, char *name, int line) {
    struct Token *token = malloc(sizeof(struct Token));
    if(!token) {
        printf("\n\nFAILED MEMORY ALLOCATION TOKEN!!");
        return NULL;
    }

    token->type = type;
    token->line = line;
    token->next = NULL;
    strncpy(token->name, name, MAX_NAME_LENGTH - 1);
    return token;
}

struct ParseTree *create_node(ParseNode type, struct Token *token, 
    struct ParseTree *left, struct ParseTree *right) {

    struct ParseTree *node = malloc(sizeof(struct ParseTree));
    if(!node) {
        printf("\n\nFAILED MEMORY ALLOCATION NODE!!");
        return NULL;
    }
    node -> type = type;
    node -> token = token;
    node -> left = left;
    node -> right = right;
    *node = (struct ParseTree) {type, token, left, right};
}

#define IS_NUM(c) (c >= '0' && c <= '9')
#define IS_LETTER(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))

void skip_space(char **source_code, char *cur_char, int *line, int *position) {
    while(true) {
        if(*cur_char == ' ') {
            *cur_char = *(*source_code + (*position)++);
        } else if(*cur_char == '\n') {
            *cur_char = *(*source_code + (*position)++);
            (*line)++;
        }else {
            return;
        }
    }
}

struct Token *tokenized(char *source_code) {
    struct Token *tokens = NULL;
    struct Token *curr = tokens;
    int line = 1;
    int position = 0;

    char cur_char = *(source_code + position++);
    while(cur_char) {
        skip_space(&source_code, &cur_char, &line, &position);

        if(IS_NUM(cur_char)) {
            char buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;

            while(IS_NUM(cur_char)) {
                buffer[buffer_index++] = cur_char;               
                cur_char = *(source_code + position++);
            }
            struct Token *token = create_token(NUMBER, buffer, line); 
            if(!tokens) 
                curr = tokens = token;
            else 
                curr = curr->next = token;
            continue;
        }
        
        if(IS_LETTER(cur_char)) {
            char buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;

            while(IS_LETTER(cur_char) || IS_NUM(cur_char)) {
                buffer[buffer_index++] = cur_char;               
                cur_char = *(source_code + position++);
            }

            struct Token *token = NULL;
            if(strcmp(buffer, "int") == 0 || strcmp(buffer, "char") == 0) 
                token = create_token(DATA_TYPE, buffer, line);
            else 
                token = create_token(IDENTIFIER, buffer, line);

            if(!tokens) 
                curr = tokens = token;
            else
                curr = curr->next = token;
            continue;
        }
        struct Token *temp = NULL;
        switch (cur_char) {
            case '(': temp = create_token(L_PAR, "(", line); break;
            case ')': temp = create_token(R_PAR, ")", line); break;
            case ';': temp = create_token(SEMI_COLON, ";", line); break;
            case '=': temp = create_token(ASSIGN, "=", line); break;
            case '+': temp = create_token(ADDITION, "+", line); break;
            case '-': temp = create_token(DIFFERENCE, "-", line); break;
            case '*': temp = create_token(MULTIPLICATION, "*", line); break;
            case '/': temp = create_token(DIVISION, "/", line); break;
            default: break;;
        }
        if(!tokens) 
            tokens = temp;
        else 
            curr = curr -> next = temp;
        cur_char = *(source_code + position++);
    }
    
    if(!tokens) 
        tokens = curr = create_token(END_OF_FILE, " ", line);
    else 
        curr= curr -> next = create_token(END_OF_FILE, " ", line);
    return tokens;
}

struct Symbol *find_variable(char *name) {
    struct Symbol *curr = head_symbol;
    while(curr) {
        if(strcmp(name,curr -> name) == 0)
            return curr;
        curr = curr -> next;
    }
    return NULL;
}

void  add_variable(char *name) {
    if(find_variable(name)) 
        return;

    struct Symbol *symbol = malloc(sizeof(struct Symbol));
    if(!symbol) {
        printf("\n\nFAILED MEMORY ALLOCATION SYMBOL");
        return;
    }

    strncpy(symbol -> name, name, MAX_NAME_LENGTH);
    symbol -> is_initiallized = 0;
    symbol -> memory_address = memory_offset;
    memory_offset+= 8;

    if(!head_symbol) 
        head_symbol = curr_symbol = symbol;
    else
        curr_symbol = curr_symbol -> next = symbol;   
}

struct ParseTree* parse_expression(struct Token **tokens) {
    return NULL;
}

struct ParseTree *parse_assignment(struct Token **tokens) {
    if(!(*tokens) -> type == IDENTIFIER) {
        add_error("expected identifier", (*tokens) -> name, (*tokens) -> line);
        return NULL;
    }
    *tokens = (*tokens) -> next;

    if((*tokens) -> type == SEMI_COLON) {
        return NULL;
    }
    if((*tokens) -> type == ASSIGN) {
        struct Token *next = (*tokens) -> next;
        struct ParseTree *node = parse_expression(&next);
        *tokens = next;
        return node;
    }
}

struct ParseTree *parse_declaration(struct Token **tokens) {    
    if(!(*tokens) -> type == DATA_TYPE) {
        add_error("Expected_variable", (*tokens) -> name, (*tokens) -> line);
        return NULL;
    }

    *tokens = (*tokens) -> next;
    printf("\n\n%s", get_token((*tokens) -> type));

    if((*tokens) -> type == IDENTIFIER) {
        struct Token* next_token = (*tokens) -> next;

        if(next_token -> type == SEMI_COLON) {

            add_variable(next_token -> name);
            curr_symbol -> is_initiallized = 1;

            return create_node(DECLARATION, next_token, NULL, NULL);
        } else if(next_token -> type == ASSIGN) {
            return parse_assignment(tokens);
        }
    }
    return NULL;
}

struct ParseTree *parse_statement(struct Token **tokens) {
    *tokens = (*tokens) -> next;

    if((*tokens) -> type == SEMI_COLON) {
        return NULL;
    }

    if((*tokens) -> type == DATA_TYPE) {
        return parse_declaration(tokens);
    }

    if((*tokens) -> type == NUM) {
        return NULL;
    }
    return NULL;
}

struct ParseTree *parse_program(struct Token *tokens) {
    struct ParseTree *head = NULL;
    struct ParseTree *current = NULL;
    while(tokens -> type != END_OF_FILE) {
        struct ParseTree *statement = parse_statement(&tokens);
        if(!head) {
            head = current = create_node(PROGRAM, tokens, statement, NULL);
        }else {
            current->right = create_node(PROGRAM, tokens, statement, NULL);
        }
    }
    return head;
}

char *get_token(Token_Type token) {
    switch (token) {
        case IDENTIFIER: return "identifier"; break;
        case ASSIGN: return "assign"; break;
        case NUMBER: return "number"; break;
        case ADDITION: return "addition"; break;
        case DIFFERENCE: return "difference"; break;
        case MULTIPLICATION: return "multiplication"; break;
        case DIVISION: return "division"; break;
        case DATA_TYPE: return "data type"; break;
        case SEMI_COLON: return "semi colon"; break;
        case L_PAR: return "left paranthesis"; break;
        case R_PAR: return "right paranthesis"; break;
        case END_OF_FILE: return "ending"; break;
        default: return "Ambot"; break;
    }
}

void print_tokens(struct Token *token) {
    while(token) {
        struct Token *next = token->next;

        printf("\n\nToken type: %s", get_token(token->type));
        printf("\nToken name: %s", token->name);
        printf("\nToken line: %d", token->line);

        token = next;
    }
}

void free_token(struct Token *head) {
    while(head) {
        struct Token *next = head->next;
        free(head);
        head = next;
    }
}

void free_nodes(struct ParseTree *nodes) {
    if(!nodes) {
        return;
    }
    free_nodes(nodes->left);
    free_nodes(nodes->right);
    free(nodes);
}

void compile(char *source_code, char *output_name) {
    struct Token *tokens = tokenized(source_code);

    
    print_tokens(tokens);

    printf("\n\nParsing");
    struct ParseTree *parse_tree = parse_program(tokens);

    free_token(tokens);
    // free_node(parse_tree);
}

char* read_file() {
    FILE *source_code = fopen("code.b","r");
   
    if(!source_code) {
        printf("\nFAILED OPEN SOURCE CODE!");
        return NULL;
    }

    char *code = malloc(MAX_LINE);
    
    if(!code) {
        printf("\nFAILED MEMORY ALLOCATION!");
        return NULL;
    }
    code[0] = '\0';
    char line[256];

    while(fgets(line, sizeof(line), source_code)) {
        strcat(code, line);
    }

    fclose(source_code);
    return code;
}

void free_symbols(struct Symbol *curr) {
    while(curr) {
        struct Symbol *temp = curr;
        curr = curr -> next;
        free(temp);
    }
}

void free_errors(struct Error *curr) {
    while(curr) {
        struct Error *temp = curr;
        curr = curr -> next;
        free(temp);
    }
}

int main() {
    char *source_code = read_file();
    printf("%s\n", source_code);

    compile(source_code, "output.s");

    free_symbols(head_symbol);
    free_errors(head_error);
    return 0;
}