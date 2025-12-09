%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "types.h" 

extern int yylex();
extern void yyerror(const char *s);
extern int yylineno;

/* Declare the function from tools.c */
extern int checkError(int a, int b);

/* Symbol Table Definition */
typedef struct {
    char name[50];
    union {
        int int_val;
        float float_val;
        char char_val;
        char *str_val;
    } value;
    char type[10];  // "int", "float", "char", "string"
    int is_initialized;
    int is_used;
} symbol;

symbol table[100];
int symCount = 0;

/* Function Prototypes */
symbol* getSymbol(const char *name);
int varExists(const char *name);
void declareVar(const char *name, const char *type);
char* processString(const char *raw_str); /* UPDATED NAME */
ExprVal do_math(ExprVal v1, ExprVal v2, char op);
void update_variable(char *name, ExprVal val, char op);

/* Print list functions */
print_item* create_print_item();
void free_print_list(print_item *list);
void execute_print(print_item *list);

int yyparse(void);
void check_unused_variables();

%}

%union {
    int num;
    float float_num;
    char *str;
    ExprVal val;
    print_item *print_list;
}

// Tokens
%token NEWLINE
%token ilimbag numero sulat letra desimal
%token ASSIGN COMMA SEMICOLON LPAREN RPAREN
%token PLUS MINUS MULTIPLY DIVIDE 
%token INCREMENT DECREMENT
%token PLUS_ASSIGN MINUS_ASSIGN MULTIPLY_ASSIGN DIVIDE_ASSIGN 
%token <str> STRING_LITERAL
%token <num> INTEGER
%token <float_num> FLOAT
%token <str> CHARACTER
%token <str> IDENTIFIER
%token ERROR_CHAR

// Operator precedence
%left PLUS MINUS
%left MULTIPLY DIVIDE 
%right UMINUS UPLUS 

// Types
%type <val> expr term factor
%type <str> string_val char_val
%type <print_list> print_items print_item

%%

program: /* empty */
    | program line
    ;

line: statement NEWLINE 
    | statement  /* Allows file to end without newline */
    | NEWLINE 
    ;

statement: declaration      
    | assignment            
    | print_stmt
    | expr { /* Silent execution */ }
    ;

declaration: numero IDENTIFIER {
        declareVar($2, "int");
        free($2);
    }
    | numero IDENTIFIER ASSIGN expr {
        declareVar($2, "int");
        symbol *var = getSymbol($2);
        if ($4.type == 0 || $4.type == 3) var->value.int_val = $4.i_val;
        else if ($4.type == 1) var->value.int_val = (int)$4.f_val;
        else yyerror("Cannot assign string to int");
        var->is_initialized = 1;
        free($2);
    }
    | desimal IDENTIFIER {
        declareVar($2, "float");
        free($2);
    }
    | desimal IDENTIFIER ASSIGN expr {
        declareVar($2, "float");
        symbol *var = getSymbol($2);
        if ($4.type == 1) var->value.float_val = $4.f_val;
        else if ($4.type == 0 || $4.type == 3) var->value.float_val = (float)$4.i_val;
        else yyerror("Cannot assign string to float");
        var->is_initialized = 1;
        free($2);
    }
    | letra IDENTIFIER {
        declareVar($2, "char");
        free($2);
    }
    | letra IDENTIFIER ASSIGN char_val {
        declareVar($2, "char");
        symbol *var = getSymbol($2);
        if ($4 && $4[0]) var->value.char_val = $4[0];
        var->is_initialized = 1;
        free($2);
        free($4);
    }
    | sulat IDENTIFIER {
        declareVar($2, "string");
        free($2);
    }
    | sulat IDENTIFIER ASSIGN string_val {
        declareVar($2, "string");
        symbol *var = getSymbol($2);
        var->value.str_val = $4; 
        var->is_initialized = 1;
        free($2);
    }
    ;

assignment: IDENTIFIER ASSIGN expr {
        update_variable($1, $3, '=');
        free($1);
    }
    | IDENTIFIER PLUS_ASSIGN expr {
        update_variable($1, $3, '+');
        free($1);
    }
    | IDENTIFIER MINUS_ASSIGN expr {
        update_variable($1, $3, '-');
        free($1);
    }
    | IDENTIFIER MULTIPLY_ASSIGN expr {
        update_variable($1, $3, '*');
        free($1);
    }
    | IDENTIFIER DIVIDE_ASSIGN expr {
        update_variable($1, $3, '/');
        free($1);
    }
    | IDENTIFIER ASSIGN char_val {
        if (!varExists($1)) yyerror("Undeclared variable");
        symbol *var = getSymbol($1);
        if (strcmp(var->type, "char") == 0) {
            var->value.char_val = $3[0];
            var->is_initialized = 1;
        } else if (strcmp(var->type, "string") == 0) {
             if (var->value.str_val) free(var->value.str_val);
             char *s = malloc(2); s[0]=$3[0]; s[1]='\0';
             var->value.str_val = s;
             var->is_initialized = 1;
        } else {
             yyerror("Type mismatch in assignment");
        }
        free($1); free($3);
    }
    | IDENTIFIER ASSIGN string_val {
        if (!varExists($1)) yyerror("Undeclared variable");
        symbol *var = getSymbol($1);
        if (strcmp(var->type, "string") != 0) yyerror("Cannot assign string to non-string variable");
        if (var->value.str_val) free(var->value.str_val);
        var->value.str_val = $3;
        var->is_initialized = 1;
        free($1);
    }
    | IDENTIFIER INCREMENT {
        if (!varExists($1)) yyerror("Undeclared variable");
        symbol *var = getSymbol($1);
        if (strcmp(var->type, "int") == 0) var->value.int_val++;
        else if (strcmp(var->type, "float") == 0) var->value.float_val += 1.0;
        else yyerror("++ only for numeric types");
        free($1);
    }
    | IDENTIFIER DECREMENT {
        if (!varExists($1)) yyerror("Undeclared variable");
        symbol *var = getSymbol($1);
        if (strcmp(var->type, "int") == 0) var->value.int_val--;
        else if (strcmp(var->type, "float") == 0) var->value.float_val -= 1.0;
        else yyerror("-- only for numeric types");
        free($1);
    }
    | INCREMENT IDENTIFIER {
        if (!varExists($2)) yyerror("Undeclared variable");
        symbol *var = getSymbol($2);
        if (strcmp(var->type, "int") == 0) var->value.int_val++;
        else if (strcmp(var->type, "float") == 0) var->value.float_val += 1.0;
        else yyerror("++ only for numeric types");
        free($2);
    }
    | DECREMENT IDENTIFIER {
        if (!varExists($2)) yyerror("Undeclared variable");
        symbol *var = getSymbol($2);
        if (strcmp(var->type, "int") == 0) var->value.int_val--;
        else if (strcmp(var->type, "float") == 0) var->value.float_val -= 1.0;
        else yyerror("-- only for numeric types");
        free($2);
    }
    ;

print_stmt: ilimbag print_items {
        execute_print($2);
        free_print_list($2);
    }
    | ilimbag {
        printf("\n");
    }
    ;

print_items: print_item { $$ = $1; }
    | print_items COMMA print_item {
        print_item *current = $1;
        while (current->next) current = current->next;
        current->next = $3;
        $$ = $1;
    }
    ;

print_item: STRING_LITERAL {
        $$ = create_print_item();
        $$->type = PRINT_STRING;
        $$->value.str = processString($1); /* UPDATED */
        free($1);
    }
    | CHARACTER {
        $$ = create_print_item();
        $$->type = PRINT_CHAR;
        char *temp = processString($1); /* UPDATED */
        $$->value.char_val = temp[0];
        free(temp);
        free($1);
    }
    | expr {
        $$ = create_print_item();
        $$->type = PRINT_EXPR;
        
        $$->value.expr_val.type = $1.type;
        
        if ($1.type == 0) { // int
            $$->value.expr_val.value.i_val = $1.i_val;
        } else if ($1.type == 1) { // float
            $$->value.expr_val.value.f_val = $1.f_val;
        } else if ($1.type == 2) { // string
            $$->value.expr_val.value.s_val = $1.s_val ? strdup($1.s_val) : NULL;
        } else if ($1.type == 3) { // char
            $$->value.expr_val.value.i_val = $1.i_val;
        }
    }
    ;

expr: term { $$ = $1; }
    | expr PLUS term { $$ = do_math($1, $3, '+'); }
    | expr MINUS term { $$ = do_math($1, $3, '-'); }
    ;

term: factor { $$ = $1; }
    | term MULTIPLY factor { $$ = do_math($1, $3, '*'); }
    | term DIVIDE factor { $$ = do_math($1, $3, '/'); }
    ;

factor: INTEGER { 
        $$.type = 0; 
        $$.i_val = $1; 
        $$.f_val = 0.0; 
    }
    | FLOAT { 
        $$.type = 1; 
        $$.i_val = 0; 
        $$.f_val = $1; 
    }
    | IDENTIFIER {
        if (!varExists($1)) yyerror("Undeclared variable");
        symbol *var = getSymbol($1);
        if (strcmp(var->type, "int") == 0) {
            $$.type = 0; $$.i_val = var->value.int_val;
        } else if (strcmp(var->type, "float") == 0) {
            $$.type = 1; $$.f_val = var->value.float_val;
        } else if (strcmp(var->type, "char") == 0) {
            $$.type = 3; /* Type 3 for Char */
            $$.i_val = (int)var->value.char_val; 
        } else if (strcmp(var->type, "string") == 0) {
            $$.type = 2; $$.s_val = var->value.str_val;
        } else {
             $$.type=0; 
        }
        var->is_used = 1;
        free($1);
    }
    | MINUS factor %prec UMINUS { 
        $$ = $2;
        if($$.type == 0 || $$.type == 3) $$.i_val = -$$.i_val;
        else if($$.type == 1) $$.f_val = -$$.f_val;
        else yyerror("Cannot negate a string");
    }
    | PLUS factor %prec UPLUS { 
        $$ = $2; 
        if($$.type == 2) yyerror("Cannot use + on string");
    }
    | LPAREN expr RPAREN { $$ = $2; }
    ;

string_val: STRING_LITERAL {
        $$ = processString($1); /* UPDATED */
        free($1);
    }
    ;

char_val: CHARACTER {
        char* temp = processString($1); /* UPDATED */
        $$ = temp;
        free($1);
    }
    ;

%%

/* --- Helper Functions --- */

void update_variable(char *name, ExprVal val, char op) {
    if (!varExists(name)) yyerror("Undeclared variable");
    
    symbol *var = getSymbol(name);
    
    float val_f = (val.type == 1) ? val.f_val : (float)val.i_val;
    int val_i   = (val.type == 1) ? (int)val.f_val : val.i_val;
    
    if (strcmp(var->type, "int") == 0) {
        if (val.type == 2) yyerror("Cannot assign string to int");
        switch(op) {
            case '=': var->value.int_val = val_i; break;
            case '+': var->value.int_val += val_i; break;
            case '-': var->value.int_val -= val_i; break;
            case '*': var->value.int_val *= val_i; break;
            case '/': 
                if (checkError(var->value.int_val, val_i) == -1) yyerror("Division by zero");
                var->value.int_val /= val_i; 
                break;
        }
        var->is_initialized = 1;
    } 
    else if (strcmp(var->type, "float") == 0) {
        if (val.type == 2) yyerror("Cannot assign string to float");
        switch(op) {
            case '=': var->value.float_val = val_f; break;
            case '+': var->value.float_val += val_f; break;
            case '-': var->value.float_val -= val_f; break;
            case '*': var->value.float_val *= val_f; break;
            case '/': 
                if (val_f == 0.0) yyerror("Division by zero");
                var->value.float_val /= val_f; 
                break;
        }
        var->is_initialized = 1;
    }
    else {
        yyerror("Invalid operation on non-numeric variable");
    }
}

ExprVal do_math(ExprVal v1, ExprVal v2, char op) {
    ExprVal res;
    res.i_val = 0; res.f_val = 0; res.s_val = NULL;

    if (v1.type == 2 || v2.type == 2) {
        yyerror("Cannot perform arithmetic on strings");
    }

    int is_float = (v1.type == 1 || v2.type == 1);
    
    float f1 = (v1.type == 1) ? v1.f_val : (float)v1.i_val;
    float f2 = (v2.type == 1) ? v2.f_val : (float)v2.i_val;
    
    int i1 = (v1.type == 1) ? (int)v1.f_val : v1.i_val;
    int i2 = (v2.type == 1) ? (int)v2.f_val : v2.i_val;

    res.type = is_float ? 1 : 0;

    switch(op) {
        case '+': 
            if(is_float) res.f_val = f1 + f2; 
            else res.i_val = i1 + i2; 
            break;
        case '-': 
            if(is_float) res.f_val = f1 - f2; 
            else res.i_val = i1 - i2; 
            break;
        case '*': 
            if(is_float) res.f_val = f1 * f2; 
            else res.i_val = i1 * i2; 
            break;
        case '/': 
            if (is_float) {
                if (f2 == 0.0) yyerror("Division by zero");
                res.f_val = f1 / f2;
            } else {
                if (checkError(i1, i2) == -1) yyerror("Division by zero");
                res.i_val = i1 / i2; 
            }
            break;
    }
    return res;
}

/* REPLACED removeQuotes with processString to handle \n */
char* processString(const char *raw_str) {
    if (raw_str == NULL) return NULL;
    int len = strlen(raw_str);
    
    char *processed = malloc(len + 1);
    if (!processed) return NULL;

    int i = 0, j = 0;
    
    // Check for surrounding quotes to strip them
    int start = 0, end = len;
    if (len >= 2 && (raw_str[0] == '"' || raw_str[0] == '\'') && raw_str[0] == raw_str[len-1]) {
        start = 1;
        end = len - 1;
    }

    // Iterate through the string processing escapes
    for (i = start; i < end; i++) {
        if (raw_str[i] == '\\' && i + 1 < end) {
            switch (raw_str[i+1]) {
                case 'n': processed[j++] = '\n'; i++; break; // Newline
                case 't': processed[j++] = '\t'; i++; break; // Tab
                case '\\': processed[j++] = '\\'; i++; break; // Backslash
                case '"': processed[j++] = '\"'; i++; break; // Double Quote
                case '\'': processed[j++] = '\''; i++; break; // Single Quote
                default: processed[j++] = raw_str[i]; break; // Ignore invalid escape
            }
        } else {
            processed[j++] = raw_str[i];
        }
    }
    processed[j] = '\0';
    return processed;
}

symbol* getSymbol(const char *name) {
    for (int i = 0; i < symCount; i++)
        if (strcmp(table[i].name, name) == 0)
            return &table[i];
    return NULL;
}

int varExists(const char *name) {
    return getSymbol(name) != NULL;
}

void declareVar(const char *name, const char *type) {
    if (varExists(name)) yyerror("Redeclaration of variable");
    if (symCount >= 100) yyerror("Symbol table overflow");
    
    strcpy(table[symCount].name, name);
    strcpy(table[symCount].type, type);
    table[symCount].value.int_val = 0; 
    table[symCount].value.str_val = NULL;
    table[symCount].is_initialized = 0;
    table[symCount].is_used = 0;
    symCount++;
}

print_item* create_print_item() {
    print_item *item = (print_item*)malloc(sizeof(print_item));
    item->next = NULL;
    item->value.str = NULL; 
    return item;
}

void free_print_list(print_item *list) {
    while (list) {
        print_item *next = list->next;
        if (list->type == PRINT_STRING && list->value.str) {
            free(list->value.str);
        } 
        else if (list->type == PRINT_EXPR && list->value.expr_val.type == 2 && list->value.expr_val.value.s_val) {
            free(list->value.expr_val.value.s_val);
        }
        free(list);
        list = next;
    }
}

/* UPDATED execute_print to handle spacing around newlines */
void execute_print(print_item *list) {
    print_item *current = list;
    int last_was_newline = 0; /* Tracks if the previous print ended in \n */

    while (current) {
        /* Only print a space if it's NOT the first item AND the last item didn't end in \n */
        if (current != list && !last_was_newline) {
            printf(" ");
        }

        switch (current->type) {
            case PRINT_STRING:
                printf("%s", current->value.str);
                if (current->value.str && strlen(current->value.str) > 0) {
                     char last = current->value.str[strlen(current->value.str) - 1];
                     last_was_newline = (last == '\n');
                } else {
                     last_was_newline = 0;
                }
                break;
            case PRINT_CHAR:
                printf("%c", current->value.char_val);
                last_was_newline = (current->value.char_val == '\n');
                break;
            case PRINT_EXPR:
                if (current->value.expr_val.type == 1) { // Float
                    printf("%.2f", current->value.expr_val.value.f_val);
                } else if (current->value.expr_val.type == 0) { // Int
                    printf("%d", current->value.expr_val.value.i_val);
                } else if (current->value.expr_val.type == 2) { // String Variable
                    printf("%s", current->value.expr_val.value.s_val ? current->value.expr_val.value.s_val : "(null)");
                } else if (current->value.expr_val.type == 3) { // Char Variable
                    printf("%c", (char)current->value.expr_val.value.i_val);
                }
                last_was_newline = 0; // Assume numbers/exprs don't end in \n
                break;
        }
        current = current->next;
    }
    printf("\n");
}

void check_unused_variables() {
    printf("\n=== Variable Usage Report ===\n");
    int warnings = 0;
    for (int i = 0; i < symCount; i++) {
        if (!table[i].is_used) {
            printf("Warning: Variable '%s' (type: %s) declared but never used\n", table[i].name, table[i].type);
            warnings++;
        }
        if (!table[i].is_initialized && table[i].is_used) {
            printf("Warning: Variable '%s' (type: %s) used but may not have been initialized\n", table[i].name, table[i].type);
            warnings++;
        }
    }
    if (warnings == 0) {
        printf("No warnings found.\n");
    }
}

int main(void) {
    yylineno = 1;
    int result = yyparse();
    check_unused_variables();
    
    for (int i = 0; i < symCount; i++) {
        if (strcmp(table[i].type, "string") == 0 && table[i].value.str_val) {
            free(table[i].value.str_val);
        }
    }
    return result;
}

void yyerror(const char *s) {
    fprintf(stderr, "LINE %d ERROR: %s\n", yylineno, s);
    exit(1); 
}