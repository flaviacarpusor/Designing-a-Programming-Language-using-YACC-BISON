%{
#include <iostream>
#include <fstream>
#include <string>
#include "compiler.hpp"   // Aici avem structurile, enum class Category, functiile etc.

extern FILE* yyin;
extern char* yytext;
extern int yylineno;
extern int yylex();
void yyerror(const char * s);
%}

%union {
    char* string;
    int int_val;
    float float_val;
    struct AST* tree;
}

/* Listam TOATE token-urile */
%token <string> RETURN CLASS CONST MAIN PRINT  TYPEOF
%token <string> TYPE VOID ID ASSIGN VAR_CHAR VAR_STRING
%token <int_val> VAR_INT
%token <float_val> VAR_FLOAT
%token <string> VAR_BOOL
%token <string> LESS GR LEQ GEQ 
%token <string> EQ NEQ
%token <string> NOT AND OR
%token <string> IF ELSE FOR DO WHILE

%type <tree> EXPR
%type <tree> COND
%type <string> LVALUE

%start progr

%left OR
%left AND
%left EQ NEQ
%left LESS GR LEQ GEQ
%left '+' '-'
%left '*' '/' '%'

%%

progr 
  : SECTIONS 
    {
      std::cout << "The program is correct!" << std::endl;
    }
  | SECTIONS error 
    {
      std::cout << "Unexpected text after end of program!" << std::endl;
    }
  ;

SECTIONS 
  : SECT1_USER_DEFINED_DATA SECTIONS
  | SECT2_GLOBAL_VARIABLES SECTIONS
  | SECT3_GLOBAL_FUNCTIONS SECTIONS
  | SECT4_MAIN
  ;

SECT1_USER_DEFINED_DATA 
  : USER_DEFINED_TYPE
  ;

USER_DEFINED_TYPE 
  : CLASS ID 
    {
      // domain si functionDomain sunt std::string
      domain = $2; 
      addClass($2, yylineno);
      functionDomain = $2;
    }
    '{' INSIDE_CLASS '}' ';'
    {
      domain = "global";
      functionDomain = "global";
    }
  ;

INSIDE_CLASS 
  : VAR_DECL ';' INSIDE_CLASS
  | FUNC_DECL INSIDE_CLASS
  | /* epsilon */
  ;

SECT2_GLOBAL_VARIABLES 
  : VAR_DECL ';'
  ;

SECT3_GLOBAL_FUNCTIONS 
  : FUNC_DECL
  ;

SECT4_MAIN 
  : TYPE MAIN 
    {
      domain = "main";
    }
    '(' ')' '{' INSTR_LIST '}' 
  ;


/* =============== VAR_DECL =============== */
VAR_DECL 
  : TYPE ID 
    {
      AST* aux = buildTree(
          (std::string($1) == "bool") ? "false" : "0",
          convertStringToEnum($1),
          nullptr, nullptr, yylineno
      );
      addVar($1, $2, evaluateTree(aux, yylineno), domain, false, yylineno);
    }
  | CONST TYPE ID 
    {
      AST* aux = buildTree(
          (std::string($2) == "bool") ? "false":"0",
          convertStringToEnum($2),
          nullptr, nullptr, yylineno
      );
      addVar($2, $3, evaluateTree(aux, yylineno), domain, true, yylineno);
    }
  | TYPE ID ASSIGN EXPR 
    {
      addVar($1, $2, evaluateTree($4, yylineno), domain, false, yylineno);
    }
  | CONST TYPE ID ASSIGN EXPR
    {
      addVar($2, $3, evaluateTree($5, yylineno), domain, true, yylineno);
    }
  | TYPE ID '[' EXPR ']'
    {
      int size = checkSize(evaluateTree($4, yylineno), yylineno);
      addArray($1, $2, size, domain, false, yylineno);
    }
  | ID ID 
    {
      checkClass($1, yylineno);
      AST* aux = buildTree("0", Category::OTHER, nullptr, nullptr, yylineno);
      addVar($1, $2, evaluateTree(aux, yylineno), domain, false, yylineno);
    }
  | TYPE ID '[' EXPR ']' '[' EXPR ']' 
  | TYPE ID '[' EXPR ']' '[' EXPR ']' '[' EXPR ']' 
  | TYPE ID '[' EXPR ']' '[' EXPR ']' '[' EXPR ']' '[' EXPR ']' 
  ;

/* =============== FUNC_DECL =============== */
FUNC_DECL 
  : TYPE ID 
    {
      domain = $2;
    }
    '(' PARAM_LIST ')' '{' INSTR_LIST '}' 
    {
      addFunction($1, $2, functionDomain, yylineno);
      domain = "global";
    }
  ;

PARAM_LIST
  : VAR_DECL { addParameter(); } ',' PARAM_LIST
  | VAR_DECL { addParameter(); }
  | /* epsilon */
  ;

/* =============== INSTR_LIST =============== */
INSTR_LIST 
  : /* epsilon */
  | INSTR_LIST VAR_DECL ';'
  | INSTR_LIST INSTR ';'
  | INSTR_LIST if
  | INSTR_LIST while
  | INSTR_LIST do
  | INSTR_LIST for
  | INSTR_LIST RETURN EXPR ';'
    {
      // Ex: return expr;
      // Aici doar parsam; interpretarea e la execuție
    }
  | INSTR_LIST PRINT '(' EXPR ')' ';'
    {
      // De ex. apelezi o funcție Print(...) dacă ai definit-o
      Print(evaluateTree($4, yylineno), yylineno);
    }
  | INSTR_LIST TYPEOF '(' EXPR ')' ';'
    {
      TypeOf(evaluateTree($4, yylineno), yylineno);
    }
  ;

/* =============== INSTR =============== */
INSTR 
  : LVALUE ASSIGN EXPR 
    {
      updateVarValue(lvalue, evaluateTree($3, yylineno), yylineno);
    }
  | EXPR
    {
      // ex: apel de funcție
    }
  ;

/* LVALUE = variabila, array[idx], obj.field, etc. */
LVALUE
  : ID
    {
      lvalue = $1;
    }
  | ID '[' EXPR ']'
    {
      lvalue = std::string($1) + "[" + evaluateTree($3, yylineno).resultStr + "]";
    }
  | ID '.' ID
    {
      // obj.field => lvalue = "obj.field" (dacă vrei)
      lvalue = std::string($1) + "." + std::string($3);
    }
  ;

/* =============== EXPR =============== */
EXPR 
  : EXPR '+' EXPR 
    {
      $$ = buildTree("+", Category::OPERATOR, $1, $3, yylineno);
    }
  | EXPR '-' EXPR 
    {
      $$ = buildTree("-", Category::OPERATOR, $1, $3, yylineno);
    }
  | EXPR '*' EXPR
    {
      $$ = buildTree("*", Category::OPERATOR, $1, $3, yylineno);
    }
  | EXPR '/' EXPR
    {
      $$ = buildTree("/", Category::OPERATOR, $1, $3, yylineno);
    }
  | EXPR '%' EXPR
    {
      $$ = buildTree("%", Category::OPERATOR, $1, $3, yylineno);
    }
  | '(' EXPR ')'
    {
      $$ = $2;
    }
  | VAR_INT
    {
      $$ = buildTree(itoaCustom($1), Category::NUMBER_INT, nullptr, nullptr, yylineno);
    }
  | VAR_FLOAT
    {
      $$ = buildTree(ftoaCustom($1), Category::NUMBER_FLOAT, nullptr, nullptr, yylineno);
    }
  | VAR_BOOL
    {
      $$ = buildTree($1, Category::NUMBER_BOOL, nullptr, nullptr, yylineno);
    }
  | VAR_CHAR
    {
      $$ = buildTree($1, Category::CHAR, nullptr, nullptr, yylineno);
    }
  | VAR_STRING
    {
      $$ = buildTree($1, Category::STRING, nullptr, nullptr, yylineno);
    }
  | EXPR AND EXPR
    {
      $$ = buildTree("&&", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR OR EXPR
    {
      $$ = buildTree("||", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR LESS EXPR
    {
      $$ = buildTree("<", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR GR EXPR
    {
      $$ = buildTree(">", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR LEQ EXPR
    {
      $$ = buildTree("<=", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR GEQ EXPR
    {
      $$ = buildTree(">=", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR EQ EXPR
    {
      $$ = buildTree("==", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | EXPR NEQ EXPR
    {
      $$ = buildTree("!=", Category::NUMBER_BOOL, $1, $3, yylineno);
    }
  | NOT '(' EXPR ')'
    {
      $$ = buildTree("!", Category::NUMBER_BOOL, $3, nullptr, yylineno);
    }
  | ID '(' ARGS_LIST ')'
    {
      compareParamWithArgs($1, args, "global", yylineno);
      $$ = buildTree($1, convertStringToEnum(getFuncType($1)), nullptr, nullptr, yylineno);
    }
  | ID
    {
      $$ = buildTree($1, Category::IDENTIFIER, nullptr, nullptr, yylineno);
    }
  | ID '[' EXPR ']'
    {
      std::string rvalue = std::string($1) + "[" + evaluateTree($3, yylineno).resultStr + "]";
      $$ = buildTree(rvalue, Category::IDENTIFIER, nullptr, nullptr, yylineno);
    }
  | ID '.' ID '(' ARGS_LIST ')'
    {
      compareParamWithArgs($3, args, getTypeOfObject($1, yylineno), yylineno);
      isMemberInClass($1, $3, yylineno);
      $$ = buildTree($3, Category::OTHER, nullptr, nullptr, yylineno);
    }
  | ID '.' ID
    {
      isIdInClass($1, $3, yylineno);
      $$ = buildTree("0", Category::OTHER, nullptr, nullptr, yylineno);
    }
  ;

/* Conditiile if(...) / while(...) etc. */
COND 
  : EXPR
  ;

/* Instructiuni de control */
if 
  : IF '(' COND ')' '{' INSTR_LIST '}'
  | IF '(' COND ')' '{' INSTR_LIST '}' ELSE '{' INSTR_LIST '}'
  ;

while 
  : WHILE '(' COND ')' '{' INSTR_LIST '}'
  ;

do 
  : DO '{' INSTR_LIST '}' WHILE '(' COND ')' ';'
  ;

for 
  : FOR '(' INSTR ';' COND ';' INSTR ')' '{' INSTR_LIST '}'
  ;

/* ARGS_LIST = argumentele unui apel de funcție */
ARGS_LIST
  : EXPR ',' 
    {
      // fiindcă args e std::string, facem concatenare
      args += convertEnumToString(evaluateTree($1, yylineno).treeType);
      args += ", ";
    }
    ARGS_LIST
  | EXPR
    {
      args += convertEnumToString(evaluateTree($1, yylineno).treeType);
    }
  | /* epsilon */
    {
      args += "-";
    }
  ;

%%

void yyerror(const char * s) {
    std::cerr << "{error} " << s << " at line: " << yylineno << " :(" << std::endl;
}


int main(int argc, char **argv) {
    
    std::ofstream ffunc("functions.txt");

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
    }

    yyparse();

  
    printFunc(ffunc);

    
    ffunc.close();
    return 0;
}
