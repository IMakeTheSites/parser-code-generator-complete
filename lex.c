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
*/

// Included imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// TokenType Enumeration in C
typedef enum {
    skipsym = 1,
    identsym,
    numbersym,
    plussym,
    minussym,
    multsym,
    slashsym,
    eqsym,
    neqsym,
    lessym,
    leqsym,
    gtrsym,
    geqsym,
    lparentsym,
    rparentsym,
    commasym,
    semicolonsym,
    periodsym,
    becomessym,
    beginsym,
    endsym,
    ifsym,
    fisym,
    thensym,
    whilesym,
    dosym,
    callsym,
    constsym,
    varsym,
    procsym,
    writesym,
    readsym,
    elsesym,
    evensym
} TokenType;

// Tokens and types
typedef struct {
    TokenType type;
    char lexeme[12];
    int number;
    int error; // 0=none, 1=id too long, 2=num too long, 3=invalid
} Token;

// Function definitions
int peek();
void skipComment();
Token scanIdent(int first);
Token scanSymbol(int first);
Token scanNumber(int first);
void scanAllTokens(void);
void printResults(const char* fname);

// Definitions and token count
#define MAXTOK 500
Token tokens[MAXTOK];
int tokenCount = 0;

// Keyword lookup struct
struct { const char* word; TokenType token; } kw[] = {
    {"begin", beginsym},
    {"end", endsym},
    {"if", ifsym},
    {"fi", fisym},
    {"then", thensym},
    {"while", whilesym},
    {"do", dosym},
    {"call", callsym},
    {"const", constsym},
    {"var", varsym},
    {"procedure", procsym},
    {"write", writesym},
    {"read", readsym},
    {"else", elsesym},
    {"even", evensym}
};
int kwCount = sizeof(kw) / sizeof(kw[0]);

// Special symbol lookup (single & double)
struct { const char* s; TokenType t; } syms[] = {
    {"+", plussym},
    {"-", minussym},
    {"*", multsym},
    {"/", slashsym},
    {"=", eqsym},
    {"<>", neqsym},
    {"<", lessym},
    {"<=", leqsym},
    {">", gtrsym},
    {">=", geqsym},
    {"(", lparentsym},
    {")", rparentsym},
    {",", commasym},
    {";", semicolonsym},
    {".", periodsym},
    {":=", becomessym}
};
int symCount = sizeof(syms) / sizeof(syms[0]);

// Main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file name>\n", argv[0]);
        return 1;
    }
    if (!freopen(argv[1], "r", stdin)) {
        perror("Error opening input file");
        return 1;
    }
    scanAllTokens();
    printResults(argv[1]);
    return 0;
}

// Peek function to reach next character
int peek() {
    int character = fgetc(stdin);
    if (character != EOF) {
        ungetc(character, stdin);
    }
    return character;
}

// Function to Skip /* ... */ comments
void skipComment() {
    int prev = 0;
    int character;
    while ((character = fgetc(stdin)) != EOF) {
        if (prev == '*' && character == '/') break;
        prev = character;
    }
}

// Scan an identifier or keyword
Token scanIdent(int first) {
    Token tk = { identsym, "", 0, 0 };
    char buf[16];
    int len = 0, c;
    buf[len++] = first;
    
    while (isalnum(c = fgetc(stdin))) {
        if (len < 11) buf[len++] = c;
        else tk.error = 1; // too long
    }
    if (c != EOF) ungetc(c, stdin);
    
    buf[len] = '\0';
    if (len > 11) buf[11] = '\0';
    strcpy(tk.lexeme, buf);
    
    // Check keyword
    for (int i = 0; i < kwCount; i++) {
        if (strcmp(buf, kw[i].word) == 0) {
            tk.type = kw[i].token;
            break;
        }
    }
    return tk;
}

// Scan a number
Token scanNumber(int first) {
    Token tk = { numbersym, "", 0, 0 };
    char buf[16]; 
    int len = 0, c;
    buf[len++] = first;
    
    while (isdigit(c = fgetc(stdin))) {
        if (len < 5) buf[len++] = c;
        else tk.error = 2; // too long
    }
    if (c != EOF) {
        ungetc(c, stdin);
    }
    buf[len] = '\0';
    strcpy(tk.lexeme, buf);
    tk.number = atoi(buf); 
    return tk;
}

// Scan a symbol or mark invalid
Token scanSymbol(int first) {
    Token tk = { skipsym, "", 0, 0 };
    char buf[3] = { first, 0, 0 };
    int c = fgetc(stdin);
    
    if (c != EOF) {
        buf[1] = c;
        buf[2] = '\0'; 
        
        for (int i = 0; i < symCount; i++) {
            if (strcmp(buf, syms[i].s) == 0) {
                tk.type = syms[i].t;
                strcpy(tk.lexeme, buf);
                return tk;
            }
        }
        ungetc(c, stdin);
        buf[1] = '\0';
    }
    
    // single-char
    for (int i = 0; i < symCount; i++) {
        if (strcmp(buf, syms[i].s) == 0) {
            tk.type = syms[i].t;
            strcpy(tk.lexeme, buf);
            return tk;
        }
    }
    
    // Mark as invalid
    tk.error = 3;
    strcpy(tk.lexeme, buf);
    return tk;
}

// Main scanning loop
void scanAllTokens(void) {
    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (isspace(ch)) continue;
        
        if (ch == '/' && peek() == '*') {
            fgetc(stdin); // consume '*'
            skipComment();
            continue;
        }
        
        Token t;
        if (isalpha(ch)) {
            t = scanIdent(ch);
        } else if (isdigit(ch)) {
            t = scanNumber(ch);
        } else {
            t = scanSymbol(ch);
        }
        
        if (tokenCount < MAXTOK) {
            tokens[tokenCount++] = t;
        } else {
            fprintf(stderr, "Error: token buffer overflow\n");
            break;
        }
    }
}

// Print the three required sections
void printResults(const char* fname) {
    // 1) Source Program
    printf("Source Program :\n\n");
    FILE* f = fopen(fname, "r");
    if (f) {
        int c;
        while ((c = fgetc(f)) != EOF) putchar(c);
        fclose(f);
    }

    // 2) Lexeme Table
    printf("\n\nLexeme Table :\n\n");
    printf("%-7s %-15s\n", "lexeme", "token type");
    for (int i = 0; i < tokenCount; i++) {
        Token* t = &tokens[i];
        if (t->error == 1) printf("%-15s Identifier too long\n", t->lexeme);
        else if (t->error == 2) printf("%-7s Number too long\n", t->lexeme);
        else if (t->error == 3) printf("%-15s Invalid symbol\n", t->lexeme);
        else printf("%-7s %-15d\n", t->lexeme, t->type);
    }

    // 3) Token List
    printf("\nToken List :\n\n");
    for (int i = 0; i < tokenCount; i++) {
        Token* t = &tokens[i];
        int code = t->error ? skipsym : t->type;
        
        printf("%d", code);
        if (!t->error && (code == identsym || code == numbersym)) {
            printf(" %s", t->lexeme);
        }
        if (i < tokenCount - 1) printf(" ");
    }
    printf("\n");
}