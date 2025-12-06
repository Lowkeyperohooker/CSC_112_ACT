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
\'([^'\\]|\\.)*\'        {  
    char *text = yytext;
    if (strlen(text) >= 3) {
        if (text[1] == '\\') {
            switch(text[2]) {
                case 'n': yylval.num = '\n'; break;
                case 't': yylval.num = '\t'; break;
                case 'r': yylval.num = '\r'; break;
                case '0': yylval.num = '\0'; break;
                case '\\': yylval.num = '\\'; break;
                case '\'': yylval.num = '\''; break;
                default: yylval.num = text[2]; break;
            }
        } else {
            yylval.num = text[1];
        }
    } else {
        yylval.num = 0;
    }
    return CHAR_LITERAL;
}

[0-9]+                   {  yylval.num = atoi(yytext); return INTEGER; }

[a-zA-Z_][a-zA-Z0-9_]*   {  
    yylval.str = strdup(yytext); 
    return IDENTIFIER; 
}

.                        {  
    fprintf(stderr, "LINE %d: Invalid character '%c'\n", line_no, yytext[0]);
    return ERROR; 
}

%%