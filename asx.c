#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 256
#define MAX_TOKENS   10
#define MAX_SYMBOLS  500

// -------------------------------
// // SYMTAB ENTRY STRUCTURE
// // -------------------------------
typedef struct {
	int lineNumber;
	char label[50];
} Symbol;

// Global SYMTAB
Symbol SYMTAB[MAX_SYMBOLS];
int symCount = 0;
//
//         // -------------------------------
//         // OPEN FILE FUNCTION
//         // -------------------------------
FILE* openFile(const char *filename) {
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("Error opening file");
	return NULL;
}
	return fp;
}
//
//                                         // -------------------------------
//                                         // MIN
//                                         // -------------------------------
int main() {
	FILE *fp_sic = openFile("program.sic");
	if (fp_sic == NULL) return EXIT_FAILURE;

	FILE *fp_lst = fopen("program.l", "w");
	if (fp_lst == NULL) {
		perror("Error creating program.l");
		fclose(fp_sic);
		return EXIT_FAILURE;
	}

	char line[MAX_LINE_LEN];                                                          int lineNum = 1;
	
	printf("Processing program.sic and generating program.l...\n\n");
	while (fgets(line, sizeof(line), fp_sic) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		// copy entire linr into listing file
		fprintf(fp_lst, "%d\t%s\n", lineNum, line);
		// tokenize line by spaces
		char lineCopy[MAX_LINE_LEN];
		strcpy(lineCopy, line);

		char *tokens[MAX_TOKENS];
		int tokenCount = 0;
		char *token = strtok(lineCopy, " ");

		while (token != NULL && tokenCount < MAX_TOKENS) {
			tokens[tokenCount++] = token;
			token = strtok(NULL, " ");
		}

		// if there are exactly 4 elements: line, label, opcode, operand
		if (tokenCount == 4) {
			strcpy(SYMTAB[symCount].label, tokens[1]);   // label is token[1]
			SYMTAB[symCount].lineNumber = lineNum;
			symCount++;
		}

		// if there are 3 elements: line, opcode, operand 

	//	if (tokenCount == 3) {
	//		strcpy(
	//	}

		lineNum++;
	}

	fclose(fp_sic);
	fclose(fp_lst);

	// print symtab contents 
	printf("SYMTAB contents:/n"); 
	for (int i = 0; i < symCount; i++) {
		printf("%d\t%s\n", SYMTAB[i].lineNumber, SYMTAB[i].label); 
	}

	return EXIT_SUCCESS;
}
