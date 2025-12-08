%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "y.tab.h"
%}

%option noyywrap
%option yylineno

%%

[ \t]+          ; 
\/\/.* ;  
\n              { return NEWLINE; }

","             { return COMMA; }
";"             { return SEMICOLON; }
"="             { return ASSIGN; }

"++"            { return INCREMENT; }
"--"            { return DECREMENT; }

"+="            { return PLUS_ASSIGN; }
"-="            { return MINUS_ASSIGN; }
"*="            { return MULTIPLY_ASSIGN; }
"/="            { return DIVIDE_ASSIGN; }

"+"             { return PLUS; }
"-"             { return MINUS; }
"*"             { return MULTIPLY; }
"/"             { return DIVIDE; }

"("             { return LPAREN; }
")"             { return RPAREN; }

ilimbag         { return ilimbag; }
numero          { return numero; }
sulat           { return sulat; }
letra           { return letra; }
desimal         { return desimal; }

\"([^"\\]|\\.)*\"  { 
    yylval.str = strdup(yytext); 
    return STRING_LITERAL; 
}

\'([^'\\]|\\.)\'   { 
    yylval.str = strdup(yytext); 
    return CHARACTER; 
} 

[0-9]+          { 
    yylval.num = atoi(yytext); 
    return INTEGER; 
}        

[0-9]*\.[0-9]+  { 
    yylval.float_num = atof(yytext); 
    return FLOAT; 
} 

[0-9]+\.[0-9]* { 
    yylval.float_num = atof(yytext); 
    return FLOAT; 
} 

[a-zA-Z_][a-zA-Z0-9_]* {  
    yylval.str = strdup(yytext);
    return IDENTIFIER;
}

.               {  
    fprintf(stderr, "LEXER ERROR: Invalid character '%c' at line %d\n", *yytext, yylineno);
    exit(1);
}

%%