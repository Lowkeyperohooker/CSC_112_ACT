%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "types.h" 

extern int yylex();
extern void yyerror(const char *s);
extern int yylineno;

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
char* removeQuotes(const char *quoted_str);
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
        if ($4.type == 0) var->value.int_val = $4.i_val;
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
        else if ($4.type == 0) var->value.float_val = (float)$4.i_val;
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
        $$->value.str = removeQuotes($1);
        free($1);
    }
    | CHARACTER {
        $$ = create_print_item();
        $$->type = PRINT_CHAR;
        $$->value.char_val = removeQuotes($1)[0];
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
            $$.type = 0; $$.i_val = (int)var->value.char_val; 
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
        if($$.type == 0) $$.i_val = -$$.i_val;
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
        $$ = removeQuotes($1);
        free($1);
    }
    ;

char_val: CHARACTER {
        $$ = removeQuotes($1);
        free($1);
    }
    ;

%%

/* --- Helper Functions --- */

void update_variable(char *name, ExprVal val, char op) {
    if (!varExists(name)) yyerror("Undeclared variable");
    
    symbol *var = getSymbol(name);
    
    float val_f = (val.type == 1) ? val.f_val : (float)val.i_val;
    int val_i   = (val.type == 0) ? val.i_val : (int)val.f_val;
    
    if (strcmp(var->type, "int") == 0) {
        if (val.type == 2) yyerror("Cannot assign string to int");
        switch(op) {
            case '=': var->value.int_val = val_i; break;
            case '+': var->value.int_val += val_i; break;
            case '-': var->value.int_val -= val_i; break;
            case '*': var->value.int_val *= val_i; break;
            case '/': 
                if (val_i == 0) yyerror("Division by zero");
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
    float f1 = (v1.type == 0) ? (float)v1.i_val : v1.f_val;
    float f2 = (v2.type == 0) ? (float)v2.i_val : v2.f_val;

    /* Removed Modulo Case */

    res.type = is_float ? 1 : 0;

    switch(op) {
        case '+': 
            if(is_float) res.f_val = f1 + f2; 
            else res.i_val = v1.i_val + v2.i_val; 
            break;
        case '-': 
            if(is_float) res.f_val = f1 - f2; 
            else res.i_val = v1.i_val - v2.i_val; 
            break;
        case '*': 
            if(is_float) res.f_val = f1 * f2; 
            else res.i_val = v1.i_val * v2.i_val; 
            break;
        case '/': 
            if (f2 == 0) yyerror("Division by zero");
            if(is_float) res.f_val = f1 / f2; 
            else res.i_val = v1.i_val / v2.i_val; 
            break;
    }
    return res;
}

char* removeQuotes(const char *quoted_str) {
    if (quoted_str == NULL) return NULL;
    int len = strlen(quoted_str);
    if (len >= 2 && (quoted_str[0] == '"' || quoted_str[0] == '\'') && quoted_str[0] == quoted_str[len-1]) {
        char *result = malloc(len - 1);
        if (result) {
            strncpy(result, quoted_str + 1, len - 2);
            result[len - 2] = '\0';
            return result;
        }
    }
    return strdup(quoted_str);
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

void execute_print(print_item *list) {
    int first_item = 1;
    print_item *current = list;
    
    while (current) {
        if (!first_item) printf(" ");
        first_item = 0;
        
        switch (current->type) {
            case PRINT_STRING:
                printf("%s", current->value.str);
                break;
            case PRINT_CHAR:
                printf("%c", current->value.char_val);
                break;
            case PRINT_EXPR:
                if (current->value.expr_val.type == 1) { // Float
                    printf("%.2f", current->value.expr_val.value.f_val);
                } else if (current->value.expr_val.type == 0) { // Int
                    printf("%d", current->value.expr_val.value.i_val);
                } else if (current->value.expr_val.type == 2) { // String Variable
                    printf("%s", current->value.expr_val.value.s_val ? current->value.expr_val.value.s_val : "(null)");
                }
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