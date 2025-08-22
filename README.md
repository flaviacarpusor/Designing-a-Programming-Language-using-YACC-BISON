## Project Description

This project implements a simple compiler using Flex (Lex) and Bison (Yacc) for a custom programming language. The compiler performs lexical analysis, syntax analysis, and basic semantic checks. It supports user-defined classes, global variables, functions, and a main section. The language includes features such as variable declarations, arrays, constants, arithmetic and logical expressions, control flow statements (`if`, `while`, `do-while`, `for`), and built-in functions like `Print` and `TypeOf`.

The main components are:
- **Lexical Analyzer (`compiler.l`)**: Defines tokens for keywords, operators, identifiers, literals, and handles line counting.
- **Syntax Analyzer (`compiler.y`)**: Specifies grammar rules for the language, builds an Abstract Syntax Tree (AST), and performs semantic actions such as variable/function/class registration and type checking.
- **Header (`compiler.hpp`)**: Contains data structures for variables, functions, classes, enums for types, and utility functions for semantic analysis and AST evaluation.

## Build and Run Instructions

To build and run the compiler, use the following commands:

```bash
flex -o lex.yy.cpp compiler.l
bison -d -o compiler.tab.cpp compiler.y
g++ lex.yy.cpp compiler.tab.cpp -o compiler -lfl
./compiler inputCorrect.txt
```

- `flex -o lex.yy.cpp compiler.l`  
  Generates the lexical analyzer source file from `compiler.l`.

- `bison -d -o compiler.tab.cpp compiler.y`  
  Generates the parser source and header files from `compiler.y`.

- `g++ lex.yy.cpp compiler.tab.cpp -o compiler -lfl`  
  Compiles the generated C++ files and links the Flex library.

- `./compiler inputCorrect.txt`  
  Runs the compiler on the provided input file.

## Features Implemented

- Lexical analysis for all language tokens.
- Syntax analysis for class definitions, variable and function declarations, main section, and control flow.
- Semantic checks for variable, function, and class declarations.
- AST construction and evaluation for expressions.
- Built-in functions: `Print` and `TypeOf`.
- Error reporting with line numbers.
- Output of function information to `functions.txt`.
