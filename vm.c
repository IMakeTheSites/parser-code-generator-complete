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

int PAS[500] = {0};

int BaseHelper(int bp, int l, int PAS[]);
void PrintStackTrace(int PC, int BP, int SP, int initialBP, int PAS[]);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Usage: %s input.txt\n", argv[0]);
        return 1;
    }

    FILE* fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("Error: cannot open file %s\n", argv[1]);
        return 1;
    }

    int loadpc = 499;
    int op, l, m;

    while (fscanf(fp, "%d %d %d", &op, &l, &m) == 3) {
        PAS[loadpc]     = op;
        PAS[loadpc - 1] = l;
        PAS[loadpc - 2] = m;
        loadpc -= 3;
    }

    fclose(fp);

    int pc = 499;
    int sp = loadpc + 1;
    int bp = sp - 1;
    int initBP = bp;
    int halted = 0;

    printf("\tL\tM\tPC\tBP\tSP\tstack\n");
    printf("Initial values : %d %d %d\n", pc, bp, sp);

    while (!halted)
    {
        int prevpc = pc;

        op = PAS[pc];
        l  = PAS[pc - 1];
        m  = PAS[pc - 2];

        pc -= 3;

        switch (op)
        {
            /* LIT */
            case 1:
                printf("LIT");
                sp -= 1;
                PAS[sp] = m;
                break;

            /* OPR */
            case 2:
                switch (m) {

                    case 0: /* RTN */
                        printf("RTN");
                        sp = bp + 1;
                        bp = PAS[sp - 2];
                        pc = PAS[sp - 3];
                        break;

                    case 1: /* ADD */
                        printf("ADD");
                        PAS[sp + 1] = PAS[sp + 1] + PAS[sp];
                        sp += 1;
                        break;

                    case 2: /* SUB */
                        printf("SUB");
                        PAS[sp + 1] = PAS[sp + 1] - PAS[sp];
                        sp += 1;
                        break;

                    case 3: /* MUL */
                        printf("MUL");
                        PAS[sp + 1] = PAS[sp + 1] * PAS[sp];
                        sp += 1;
                        break;

                    case 4: /* DIV */
                        printf("DIV");
                        PAS[sp + 1] = PAS[sp + 1] / PAS[sp];
                        sp += 1;
                        break;

                    case 5: /* EQL */
                        printf("EQL");
                        PAS[sp + 1] = (PAS[sp + 1] == PAS[sp]);
                        sp += 1;
                        break;

                    case 6: /* NEQ */
                        printf("NEQ");
                        PAS[sp + 1] = (PAS[sp + 1] != PAS[sp]);
                        sp += 1;
                        break;

                    case 7: /* LSS */
                        printf("LSS");
                        PAS[sp + 1] = (PAS[sp + 1] < PAS[sp]);
                        sp += 1;
                        break;

                    case 8: /* LEQ */
                        printf("LEQ");
                        PAS[sp + 1] = (PAS[sp + 1] <= PAS[sp]);
                        sp += 1;
                        break;

                    case 9: /* GTR */
                        printf("GTR");
                        PAS[sp + 1] = (PAS[sp + 1] > PAS[sp]);
                        sp += 1;
                        break;

                    case 10: /* GEQ */
                        printf("GEQ");
                        PAS[sp + 1] = (PAS[sp + 1] >= PAS[sp]);
                        sp += 1;
                        break;

                    case 11: /* EVEN (HW4 REQUIRED) */
                        printf("EVEN");
                        PAS[sp + 1] = (PAS[sp] % 2 == 0);
                        sp += 1;
                        break;

                    default:
                        printf("invalid m");
                        break;
                }
                break;

            /* LOD */
            case 3:
                printf("LOD");
                sp -= 1;
                PAS[sp] = PAS[ BaseHelper(bp, l, PAS) - m ];
                break;

            /* STO */
            case 4:
                printf("STO");
                PAS[ BaseHelper(bp, l, PAS) - m ] = PAS[sp];
                sp += 1;
                break;

            /* CAL */
            case 5:
                printf("CAL");
                PAS[sp - 1] = BaseHelper(bp, l, PAS); /* static link */
                PAS[sp - 2] = bp;                     /* dynamic link */
                PAS[sp - 3] = pc;                     /* return address */
                bp = sp - 1;
                pc = 499 - m;
                break;

            /* INC */
            case 6:
                printf("INC");
                sp -= m;
                break;

            /* JMP */
            case 7:
                printf("JMP");
                pc = 499 - m;
                break;

            /* JPC */
            case 8:
                printf("JPC");
                if (PAS[sp] == 0)
                    pc = 499 - m;
                else
                    pc = prevpc - 3;
                sp += 1;
                break;

            /* SYS */
            case 9:
                printf("SYS");
                if (m == 1) {
                    printf("\tOutput result is : %d\n", PAS[sp]);
                    sp += 1;
                }
                else if (m == 2) {
                    printf("\tPlease Enter an Integer: ");
                    scanf("%d", &PAS[--sp]);
                }
                else if (m == 3) {
                    halted = 1;
                }
                break;

            default:
                printf("Invalid / unrecognized cmd\n");
                halted = 1;
                break;
        }

        printf("\t%d\t%d\t", l, m);
        PrintStackTrace(pc, bp, sp, initBP, PAS);
    }

    return 0;
}

/* BaseHelper: follow static link chain l times */
int BaseHelper(int bp, int l, int PAS[])
{
    int arb = bp;
    while (l > 0) {
        arb = PAS[arb];
        l--;
    }
    return arb;
}

/* PrintStackTrace: prints PC, BP, SP, and stack contents */
void PrintStackTrace(int PC, int BP, int SP, int initialBP, int PAS[])
{
    printf("%d\t%d\t%d\t", PC, BP, SP);

    for (int i = initialBP; i > BP; i--)
        printf("%d ", PAS[i]);

    if (BP < initialBP)
        printf("| ");

    for (int i = BP; i >= SP; i--)
        printf("%d ", PAS[i]);

    printf("\n");
}
