/*
	Name 1: Thomas McRoberts
    UTEID 1: tpm627
*/

/***************************************************************/
/*                                                             */
/*   LC-3b Simulator                                           */
/*                                                             */
/*   EE 460N                                                   */
/*   The University of Texas at Austin                         */
/*                                                             */
/***************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/***************************************************************/
/*                                                             */
/* Files:  ucode        Microprogram file                      */
/*         isaprogram   LC-3b machine language program file    */
/*                                                             */
/***************************************************************/

/***************************************************************/
/* These are the functions you'll have to write.               */
/***************************************************************/

void eval_micro_sequencer();
void cycle_memory();
void eval_bus_drivers();
void drive_bus();
void latch_datapath_values();

/***************************************************************/
/* A couple of useful definitions.                             */
/***************************************************************/
#define FALSE 0
#define TRUE  1
#define DEBUG 1

/***************************************************************/
/* Use this to avoid overflowing 16 bits on the bus.           */
/***************************************************************/
#define Low16bits(x) ((x) & 0xFFFF)

/***************************************************************/
/* Definition of the control store layout.                     */
/***************************************************************/
#define CONTROL_STORE_ROWS 64
#define INITIAL_STATE_NUMBER 18

/***************************************************************/
/* Definition of bit order in control store word.              */
/***************************************************************/
enum CS_BITS {                                                  
    IRD,
    COND1, COND0,
    J5, J4, J3, J2, J1, J0,
    LD_MAR,
    LD_MDR,
    LD_IR,
    LD_BEN,
    LD_REG,
    LD_CC,
    LD_PC,
    GATE_PC,
    GATE_MDR,
    GATE_ALU,
    GATE_MARMUX,
    GATE_SHF,
    PCMUX1, PCMUX0,
    DRMUX,
    SR1MUX,
    ADDR1MUX,
    ADDR2MUX1, ADDR2MUX0,
    MARMUX,
    ALUK1, ALUK0,
    MIO_EN,
    R_W,
    DATA_SIZE,
    LSHF1,
    CONTROL_STORE_BITS
} CS_BITS;

/***************************************************************/
/* Functions to get at the control bits.                       */
/***************************************************************/
int GetIRD(int *x)           { return(x[IRD]); }
int GetCOND(int *x)          { return((x[COND1] << 1) + x[COND0]); }
int GetJ(int *x)             { return((x[J5] << 5) + (x[J4] << 4) +
				      (x[J3] << 3) + (x[J2] << 2) +
				      (x[J1] << 1) + x[J0]); }
int GetLD_MAR(int *x)        { return(x[LD_MAR]); }
int GetLD_MDR(int *x)        { return(x[LD_MDR]); }
int GetLD_IR(int *x)         { return(x[LD_IR]); }
int GetLD_BEN(int *x)        { return(x[LD_BEN]); }
int GetLD_REG(int *x)        { return(x[LD_REG]); }
int GetLD_CC(int *x)         { return(x[LD_CC]); }
int GetLD_PC(int *x)         { return(x[LD_PC]); }
int GetGATE_PC(int *x)       { return(x[GATE_PC]); }
int GetGATE_MDR(int *x)      { return(x[GATE_MDR]); }
int GetGATE_ALU(int *x)      { return(x[GATE_ALU]); }
int GetGATE_MARMUX(int *x)   { return(x[GATE_MARMUX]); }
int GetGATE_SHF(int *x)      { return(x[GATE_SHF]); }
int GetPCMUX(int *x)         { return((x[PCMUX1] << 1) + x[PCMUX0]); }
int GetDRMUX(int *x)         { return(x[DRMUX]); }
int GetSR1MUX(int *x)        { return(x[SR1MUX]); }
int GetADDR1MUX(int *x)      { return(x[ADDR1MUX]); }
int GetADDR2MUX(int *x)      { return((x[ADDR2MUX1] << 1) + x[ADDR2MUX0]); }
int GetMARMUX(int *x)        { return(x[MARMUX]); }
int GetALUK(int *x)          { return((x[ALUK1] << 1) + x[ALUK0]); }
int GetMIO_EN(int *x)        { return(x[MIO_EN]); }
int GetR_W(int *x)           { return(x[R_W]); }
int GetDATA_SIZE(int *x)     { return(x[DATA_SIZE]); } 
int GetLSHF1(int *x)         { return(x[LSHF1]); }

/***************************************************************/
/* The control store rom.                                      */
/***************************************************************/
int CONTROL_STORE[CONTROL_STORE_ROWS][CONTROL_STORE_BITS];

/***************************************************************/
/* Main memory.                                                */
/***************************************************************/
/* MEMORY[A][0] stores the least significant byte of word at word address A
   MEMORY[A][1] stores the most significant byte of word at word address A 
   There are two write enable signals, one for each byte. WE0 is used for 
   the least significant byte of a word. WE1 is used for the most significant 
   byte of a word. */

#define WORDS_IN_MEM    0x08000 
#define MEM_CYCLES      5
int MEMORY[WORDS_IN_MEM][2];

/***************************************************************/

/***************************************************************/

/***************************************************************/
/* LC-3b State info.                                           */
/***************************************************************/
#define LC_3b_REGS 8

int RUN_BIT;	/* run bit */
int BUS;	/* value of the bus */

typedef struct System_Latches_Struct{

int PC,		/* program counter */
    MDR,	/* memory data register */
    MAR,	/* memory address register */
    IR,		/* instruction register */
    N,		/* n condition bit */
    Z,		/* z condition bit */
    P,		/* p condition bit */
    BEN;        /* ben register */

int READY;	/* ready bit */
  /* The ready bit is also latched as you dont want the memory system to assert it 
     at a bad point in the cycle*/

int REGS[LC_3b_REGS]; /* register file. */

int MICROINSTRUCTION[CONTROL_STORE_BITS]; /* The microinstruction */

int STATE_NUMBER; /* Current State Number - Provided for debugging */ 
} System_Latches;

/* Data Structure for Latch */

System_Latches CURRENT_LATCHES, NEXT_LATCHES;

/***************************************************************/
/* A cycle counter.                                            */
/***************************************************************/
int CYCLE_COUNT;

/***************************************************************/
/*                                                             */
/* Procedure : help                                            */
/*                                                             */
/* Purpose   : Print out a list of commands.                   */
/*                                                             */
/***************************************************************/
void help() {                                                    
    printf("----------------LC-3bSIM Help-------------------------\n");
    printf("go               -  run program to completion       \n");
    printf("run n            -  execute program for n cycles    \n");
    printf("mdump low high   -  dump memory from low to high    \n");
    printf("rdump            -  dump the register & bus values  \n");
    printf("?                -  display this help menu          \n");
    printf("quit             -  exit the program                \n\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : cycle                                           */
/*                                                             */
/* Purpose   : Execute a cycle                                 */
/*                                                             */
/***************************************************************/
void cycle() {                                                

  eval_micro_sequencer();   
  cycle_memory();
  eval_bus_drivers();
  drive_bus();
  latch_datapath_values();

  CURRENT_LATCHES = NEXT_LATCHES;

  CYCLE_COUNT++;
}

/***************************************************************/
/*                                                             */
/* Procedure : run n                                           */
/*                                                             */
/* Purpose   : Simulate the LC-3b for n cycles.                 */
/*                                                             */
/***************************************************************/
void run(int num_cycles) {                                      
    int i;

    if (RUN_BIT == FALSE) {
	printf("Can't simulate, Simulator is halted\n\n");
	return;
    }

    printf("Simulating for %d cycles...\n\n", num_cycles);
    for (i = 0; i < num_cycles; i++) {
	if (CURRENT_LATCHES.PC == 0x0000) {
	    RUN_BIT = FALSE;
	    printf("Simulator halted\n\n");
	    break;
	}
	cycle();
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : go                                              */
/*                                                             */
/* Purpose   : Simulate the LC-3b until HALTed.                 */
/*                                                             */
/***************************************************************/
void go() {                                                     
    if (RUN_BIT == FALSE) {
	printf("Can't simulate, Simulator is halted\n\n");
	return;
    }

    printf("Simulating...\n\n");
    while (CURRENT_LATCHES.PC != 0x0000)
	cycle();
    RUN_BIT = FALSE;
    printf("Simulator halted\n\n");
}

/***************************************************************/ 
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(FILE * dumpsim_file, int start, int stop) {          
    int address; /* this is a byte address */

    printf("\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
    printf("-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
	printf("  0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    printf("\n");

    /* dump the memory contents into the dumpsim file */
    fprintf(dumpsim_file, "\nMemory content [0x%.4x..0x%.4x] :\n", start, stop);
    fprintf(dumpsim_file, "-------------------------------------\n");
    for (address = (start >> 1); address <= (stop >> 1); address++)
	fprintf(dumpsim_file, " 0x%.4x (%d) : 0x%.2x%.2x\n", address << 1, address << 1, MEMORY[address][1], MEMORY[address][0]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */   
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump(FILE * dumpsim_file) {                               
    int k; 

    printf("\nCurrent register/bus values :\n");
    printf("-------------------------------------\n");
    printf("Cycle Count  : %d\n", CYCLE_COUNT);
    printf("PC           : 0x%.4x\n", CURRENT_LATCHES.PC);
    printf("IR           : 0x%.4x\n", CURRENT_LATCHES.IR);
    printf("STATE_NUMBER : 0x%.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    printf("BUS          : 0x%.4x\n", BUS);
    printf("MDR          : 0x%.4x\n", CURRENT_LATCHES.MDR);
    printf("MAR          : 0x%.4x\n", CURRENT_LATCHES.MAR);
    printf("CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    printf("Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
	printf("%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
    printf("\n");

    /* dump the state information into the dumpsim file */
    fprintf(dumpsim_file, "\nCurrent register/bus values :\n");
    fprintf(dumpsim_file, "-------------------------------------\n");
    fprintf(dumpsim_file, "Cycle Count  : %d\n", CYCLE_COUNT);
    fprintf(dumpsim_file, "PC           : 0x%.4x\n", CURRENT_LATCHES.PC);
    fprintf(dumpsim_file, "IR           : 0x%.4x\n", CURRENT_LATCHES.IR);
    fprintf(dumpsim_file, "STATE_NUMBER : 0x%.4x\n\n", CURRENT_LATCHES.STATE_NUMBER);
    fprintf(dumpsim_file, "BUS          : 0x%.4x\n", BUS);
    fprintf(dumpsim_file, "MDR          : 0x%.4x\n", CURRENT_LATCHES.MDR);
    fprintf(dumpsim_file, "MAR          : 0x%.4x\n", CURRENT_LATCHES.MAR);
    fprintf(dumpsim_file, "CCs: N = %d  Z = %d  P = %d\n", CURRENT_LATCHES.N, CURRENT_LATCHES.Z, CURRENT_LATCHES.P);
    fprintf(dumpsim_file, "Registers:\n");
    for (k = 0; k < LC_3b_REGS; k++)
	fprintf(dumpsim_file, "%d: 0x%.4x\n", k, CURRENT_LATCHES.REGS[k]);
    fprintf(dumpsim_file, "\n");
    fflush(dumpsim_file);
}

/***************************************************************/
/*                                                             */
/* Procedure : get_command                                     */
/*                                                             */
/* Purpose   : Read a command from standard input.             */  
/*                                                             */
/***************************************************************/
void get_command(FILE * dumpsim_file) {                         
    char buffer[20];
    int start, stop, cycles;

    printf("LC-3b-SIM> ");

    scanf("%s", buffer);
    printf("\n");

    switch(buffer[0]) {
    case 'G':
    case 'g':
	go();
	break;

    case 'M':
    case 'm':
	scanf("%i %i", &start, &stop);
	mdump(dumpsim_file, start, stop);
	break;

    case '?':
	help();
	break;
    case 'Q':
    case 'q':
	printf("Bye.\n");
	exit(0);

    case 'R':
    case 'r':
	if (buffer[1] == 'd' || buffer[1] == 'D')
	    rdump(dumpsim_file);
	else {
	    scanf("%d", &cycles);
	    run(cycles);
	}
	break;

    default:
	printf("Invalid Command\n");
	break;
    }
}

/***************************************************************/
/*                                                             */
/* Procedure : init_control_store                              */
/*                                                             */
/* Purpose   : Load microprogram into control store ROM        */ 
/*                                                             */
/***************************************************************/
void init_control_store(char *ucode_filename) {                 
    FILE *ucode;
    int i, j, index;
    char line[200];

    printf("Loading Control Store from file: %s\n", ucode_filename);

    /* Open the micro-code file. */
    if ((ucode = fopen(ucode_filename, "r")) == NULL) {
	printf("Error: Can't open micro-code file %s\n", ucode_filename);
	exit(-1);
    }

    /* Read a line for each row in the control store. */
    for(i = 0; i < CONTROL_STORE_ROWS; i++) {
	if (fscanf(ucode, "%[^\n]\n", line) == EOF) {
	    printf("Error: Too few lines (%d) in micro-code file: %s\n",
		   i, ucode_filename);
	    exit(-1);
	}

	/* Put in bits one at a time. */
	index = 0;

	for (j = 0; j < CONTROL_STORE_BITS; j++) {
	    /* Needs to find enough bits in line. */
	    if (line[index] == '\0') {
		printf("Error: Too few control bits in micro-code file: %s\nLine: %d\n",
		       ucode_filename, i);
		exit(-1);
	    }
	    if (line[index] != '0' && line[index] != '1') {
		printf("Error: Unknown value in micro-code file: %s\nLine: %d, Bit: %d\n",
		       ucode_filename, i, j);
		exit(-1);
	    }

	    /* Set the bit in the Control Store. */
	    CONTROL_STORE[i][j] = (line[index] == '0') ? 0:1;
	    index++;
	}

	/* Warn about extra bits in line. */
	if (line[index] != '\0')
	    printf("Warning: Extra bit(s) in control store file %s. Line: %d\n",
		   ucode_filename, i);
    }
    printf("\n");
}

/************************************************************/
/*                                                          */
/* Procedure : init_memory                                  */
/*                                                          */
/* Purpose   : Zero out the memory array                    */
/*                                                          */
/************************************************************/
void init_memory() {                                           
    int i;

    for (i=0; i < WORDS_IN_MEM; i++) {
	MEMORY[i][0] = 0;
	MEMORY[i][1] = 0;
    }
}

/**************************************************************/
/*                                                            */
/* Procedure : load_program                                   */
/*                                                            */
/* Purpose   : Load program and service routines into mem.    */
/*                                                            */
/**************************************************************/
void load_program(char *program_filename) {                   
    FILE * prog;
    int ii, word, program_base;

    /* Open program file. */
    prog = fopen(program_filename, "r");
    if (prog == NULL) {
	printf("Error: Can't open program file %s\n", program_filename);
	exit(-1);
    }

    /* Read in the program. */
    if (fscanf(prog, "%x\n", &word) != EOF)
	program_base = word >> 1;
    else {
	printf("Error: Program file is empty\n");
	exit(-1);
    }

    ii = 0;
    while (fscanf(prog, "%x\n", &word) != EOF) {
	/* Make sure it fits. */
	if (program_base + ii >= WORDS_IN_MEM) {
	    printf("Error: Program file %s is too long to fit in memory. %x\n",
		   program_filename, ii);
	    exit(-1);
	}

	/* Write the word to memory array. */
	MEMORY[program_base + ii][0] = word & 0x00FF;
	MEMORY[program_base + ii][1] = (word >> 8) & 0x00FF;
	ii++;
    }

    if (CURRENT_LATCHES.PC == 0) CURRENT_LATCHES.PC = (program_base << 1);

    printf("Read %d words from program into memory.\n\n", ii);
}

/***************************************************************/
/*                                                             */
/* Procedure : initialize                                      */
/*                                                             */
/* Purpose   : Load microprogram and machine language program  */ 
/*             and set up initial state of the machine.        */
/*                                                             */
/***************************************************************/
void initialize(char *ucode_filename, char *program_filename, int num_prog_files) { 
    int i;
    init_control_store(ucode_filename);

    init_memory();
    for ( i = 0; i < num_prog_files; i++ ) {
	load_program(program_filename);
	while(*program_filename++ != '\0');
    }
    CURRENT_LATCHES.Z = 1;
    CURRENT_LATCHES.STATE_NUMBER = INITIAL_STATE_NUMBER;
    memcpy(CURRENT_LATCHES.MICROINSTRUCTION, CONTROL_STORE[INITIAL_STATE_NUMBER], sizeof(int)*CONTROL_STORE_BITS);

    NEXT_LATCHES = CURRENT_LATCHES;

    RUN_BIT = TRUE;
}

/***************************************************************/
/*                                                             */
/* Procedure : main                                            */
/*                                                             */
/***************************************************************/
int main(int argc, char *argv[]) {                              
    FILE * dumpsim_file;

    /* Error Checking */
    if (argc < 3) {
	printf("Error: usage: %s <micro_code_file> <program_file_1> <program_file_2> ...\n",
	       argv[0]);
	exit(1);
    }

    printf("LC-3b Simulator\n\n");

    initialize(argv[1], argv[2], argc - 2);

    if ( (dumpsim_file = fopen( "dumpsim", "w" )) == NULL ) {
	printf("Error: Can't open dumpsim file\n");
	exit(-1);
    }

    while (1)
	get_command(dumpsim_file);

}

/* Do not modify the above code.
   You are allowed to use the following global variables in your
   code. These are defined above.

   CONTROL_STORE
   MEMORY
   BUS

   CURRENT_LATCHES
   NEXT_LATCHES

   You may define your own local/global variables and functions.
   You may use the functions to get at the control bits defined
   above.

   Begin your code here 	  			       */
/***************************************************************/


int sext(int num, int bits){
	int sign = (num >> (bits - 1)) & 0x1;
	int ret;
	ret = 0xFFFF << bits;

	if(sign){
		ret = ret | num;
 	}
	else{
		ret = (~ret) & num;
	}

	return ret;
}
  




/* 
 * Evaluate the address of the next state according to the 
 * micro sequencer logic. Latch the next microinstruction.
 */
void eval_micro_sequencer() {	
	
    
    int COND = GetCOND(CURRENT_LATCHES.MICROINSTRUCTION);
	int BEN = CURRENT_LATCHES.BEN;
    if(BEN){
    }
	int R  = CURRENT_LATCHES.READY;
 	int IR11 = (CURRENT_LATCHES.IR >> 11) & 0x1;
	int opcode[4];
    int i = 0;
    printf("opcode:");
    for(i = 0; i < 4; i++){
        opcode[i] = (CURRENT_LATCHES.IR >> (i + 12) ) & 0x1;
        printf("%d", opcode[i]);
    }
    printf("\n");
	int IRD = GetIRD(CURRENT_LATCHES.MICROINSTRUCTION); 
	int nextStateAddr[6];
    int J = GetJ(CURRENT_LATCHES.MICROINSTRUCTION);
    
    if(IRD){
        nextStateAddr[0] = opcode[0];
        nextStateAddr[1] = opcode[1];
        nextStateAddr[2] = opcode[2];
        nextStateAddr[3] = opcode[3];
        nextStateAddr[4] = 0;
        nextStateAddr[5] = 0;
    }
    else{
        nextStateAddr[0] = (J & 0x1) || ( (COND == 3) && IR11);
        nextStateAddr[1] = ( (J >> 1) & 0x1) || ( (COND == 1) && R);
        nextStateAddr[2] = ( (J >> 2) & 0x1) || ( (COND == 2) && BEN);
        nextStateAddr[3] = ( (J >> 3) & 0x1);
        nextStateAddr[4] = ( (J >> 4) & 0x1);
        nextStateAddr[5] = ( (J >> 5) & 0x1);

    }
    int nextState = nextStateAddr[0] + 2*nextStateAddr[1] + 4*nextStateAddr[2] + 8*nextStateAddr[3] + 16*nextStateAddr[4] + 32*nextStateAddr[5];
    printf("**************Current state %d\n", CURRENT_LATCHES.STATE_NUMBER);
    NEXT_LATCHES.STATE_NUMBER = nextState;
    printf("**************NEXT state %d\n",NEXT_LATCHES.STATE_NUMBER);
	for(i = 0; i < CONTROL_STORE_BITS; i++){
        printf("%d", CONTROL_STORE[nextState][i]);
        NEXT_LATCHES.MICROINSTRUCTION[i] = CONTROL_STORE[nextState][i];
    }
    printf("\n");
}



/* 
 * This function emulates memory and the WE logic. 
 * Keep track of which cycle of MEMEN we are dealing with.  
 * If fourth, we need to latch Ready bit at the end of 
 * cycle to prepare microsequencer for the fifth cycle.  
 */

int memCycles = 0;

void cycle_memory() {
    if( GetMIO_EN(CURRENT_LATCHES.MICROINSTRUCTION) ){
        /*  Enable memory  */
        if(CURRENT_LATCHES.READY){
            NEXT_LATCHES.READY = 0;
            memCycles = 0;
        }
        else{
            if(memCycles == 4){
                /*  Memory Access  */
                if(GetR_W(CURRENT_LATCHES.MICROINSTRUCTION) == 0){
                    /*  Read  */
                    if(GetDATA_SIZE(CURRENT_LATCHES.MICROINSTRUCTION) == 0){
                        /* byte */
                        NEXT_LATCHES.MDR = (MEMORY[CURRENT_LATCHES.MAR >> 1][0] + (MEMORY[CURRENT_LATCHES.MAR >> 1][1] << 8) );
                    }
                    else{
                        /* word */ 
                        NEXT_LATCHES.MDR = (MEMORY[CURRENT_LATCHES.MAR >> 1][0] + (MEMORY[CURRENT_LATCHES.MAR >> 1][1] << 8) );
                    }
                
                printf("  MDR= 0x%4x\n", NEXT_LATCHES.MDR); 
                }
                else{
                    /* WRITE */
                    if(GetDATA_SIZE(CURRENT_LATCHES.MICROINSTRUCTION) == 0){
                        /* byte */
                        int data = CURRENT_LATCHES.MDR & 0xFF;
                        MEMORY[CURRENT_LATCHES.MAR >> 1][CURRENT_LATCHES.MAR & 0x1] = data;

                    }
                    else{
                        /* word */ 
                        int hibyte = (CURRENT_LATCHES.MDR >> 8) & 0xFF;
                        int lobyte = CURRENT_LATCHES.MDR & 0xFF;
                        MEMORY[CURRENT_LATCHES.MAR >> 1][1] = hibyte;
                        MEMORY[CURRENT_LATCHES.MAR >> 1][0] = lobyte;
                    }
                }
                
                NEXT_LATCHES.READY = 1;
            }
        }

    memCycles ++;
    }
}



int outSR1 = 0;
int outSR2MUX = 0;
int outALU = 0;
int outSHF = 0;

int outADDR2MUX = 0;
int outLSFH1 = 0;
int outADDR1MUX = 0;
int outADDRALU = 0;
int outPCMUX = 0;
int outMARMUX = 0;

int outMDRMUX = 0;
int outMDRtoBUSLOGIC = 0;
int outBUStoMDRLOGIC = 0;
/* 
 * Datapath routine emulating operations before driving the bus.
 * Evaluate the input of tristate drivers 
 *      Gate_MARMUX,
 *		Gate_PC,
 *		Gate_ALU,
 *		Gate_SHF,
 *		Gate_MDR.
 */   
void eval_bus_drivers() {
    int* uinstr = CURRENT_LATCHES.MICROINSTRUCTION;
    /*  set SR1  */
    if(GetSR1MUX(uinstr)){
        outSR1 = CURRENT_LATCHES.REGS[ (CURRENT_LATCHES.IR >> 6) & 0x7];
    }
    else{   
        outSR1 = CURRENT_LATCHES.REGS[ (CURRENT_LATCHES.IR >> 9) & 0x7];
    }
	outSR1 = Low16bits(outSR1);
    printf("SR1 0x%4x\n", outSR1);

    /*  set SR2  */
    if( (CURRENT_LATCHES.IR >> 5) & 0x1){
        outSR2MUX = sext(CURRENT_LATCHES.IR & 0x1F, 5);
    }
    else{
        outSR2MUX = CURRENT_LATCHES.REGS[ (CURRENT_LATCHES.IR) & 0x3];
    }
    outSR2MUX = Low16bits(outSR2MUX);
	
    /*  Set ALU  */	
    switch(GetALUK(uinstr)) {
    case 0:
        outALU = outSR1 + outSR2MUX; 
        break;

    case 1:
        outALU = outSR1 & outSR2MUX;
        break;

    case 2:
        outALU = outSR1 ^ outSR2MUX;
        break;

    case 3:
        outALU = outSR1;
        break;
    defualt:
        break;
    }
    outALU = Low16bits(outALU);
    

    /*  Set SHF  */
    int amount4 = (CURRENT_LATCHES.IR & 0xF);
    int sign = ( (outSR1 >> 15) & 0x1 );
    int highbits = 0xFF << (16 - amount4);
    switch( (CURRENT_LATCHES.IR >> 4) & 0x3){
    case 0: 
        outSHF = outSR1 << amount4;
        break;
    case 1:
        outSHF = (outSR1 >> amount4) & (~highbits);
        break;
    case 3:
        outSHF = (outSR1 >> amount4) | highbits*sign; 
        break;
    default:
        break;

    }
    outSHF = Low16bits(outSHF);

    /*  Set ADDR2MUX  */
    switch(GetADDR2MUX(uinstr)){
    case 0:
        outADDR2MUX = 0;
        break;
    case 1:
        outADDR2MUX = sext(CURRENT_LATCHES.IR & 0x3F, 6);
        break;
    case 2:
        outADDR2MUX = sext(CURRENT_LATCHES.IR & 0x1FF, 9);
        break;
    case 3:
        outADDR2MUX = sext(CURRENT_LATCHES.IR & 0x7FF, 11);
        break;
    default:
        printf("Addr2MUX error");
        break;
    }
    outADDR2MUX = Low16bits(outADDR2MUX);
    printf("outADDR2MUX = 0x%4x\n", outADDR2MUX);
    
    /*  Set LSFH1  */
    if(GetLSHF1(uinstr)){
        outLSFH1 = outADDR2MUX << 1;
    }
    else{
        outLSFH1 = outADDR2MUX;
    }
    outLSFH1 = Low16bits(outLSFH1);
    printf("LSHF1 = 0x%4x\n", outLSFH1);

    /*  Set ADDR1MUX  */
    if(GetADDR1MUX(uinstr) == 0){
        outADDR1MUX = CURRENT_LATCHES.PC;
    }
    else{
        outADDR1MUX = outSR1;
    }
    outADDR1MUX = Low16bits(outADDR1MUX);
    printf("ADDR1MUX = 0x%4x\n", outADDR1MUX);

    /*  Set ADDRALU  */
    outADDRALU = Low16bits(outADDR1MUX + outLSFH1);
    printf("outADDRALU = 0x%4x\n", outADDRALU);    

    /*  Set PCMUX  */
    switch(GetPCMUX(uinstr)){
    case 0:
        outPCMUX = CURRENT_LATCHES.PC + 2;
        break;
    case 1:
        break;
    case 2:
        outPCMUX = outADDRALU;
        break;
    default:
        printf("Error in PCMUX");
        break;
    }
    outPCMUX = Low16bits(outPCMUX);
    printf("PCMUX = 0x%4x\n", outPCMUX);

    /*  Set MARMUX  */
    if(GetMARMUX(uinstr) == 0){
        outMARMUX = (CURRENT_LATCHES.IR && 0xFF) << 1;
    }
    else{
        outMARMUX = outADDRALU;
    }
    outMARMUX = Low16bits(outMARMUX);
    
    /*  Set MDR2BUS Logic  */ 
    if(GetDATA_SIZE(uinstr) == 0 ){
        /*  Byte  */
        if(CURRENT_LATCHES.MAR & 0x1){
            /*  HI byte  */
            outMDRtoBUSLOGIC = sext( (CURRENT_LATCHES.MDR >> 8) & 0xFF, 8);   
        }
        else{
            /*  LO Byte  */
            outMDRtoBUSLOGIC = sext(CURRENT_LATCHES.MDR & 0xFF, 8);
        }
    }
    else{
        /*  Word  */
        outMDRtoBUSLOGIC = CURRENT_LATCHES.MDR;

    }
    outMDRtoBUSLOGIC = Low16bits(outMDRtoBUSLOGIC);
}


  /* 
   * Datapath routine for driving the bus from one of the 5 possible 
   * tristate drivers. 
   */       
void drive_bus() {
    int* uinstr = CURRENT_LATCHES.MICROINSTRUCTION;
    int drives = 0;
    BUS = 0;
    if(GetGATE_PC(uinstr) ){
        BUS = CURRENT_LATCHES.PC;
        drives ++;
    }
    if(GetGATE_ALU(uinstr)){
        BUS = outALU;
        drives ++;
    }
    if(GetGATE_MDR(uinstr)){
        BUS = outMDRtoBUSLOGIC;
        drives ++;
    }
    if(GetGATE_SHF(uinstr)){
        BUS = outSHF;
        drives ++;
    }
    if(GetGATE_MARMUX(uinstr)){
        BUS = outMARMUX;
        drives ++;
    }
    if(drives > 1){
        printf("Drive Bus error: number of drives = %i", drives);
    }
    BUS = Low16bits(BUS);
}


  /* 
   * Datapath routine for computing all functions that need to latch
   * values in the data path at the end of this cycle.  Some values
   * require sourcing the bus; therefore, this routine has to come 
   * after drive_bus.
   */       
void latch_datapath_values() {
    int* uinstr = CURRENT_LATCHES.MICROINSTRUCTION;
    if(GetLD_IR(uinstr)){
        NEXT_LATCHES.IR = BUS;
    }
    if(GetLD_PC(uinstr)){
        if(GetPCMUX(uinstr) == 1){
            NEXT_LATCHES.PC = BUS;
        }
        else{
            NEXT_LATCHES.PC = Low16bits(outPCMUX);
        }
    }
    if(GetLD_MAR(uinstr)){
       NEXT_LATCHES.MAR = BUS;
    }
    if(GetLD_MDR(uinstr)){
        if(!GetMIO_EN(uinstr) ){
            NEXT_LATCHES.MDR = BUS;
        }
    }
    if(GetLD_CC(uinstr)){
        NEXT_LATCHES.N = 0;
        NEXT_LATCHES.Z = 0;
        NEXT_LATCHES.P = 0;
        if(BUS == 0){
            NEXT_LATCHES.Z = 1;
        }
        else if( (BUS >> 15) & 0x1){
            NEXT_LATCHES.N = 1;
        }
        else{
            NEXT_LATCHES.P = 1;
        }

    }
    if(GetLD_BEN(uinstr)){
        int n = ( (CURRENT_LATCHES.IR >> 11) & 0x1) && CURRENT_LATCHES.N;
        int z = ( (CURRENT_LATCHES.IR >> 10) & 0x1) && CURRENT_LATCHES.Z;
        int p = ( (CURRENT_LATCHES.IR >> 9) & 0x1) && CURRENT_LATCHES.P;
        NEXT_LATCHES.BEN = n || z || p;
        printf("nzp = %d%d%d, BEN = %d\n", n, z, p, NEXT_LATCHES.BEN);
    }
    if(GetLD_REG(uinstr)){
        if(GetDRMUX(uinstr) ){
            /*  R7  */
            NEXT_LATCHES.REGS[7] = BUS;
        }
        else{
            /*  IR[11:9]  */
            NEXT_LATCHES.REGS[ (CURRENT_LATCHES.IR >> 9) & 0x7 ] = BUS; 
        }
    }
    


}



