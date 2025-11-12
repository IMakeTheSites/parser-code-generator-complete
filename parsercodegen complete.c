/*
Assignment:
HW4 - Complete Parser and Code Generator for PL/0
(with Procedures, Call, and Else)
Author(s): Dean Walker, Mark Wlodawski
Language: C (only)
To Compile:
Scanner:
gcc -O2 -std=c11 -o lex lex.c
Parser/Code Generator:
gcc -O2 -std=c11 -o parsercodegen_complete parsercodegen_complete.c
Virtual Machine:
gcc -O2 -std=c11 -o vm vm.c
To Execute (on Eustis):
./lex <input_file.txt>
./parsercodegen_complete
./vm elf.txt
where:
<input_file.txt> is the path to the PL/0 source program
Notes:
- lex.c accepts ONE command-line argument (input PL/0 source file)
- parsercodegen_complete.c accepts NO command-line arguments
- Input filename is hard-coded in parsercodegen_complete.c
- Implements recursive-descent parser for extended PL/0 grammar
- Supports procedures, call statements, and if-then-else
- Generates PM/0 assembly code (see Appendix A for ISA)
- VM must support EVEN instruction (OPR 0 11)
- All development and testing performed on Eustis
Class: COP3402 - System Software - Fall 2025
Instructor: Dr. Jie Lin
Due Date: Friday, November 21, 2025 at 11:59 PM ET
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CODE 1000
#define MAX_SYMBOL_TABLE_SIZE 500
#define MAX_TOKEN_LEN 64
#define MAX_NAME_LEN 12
// Add a define for MAX_LEXI_LEVELS
#define MAX_LEXI_LEVELS 3

typedef struct
{
    int type;             
    char lexeme[MAX_TOKEN_LEN]; 
} Token;

Token tokens[5000];
int tokenCount = 0;
int cur = 0; // current token index

typedef struct
{
    int kind;       /* 1 = const 2 = var 3 = proc (not used) */
    char name[MAX_NAME_LEN];
    int val;
    int level;
    int addr;
    int mark;
} symbol;

symbol symbol_table[MAX_SYMBOL_TABLE_SIZE];
int symCount = 0;

/* --- PM/0 code representation --- */
typedef struct
{
    int op;
    int l;
    int m;
} instruction;

instruction code[MAX_CODE];
int codeIndex = 0;

/*P machine opcodes(use numeric values from Appendix A) */
enum
{
    OP_LIT = 1,
    OP_OPR = 2,
    OP_LOD = 3,
    OP_STO = 4,
    OP_CAL = 5,
    OP_INC = 6,
    OP_JMP = 7,
    OP_JPC = 8,
    OP_SYS = 9
};

/* OPR subcodes from Appendix A (when emitting OPR) */
enum
{
    OPR_RET = 0,
    OPR_ADD = 1,
    OPR_SUB = 2,
    OPR_MUL = 3,
    OPR_DIV = 4,
    OPR_EQL = 5,
    OPR_NEQ = 6,
    OPR_LSS = 7,
    OPR_LEQ = 8,
    OPR_GTR = 9,
    OPR_GEQ = 10,
    OPR_EVEN = 11
};

// Prototypes 
void emit(int op, int l, int m);
int symbolTableCheck(const char* name, int level);
int findSymbol(int level, const char* name);
int addSymbol(int kind, const char* name, int val, int level, int addr);
void loadTokensFromFile(const char* filename);
void markSymbols(int level);
Token* peekToken();
Token* nextToken();
void syntaxError(const char* msg);
void writeElfOrError(const char* msg);
void printAssemblyAndSymbolTable();
void init();
void program();
void block(int level);
int var_declaration(int level); 
void const_declaration(int level);
void procdure_declaration(int level);
void statement(int level);
void condition(int level);
void expression(int level);
void term(int level);
void factor(int level);

void emit(int op, int l, int m)
{
    if (codeIndex >= MAX_CODE)
    {
        fprintf(stderr, "Internal: code overflow\n");
        exit(1);
    }
    code[codeIndex].op = op;
    code[codeIndex].l = l;
    code[codeIndex].m = m;
    codeIndex++;
}

/*
Update the following for lexical scoping. Should have a symbol at the current level only in order to 
check for errors*/
int symbolTableCheck(const char* name, int level)
{
    for (int i = 0; i < symCount; ++i)
    {
        if (strcmp(symbol_table[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// New helper function when we need to find symbols
int findSymbol(const char* name, int level) {}

// New functino for scoping

void markSymbols(int level) {}

int addSymbol(int kind, const char* name, int val, int level, int addr)
{
    if (symCount >= MAX_SYMBOL_TABLE_SIZE)
    {
        fprintf(stderr, "Symbol table overflow\n");
        exit(1);
    }
    strncpy(symbol_table[symCount].name, name, MAX_NAME_LEN - 1);
    symbol_table[symCount].name[MAX_NAME_LEN - 1] = '\0';
    symbol_table[symCount].kind = kind;
    symbol_table[symCount].val = val;
    symbol_table[symCount].level = level;
    symbol_table[symCount].addr = addr;
    symbol_table[symCount].mark = 0;
    return symCount++;
}

/* Load tokens from tokens.txt.one token per line:
   tokenType [lexeme]
   Adjust parsing here to match your lex.c output format.
*/
void loadTokensFromFile(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f)
    {
        perror("fopen tokens file");
        exit(1);
    }
    tokenCount = 0;
    while (!feof(f) && tokenCount < (int)(sizeof(tokens) / sizeof(tokens[0])))
    {
        int t;
        if (fscanf(f, "%d", &t) != 1)
        {
            break;
        }
        tokens[tokenCount].type = t;
        tokens[tokenCount].lexeme[0] = '\0';
        /* If token is identifier (assume type 2) or number (assume type 3), read lexeme */
        /* TODO: change token numbers to match your lexer's token numbering */
        if (t == 2 || t == 3)
        {
            if (fscanf(f, " %63s", tokens[tokenCount].lexeme) != 1)
            {
                tokens[tokenCount].lexeme[0] = '\0';
            }
        }
        tokenCount++;
    }
    fclose(f);
}

/* Peek current token; return pointer or NULL if EOF */
Token* peekToken()
{
    if (cur >= tokenCount)
    {
        return NULL;
    }
    return &tokens[cur];
}

/* Advance and return previous token */
Token* nextToken()
{
    if (cur >= tokenCount)
    {
        return NULL;
    }
    return &tokens[cur++];
}

/* Error handling: print message to terminal and write the same message to elf.txt then exit */
void syntaxError(const char* msg)
{
    printf("Error: %s\n", msg);
    writeElfOrError(msg);
    exit(1);
}

void writeElfOrError(const char* msg)
{
    FILE* f = fopen("elf.txt", "w");
    if (!f)
    {
        return;
    }
    fprintf(f, "Error: %s\n", msg);
    fclose(f);
}

/* Print assembly in mnemonic format and symbol table to terminal, and numeric code to elf.txt */
void printAssemblyAndSymbolTable()
{
    printf("Assembly Code:\n\nLine OP L M\n\n");
	// Print code in format
    for (int i = 0; i < codeIndex; ++i)
    {
        const char* opname = "???";
        switch (code[i].op)
        {
        case OP_LIT:
            opname = "LIT";
            break;
        case OP_OPR:
            opname = "OPR";
            break;
        case OP_LOD:
            opname = "LOD";
            break;
        case OP_STO:
            opname = "STO";
            break;
        case OP_CAL:
            opname = "CAL";
            break;
        case OP_INC:
            opname = "INC";
            break;
        case OP_JMP:
            opname = "JMP";
            break;
        case OP_JPC:
            opname = "JPC";
            break;
        case OP_SYS:
            opname = "SYS";
            break;
        }
        printf("%d %s %d %d\n\n", i, opname, code[i].l, code[i].m);
    }

    printf("Symbol Table:\n\nKind | Name | Value | Level | Address | Mark\n\n");
	// Print symbol table in format
    for (int i = 0; i < symCount; ++i)
    {
        printf("%d|%s|%d|%d|%d|%d\n", symbol_table[i].kind, symbol_table[i].name,
            symbol_table[i].val, symbol_table[i].level, symbol_table[i].addr, symbol_table[i].mark);
    }

    FILE* f = fopen("elf.txt", "w");
    if (!f)
    {
        perror("fopen elf.txt");
        return;
    }
    for (int i = 0; i < codeIndex; ++i)
    {
        fprintf(f, "%d %d %d\n", code[i].op, code[i].l, code[i].m);
    }
    fclose(f);
}

// Initialize parser state
void init()
{
    codeIndex = 0;
    symCount = 0;
    cur = 0;
    emit(OP_JMP, 0, 3);
}

// symbol defs (must match lex.c TokenType enum)
#define T_SKIPSYM 1
#define T_IDENT   2
#define T_NUMBER  3
#define T_PLUS    4
#define T_MINUS   5
#define T_MULT    6
#define T_DIV     7
#define T_EQ      8
#define T_NEQ     9
#define T_LT      10
#define T_LE      11
#define T_GT      12
#define T_GE      13
#define T_LPAREN  14
#define T_RPAREN  15
#define T_COMMA   16
#define T_SEMI    17
#define T_PERIOD  18
#define T_ASSIGN  19
#define T_BEGIN   20
#define T_END     21
#define T_IF      22
#define T_FI      23
#define T_THEN    24
#define T_WHILE   25
#define T_DO      26
#define T_CALL    27
#define T_CONST   28
#define T_VAR     29
#define T_PROC    30
#define T_WRITE   31
#define T_READ    32
#define T_ELSE    33
#define T_EVEN    34

/* program ::= block "." */
void program()
{
    Token* tk;
    block();
    tk = peekToken();
    if (!tk || tk->type != T_PERIOD)
    {
        syntaxError("program must end with period");
    }
    nextToken();
    /* Emit HALT: SYS 0 3 */
    emit(OP_SYS, 0, 3);
}

/* block ::= const-declaration var-declaration statement */
void block()
{
    const_declaration();
    int nVars = var_declaration();
    emit(OP_INC, 0, 3 + nVars);
    statement();
}

/* const-declaration ::= [ "const" ident "=" number {"," ident "=" number} ";" ] */
void const_declaration()
{
    Token* tk = peekToken();
    if (!tk)
    {
        return;
    }
    if (tk->type == T_CONST)
    {
        nextToken();
		// parse const declarations
        while (1)
        {
            Token* t = peekToken();
            if (!t || t->type != T_IDENT)
            {
                syntaxError("const, var, and read keywords must be followed by identifier");
            }
            char name[MAX_NAME_LEN];
            strncpy(name, t->lexeme, MAX_NAME_LEN - 1);
            name[MAX_NAME_LEN - 1] = '\0';
            if (symbolTableCheck(name) != -1)
            {
                syntaxError("symbol name has already been declared");
            }
            nextToken();
            t = peekToken();
            if (!t || t->type != T_EQ)
            {
                syntaxError("constants must be assigned with =");
            }
            nextToken();
            t = peekToken();
            if (!t || t->type != T_NUMBER)
            {
                syntaxError("constants must be assigned an integer value");
            }
            int val = atoi(t->lexeme);
            addSymbol(1, name, val, 0, 0);
            nextToken();
            t = peekToken();
            if (!t)
            {
                syntaxError("constant and variable declarations must be followed by a semicolon");
            }
            if (t->type == T_COMMA)
            {
                nextToken();
                continue;
            }
            if (t->type == T_SEMI)
            {
                nextToken();
                break;
            }
            syntaxError("constant and variable declarations must be followed by a semicolon");
        }
    }
}

/* var-declaration ::= [ "var" ident {"," ident} ";" ] */
int var_declaration()
{
    Token* tk = peekToken();
    int numVars = 0;
    if (!tk)
    {
        return 0;
    }
    if (tk->type == T_VAR)
    {
        nextToken();
		// parse variable declarations
        while (1)
        {
            Token* t = peekToken();
            if (!t || t->type != T_IDENT)
            {
                syntaxError("const, var, and read keywords must be followed by identifier");
            }
            char name[MAX_NAME_LEN];
            strncpy(name, t->lexeme, MAX_NAME_LEN - 1);
            name[MAX_NAME_LEN - 1] = '\0';
            if (symbolTableCheck(name) != -1)
            {
                syntaxError("symbol name has already been declared");
            }
            int addr = 3 + numVars;

            addSymbol(2, name, 0, 0, addr);
            numVars++;
            nextToken();
            t = peekToken();
            if (!t)
            {
                syntaxError("constant and variable declarations must be followed by a semicolon");
            }
            if (t->type == T_COMMA)
            {
                nextToken();
                continue;
            }
            if (t->type == T_SEMI)
            {
                nextToken();
                break;
            }
            syntaxError("constant and variable declarations must be followed by a semicolon");
        }
    }
    return numVars;
}

/* statement ::= many alternatives including empty */
void statement()
{
    Token* t = peekToken();
    if (!t)
    {
        return;
    }
    if (t->type == T_IDENT)
    {
        char name[MAX_NAME_LEN];
        strncpy(name, t->lexeme, MAX_NAME_LEN - 1);
        name[MAX_NAME_LEN - 1] = '\0';
        int idx = symbolTableCheck(name);
        if (idx == -1)
        {
            syntaxError("undeclared identifier");
        }
        if (symbol_table[idx].kind != 2)
        {
            syntaxError("only variable values may be altered");
        }
        nextToken();
        t = peekToken();
        if (!t || t->type != T_ASSIGN)
        {
            syntaxError("assignment statements must use :=");
        }
        nextToken();
        expression();
        emit(OP_STO, 0, symbol_table[idx].addr);
        symbol_table[idx].mark = 1;
        return;
    }
    // 
    else if (t->type == T_BEGIN)
    {
        nextToken();
        statement();
        while (peekToken() && peekToken()->type == T_SEMI)
        {
            nextToken();
            statement();
        }
        if (!peekToken() || peekToken()->type != T_END)
        {
            syntaxError("begin must be followed by end");
        }
        nextToken();
        return;
    }
    else if (t->type == T_IF)
    {
        nextToken(); // "if" must be consumed
        condition();
        int jpcIdx = codeIndex;
        emit(OP_JPC, 0, 0);
        if (!peekToken() || peekToken()->type != T_THEN)
            syntaxError("if must be followed by then");
        nextToken();
        statement();

        if (!peekToken() || peekToken()->type != T_FI)
            syntaxError("if statement must end with fi");
        nextToken();

        code[jpcIdx].m = codeIndex;
        return;
    }
    else if (t->type == T_WHILE)
    {
        nextToken();
        int loopIdx = codeIndex;
        condition();
        if (!peekToken() || peekToken()->type != T_DO)
        {
            syntaxError("while must be followed by do");
        }
        nextToken();
        int jpcIdx = codeIndex;
        emit(OP_JPC, 0, 0);
        statement();
        emit(OP_JMP, 0, loopIdx);
        code[jpcIdx].m = codeIndex;
        return;
    }
    else if (t->type == T_READ)
    {
        nextToken();
		// expect identifier
        if (!peekToken() || peekToken()->type != T_IDENT)
        {
            syntaxError("const, var, and read keywords must be followed by identifier");
        }
        char name[MAX_NAME_LEN];
        strncpy(name, peekToken()->lexeme, MAX_NAME_LEN - 1);
        name[MAX_NAME_LEN - 1] = '\0';
        int idx = symbolTableCheck(name);
		// check identifier exists
        if (idx == -1)
        {
            syntaxError("undeclared identifier");
        }
        if (symbol_table[idx].kind != 2)
        {
            syntaxError("only variable values may be altered");
        }
        nextToken();
        emit(OP_SYS, 0, 2);
        emit(OP_STO, 0, symbol_table[idx].addr);
        symbol_table[idx].mark = 1;
        return;
    }
    else if (t->type == T_WRITE)
    {
        nextToken();
        expression();
        emit(OP_SYS, 0, 1);
        return;
    }
    else
    {
        return;
    }
}

/* condition ::= "even" expression | expression rel-op expression */
void condition()
{
    Token* t = peekToken();
    if (!t)
    {
        syntaxError("condition must contain comparison operator");
    }
    /* If your scanner has an EVEN token, handle it here. otherwise parse expression relop expression */
    /* Example placeholder: assume no EVEN token in scanner */
    expression();
    Token* op = peekToken();
    if (!op)
    {
        syntaxError("condition must contain comparison operator");
    }
    int rel = -1;
	// handle relational operators
    if (op->type == T_EQ)
    {
        rel = OPR_EQL;
    }
    else if (op->type == T_NEQ)
    {
        rel = OPR_NEQ;
    }
    else if (op->type == T_LT)
    {
        rel = OPR_LSS;
    }
    else if (op->type == T_LE)
    {
        rel = OPR_LEQ;
    }
    else if (op->type == T_GT)
    {
        rel = OPR_GTR;
    }
    else if (op->type == T_GE)
    {
        rel = OPR_GEQ;
    }
    else
    {
        syntaxError("condition must contain comparison operator");
    }
    nextToken();
    expression();
    emit(OP_OPR, 0, rel);
}

/* expression ::= term { ("+" | "-") term } */
void expression()
{
    Token* t = peekToken();
    int unaryMinus = 0;
    if (t && t->type == T_MINUS)
    {
        unaryMinus = 1;
        nextToken();
    }
    else if (t && t->type == T_PLUS)
    {
        nextToken();
    }
    term();
    if (unaryMinus)
    {
        /* Unary minus: push 0 then subtract top */
        emit(OP_LIT, 0, 0);
        emit(OP_OPR, 0, OPR_SUB);
    }
    while (peekToken() && (peekToken()->type == T_PLUS || peekToken()->type == T_MINUS))
    {
		// handle addition and subtraction
        int op = peekToken()->type;
        nextToken();
        term();
        if (op == T_PLUS)
        {
            emit(OP_OPR, 0, OPR_ADD);
        }
        else
        {
            emit(OP_OPR, 0, OPR_SUB);
        }
    }
}

/* term ::= factor { ("*" | "/") factor } */
void term()
{
    factor();
	// handle multiplication and division
    while (peekToken() && (peekToken()->type == T_MULT || peekToken()->type == T_DIV))
    {
        int op = peekToken()->type;
        nextToken();
        factor();
        if (op == T_MULT)
        {
            emit(OP_OPR, 0, OPR_MUL);
        }
        else
        {
            emit(OP_OPR, 0, OPR_DIV);
        }
    }
}

/* factor ::= ident | number | "(" expression ")" */
void factor()
{
    Token* t = peekToken();
    if (!t)
    {
        syntaxError("arithmetic equations must contain operands, parentheses, numbers, or symbols");
    }
    // id
    if (t->type == T_IDENT)
    {
        int idx = symbolTableCheck(t->lexeme);
        if (idx == -1)
        {
            syntaxError("undeclared identifier");
        }
        if (symbol_table[idx].kind == 1)
        {
            emit(OP_LIT, 0, symbol_table[idx].val);
        }
        else if (symbol_table[idx].kind == 2)
        {
            emit(OP_LOD, 0, symbol_table[idx].addr);
            symbol_table[idx].mark = 1;
        }
        else
        {
            syntaxError("arithmetic equations must contain operands, parentheses, numbers, or symbols");
        }
        nextToken();
    }
    // numeric literal
    else if (t->type == T_NUMBER)
    {
        emit(OP_LIT, 0, atoi(t->lexeme));
        nextToken();
    }
    else if (t->type == T_LPAREN)
    {
		nextToken(); // move from '('
		expression();   // parse expression inside parentheses
        if (!peekToken() || peekToken()->type != T_RPAREN)
        {
            syntaxError("right parenthesis must follow left parenthesis");
        }
        nextToken();
    }
    else
    {
        syntaxError("arithmetic equations must contain operands, parentheses, numbers, or symbols");
    }
}

int main(void)
{
    loadTokensFromFile("tokensPrint.txt");

    init();

    program();

    printAssemblyAndSymbolTable();
    return 0;
}
