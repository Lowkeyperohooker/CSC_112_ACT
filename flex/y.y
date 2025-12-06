%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex();
extern void yyerror(const char *s);
extern int yylineno;

int lineCount = 1;

typedef struct {
    char name[50];
    int value;
    char type[10];
    char *str_value;
    int is_initialized;
    int is_used;
} symbol;

symbol table[100];
int symCount = 0;

int getVarValue(const char *name);
void setVarValue(const char *name, int value);
int varExists(const char *name);
void declareVar(const char *name, const char *type);
void setStringValue(const char *name, char *value);
symbol* getSymbol(const char *name);
char* removeQuotes(const char *quoted_str);
void custom_printf(const char *format, symbol *var);
%}

%union {
    int num;
    char *str;
}

%token NEWLINE
%token ilimbag numero sulat
%token ASSIGN COMMA SEMICOLON LPAREN RPAREN
%token PLUS MINUS MULTIPLY DIVIDE MODULO
%token INCREMENT DECREMENT
%token PLUS_ASSIGN MINUS_ASSIGN MULTIPLY_ASSIGN DIVIDE_ASSIGN MODULO_ASSIGN
%token <str> STRING_LITERAL
%token <num> INTEGER
%token <str> IDENTIFIER
%token ERROR_CHAR

%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right UMINUS

%type <num> expr term factor
%type <str> string_val

%%

program: /* empty */
    | program line
    ;

line: statement NEWLINE { lineCount++; }
    | NEWLINE { lineCount++; }
    | error NEWLINE { yyerrok; lineCount++; }
    ;

statement: declaration
    | assignment
    | print_stmt
    | expr_stmt
    ;

declaration: numero IDENTIFIER {
        declareVar($2, "int");
        free($2);
    }
    | numero IDENTIFIER ASSIGN expr {
        declareVar($2, "int");
        setVarValue($2, $4);
        free($2);
    }
    | sulat IDENTIFIER {
        declareVar($2, "string");
        free($2);
    }
    | sulat IDENTIFIER ASSIGN string_val {
        declareVar($2, "string");
        setStringValue($2, $4);
        free($2);
    }
    ;

assignment: IDENTIFIER ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            setVarValue($1, $3);
        }
        free($1);
    }
    | IDENTIFIER PLUS_ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($1);
            setVarValue($1, var->value + $3);
        }
        free($1);
    }
    | IDENTIFIER MINUS_ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($1);
            setVarValue($1, var->value - $3);
        }
        free($1);
    }
    | IDENTIFIER MULTIPLY_ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($1);
            setVarValue($1, var->value * $3);
        }
        free($1);
    }
    | IDENTIFIER DIVIDE_ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else if ($3 == 0) yyerror("Division by zero");
        else {
            symbol *var = getSymbol($1);
            setVarValue($1, var->value / $3);
        }
        free($1);
    }
    | IDENTIFIER MODULO_ASSIGN expr {
        if (!varExists($1)) yyerror("Undeclared variable");
        else if ($3 == 0) yyerror("Modulo by zero");
        else {
            symbol *var = getSymbol($1);
            setVarValue($1, var->value % $3);
        }
        free($1);
    }
    | IDENTIFIER INCREMENT {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($1);
            var->value++;
        }
        free($1);
    }
    | IDENTIFIER DECREMENT {
        if (!varExists($1)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($1);
            var->value--;
        }
        free($1);
    }
    | INCREMENT IDENTIFIER {
        if (!varExists($2)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($2);
            var->value++;
        }
        free($2);
    }
    | DECREMENT IDENTIFIER {
        if (!varExists($2)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($2);
            var->value--;
        }
        free($2);
    }
    ;

print_stmt: ilimbag string_val {
        printf("%s\n", $2);
        free($2);
    }
    | ilimbag expr {
        printf("%d\n", $2);
    }
    | ilimbag IDENTIFIER {
        if (!varExists($2)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($2);
            if (strcmp(var->type, "int") == 0)
                printf("%d\n", var->value);
            else
                printf("%s\n", var->str_value);
            var->is_used = 1;
        }
        free($2);
    }
    | ilimbag string_val COMMA IDENTIFIER {
        if (!varExists($4)) yyerror("Undeclared variable");
        else {
            symbol *var = getSymbol($4);
            custom_printf($2, var);
            var->is_used = 1;
        }
        free($2);
        free($4);
    }
    ;

expr_stmt: expr {
        /* Do nothing - expressions are evaluated silently */
        /* This allows expressions like: a + b * 3 without printing */
    }
    ;

expr: term
    | expr PLUS term { $$ = $1 + $3; }
    | expr MINUS term { $$ = $1 - $3; }
    ;

term: factor
    | term MULTIPLY factor { $$ = $1 * $3; }
    | term DIVIDE factor { 
        if ($3 == 0) { yyerror("Division by zero"); $$ = 0; }
        else { $$ = $1 / $3; }
    }
    | term MODULO factor { 
        if ($3 == 0) { yyerror("Modulo by zero"); $$ = 0; }
        else { $$ = $1 % $3; }
    }
    ;

factor: INTEGER { $$ = $1; }
    | IDENTIFIER {
        if (!varExists($1)) {
            yyerror("Undeclared variable");
            $$ = 0;
        } else {
            symbol *var = getSymbol($1);
            if (strcmp(var->type, "int") == 0) {
                $$ = var->value;
                var->is_used = 1;
            } else {
                yyerror("Variable is not an integer");
                $$ = 0;
            }
        }
        free($1);
    }
    | MINUS factor %prec UMINUS { $$ = -$2; }
    | LPAREN expr RPAREN { $$ = $2; }
    ;

string_val: STRING_LITERAL {
        $$ = removeQuotes($1);
        free($1);
    }
    ;

%%

/* Helper functions - same as before */

char* removeQuotes(const char *quoted_str) {
    if (quoted_str == NULL) return NULL;
    int len = strlen(quoted_str);
    if (len >= 2 && quoted_str[0] == '"' && quoted_str[len-1] == '"') {
        char *result = malloc(len - 1);
        strncpy(result, quoted_str + 1, len - 2);
        result[len - 2] = '\0';
        return result;
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
    if (varExists(name)) {
        yyerror("Redeclaration of variable");
        return;
    }
    strcpy(table[symCount].name, name);
    strcpy(table[symCount].type, type);
    table[symCount].value = 0;
    table[symCount].str_value = NULL;
    table[symCount].is_initialized = 0;
    table[symCount].is_used = 0;
    symCount++;
}

void setVarValue(const char *name, int value) {
    symbol *var = getSymbol(name);
    if (var) {
        var->value = value;
        var->is_initialized = 1;
    }
}

void setStringValue(const char *name, char *value) {
    symbol *var = getSymbol(name);
    if (var) {
        if (var->str_value) free(var->str_value);
        var->str_value = value;
        var->is_initialized = 1;
    }
}

int getVarValue(const char *name) {
    symbol *var = getSymbol(name);
    if (var) {
        if (strcmp(var->type, "int") == 0) return var->value;
        else yyerror("Variable is not an integer");
    } else yyerror("Undefined variable");
    return 0;
}

void custom_printf(const char *format, symbol *var) {
    if (!format || !var) return;
    char *ptr = (char*)format;
    while (*ptr) {
        if (*ptr == '%' && *(ptr + 1)) {
            switch (*(ptr + 1)) {
                case 'd':
                    if (strcmp(var->type, "int") == 0) printf("%d", var->value);
                    else printf("[ERROR: %%d expects integer]");
                    ptr += 2; break;
                case 's':
                    if (strcmp(var->type, "string") == 0) printf("%s", var->str_value ? var->str_value : "(null)");
                    else printf("[ERROR: %%s expects string]");
                    ptr += 2; break;
                case 'c':
                    if (strcmp(var->type, "int") == 0) printf("%c", (char)var->value);
                    else if (strcmp(var->type, "string") == 0 && var->str_value && var->str_value[0])
                        printf("%c", var->str_value[0]);
                    else printf("[ERROR: %%c expects character]");
                    ptr += 2; break;
                case '%': printf("%%"); ptr += 2; break;
                default: putchar(*ptr++); break;
            }
        } else putchar(*ptr++);
    }
    printf("\n");
}

void check_unused_variables() {
    printf("\n=== Variable Usage Report ===\n");
    for (int i = 0; i < symCount; i++) {
        if (!table[i].is_used)
            printf("Warning: Variable '%s' declared but never used\n", table[i].name);
        if (!table[i].is_initialized && table[i].is_used)
            printf("Warning: Variable '%s' used but may not be initialized\n", table[i].name);
    }
}

int main(void) {
    
    lineCount = 1;
    
    int result = yyparse();
    
    check_unused_variables();
    
    printf("\nProgram execution complete.\n");
    return result;
}

void yyerror(const char *s) {
    fprintf(stderr, "LINE %d ERROR: %s\n", lineCount, s);
}