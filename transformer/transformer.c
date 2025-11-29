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
    OPERATION, NUM, ID
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

struct Token *create_token(Token_Type type, char *name, int line) {
    struct Token *token = malloc(sizeof(struct Token));
    if(!token) {
        printf("\n\nFAILED MEMORY ALLOCATION TOKEN!!");
        return NULL;
    }

    *(token) = (struct Token){type, "", line, NULL};
    strncpy(token->name, name, MAX_NAME_LENGTH);
    return token;
}

struct Token *add_token(struct Token *head, Token_Type type, char *name, int line) {
    struct Token *cur = NULL;
}

struct ParseTree *create_node(ParseNode type, struct Token *token, 
    struct ParseTree *left, struct ParseTree *right) {

    struct ParseTree *node = malloc(sizeof(struct ParseTree));
    if(!node) {
        printf("\n\nFAILED MEMORY ALLOCATION NODE!!");
        return NULL;
    }
    *node = (struct ParseTree) {type, token, left, right};
}

#define IS_NUM(c) (c >= '0' & c <= '9')
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

struct Token *tokenized(char *source_code, struct Token *tokens) {
    char b[MAX_NAME_LENGTH] = {0}; 
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
            printf("\n%s\n", buffer);
            if(!tokens) 
                curr = tokens = create_token(NUMBER, buffer, line); 
            else 
                curr = curr->next = create_token(NUMBER, buffer, line);
            continue;
        }
        
            struct Token *t = NULL;
        if(IS_LETTER(cur_char)) {
            char buffer[MAX_NAME_LENGTH] = {0};
            int buffer_index = 0;

            while(IS_LETTER(cur_char) || IS_NUM(cur_char)) {
                buffer[buffer_index++] = cur_char;               
                cur_char = *(source_code + position++);
            }
            printf("\n%s\n", buffer);

            if(strcmp(buffer, "int") == 0 || strcmp(buffer, "char") == 0) 
                t = create_token(DATA_TYPE, buffer, line);
            else 
                t = create_token(IDENTIFIER, buffer, line);

            if(!tokens) 
                curr = tokens = t;
            else
                curr = tokens->next = t;
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
            curr = tokens -> next = temp;
        cur_char = *(source_code + position++);
    }
    
    curr = create_token(END_OF_FILE, " ", line);
    return tokens;
}

// Token peek_token() {
//     return tokens[cur_token];
// }

// Token get_next_token() {

// }

struct ParseTree *parse_statement() {
    // Token token = get_next_token();
}

struct ParseTree *parse_program() {
    struct ParseTree *head = NULL;
    struct ParseTree *current = NULL;
    // while(peek_token().type != END_OF_FILE) {
    //     ParseTree statement = parse_statement();
    //     if(!head) {
    //         head = current = parse_program(PROGRAM, peek_token(), statement, NULL);
    //     }else {
    //         current->right = parse_program(PROGRAM, peek_token(), statement, NULL);
    //     }
    // }
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
    free_nodes(nodes->left);
    free_nodes(nodes->right);
    free(nodes);
}

void compile(char *source_code, char *output_name) {
    struct Token *tokens = tokenized(source_code, tokens);

    
    print_tokens(tokens);

    // ParseTree *parse_tree = parse_program();

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

int main() {
    char *source_code = read_file();
    printf("%s\n", source_code);

    compile(source_code, "output.s");

    return 0;
}