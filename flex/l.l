%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "y.tab.h"

void yyerror(const char *s);
int line_no = 1;
%}

%option noyywrap
%option yylineno

%x COMMENT

%%

[ \t]+                   ;   /* ignore spaces/tabs */
[\r]+                    ;   /* ignore carriage returns */
\n                       {  line_no++; return NEWLINE; }

"//".*                   ;   /* line comments */
"/*"                     {  BEGIN(COMMENT); }
<COMMENT>"*/"            {  BEGIN(INITIAL); }
<COMMENT>\n              {  line_no++; }
<COMMENT>.               ;
<COMMENT><<EOF>>         {  yyerror("Unterminated comment"); yyterminate(); }

"ilimbag"                {  return ilimbag; }
"numero"                 {  return numero; }
"sulat"                  {  return sulat; }

"+"                      {  return PLUS; }
"-"                      {  return MINUS; }
"*"                      {  return MULTIPLY; }
"/"                      {  return DIVIDE; }
"%"                      {  return MODULO; }

"++"                     {  return INCREMENT; }
"--"                     {  return DECREMENT; }

"="                      {  return ASSIGN; }
"+="                     {  return PLUS_ASSIGN; }
"-="                     {  return MINUS_ASSIGN; }
"*="                     {  return MULTIPLY_ASSIGN; }
"/="                     {  return DIVIDE_ASSIGN; }
"%="                     {  return MODULO_ASSIGN; }

"("                      {  return LPAREN; }
")"                      {  return RPAREN; }
","                      {  return COMMA; }
";"                      {  return SEMICOLON; }

\"([^"\\]|\\.)*\"        {  yylval.str = strdup(yytext); return STRING_LITERAL; }

[0-9]+                   {  yylval.num = atoi(yytext); return INTEGER; }

[a-zA-Z_][a-zA-Z0-9_]*   {  
    yylval.str = strdup(yytext); 
    return IDENTIFIER; 
}

.                        {  
    fprintf(stderr, "LINE %d: Invalid character '%c'\n", line_no, yytext[0]);
    return ERROR_CHAR;  /* Changed from ERROR to ERROR_CHAR */
}

%%