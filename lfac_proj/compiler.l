%{
#include <stdio.h>
#include <stdlib.h>
#include "compiler.tab.hpp"
%}
%option noyywrap
%%
"main" { return MAIN; }
"Print" { return PRINT; }  
"TypeOf" { return TYPEOF; }
"==" { return EQ; }
"!=" { return NEQ; }
"<=" { return LEQ; }
">=" { return GEQ; }
"<" { return LESS; }
">" { return GR; }
"not" { return NOT;}
"&&" { return AND; }
"||" { return OR; }
"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
"do" { return DO; }
"for" { return FOR; }
"class" { return CLASS; }
"return" { return RETURN; }
"int"|"float"|"char"|"string"|"bool" { yylval.string = strdup(yytext); return TYPE; }
-?[1-9][0-9]*|0 { yylval.int_val = atoi(yytext); return VAR_INT; }
"void" { yylval.string = strdup(yytext); return VOID; }
"true"|"false" { yylval.string = strdup(yytext); return VAR_BOOL; }
\"[ _a-zA-Z0-9]+\" { yylval.string = strdup(yytext); yylval.string++; yylval.string[strlen(yylval.string) - 1] = 0; return VAR_STRING; }
\'[ _a-zA-Z0-9]\' { yylval.string = strdup(yytext); yylval.string++; yylval.string[strlen(yylval.string) - 1] = 0; return VAR_CHAR; }
-?([1-9][0-9]*\.[0-9]+|0\.[0-9]+) { yylval.float_val = atof(yytext); return VAR_FLOAT;}
"const" { return CONST; }
[_a-zA-Z][_a-zA-Z0-9]* { yylval.string = strdup(yytext); return ID; }
"=" { yylval.string = strdup(yytext); return ASSIGN; }
[ \t] ;
\n { yylineno++; }
. { return yytext[0]; }

