#include <iostream> 
#include <fstream> 
#include <string> 
#include <sstream> 
#include <iomanip> 
#include <cctype> 
#include <cstring> 

#define MAX_LINE_LEN 256
#define MAX_TOKENS   10
#define MAX_SYMBOLS  1000
#define MAX_LEVEL 32
#define MAX_OPCODE 16 
#define MAX_OPERAND 128 
#define MAX_LINES 1000
#define MAX_SYMS 1000

using namespace std;

// -------------------------------
// // SYMTAB ENTRY STRUCTURE
// // -------------------------------
typedef struct {
	int lineNumber;
	char label[50];
} Symbol;

typedef struct {
	int lineNumber; 
	int hasLabel;
	int  hasOperand;
	char label[MAX_LINE_LEN]; // ? (working on this)  empty if no label
	char opcode[MAX_OPCODE];
	char operand[MAX_OPERAND]; // ??  empty if no operand
} ParsedLine;

// Global SYMTAB
Symbol SYMTAB[MAX_SYMBOLS];
int symCount = 0;
// flags 
int n = 0, i = 0, x = 0, b = 0, p = 0, e = 0; 

//
//         // -------------------------------
//         // OPEN FILE FUNCTION
//         // -------------------------------
FILE* openFile(const char *filename) { // method to open file 
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("Error opening file");
	return NULL;
}
	return fp;
}

void writeSymTabIntoFile() { // writes symtab into a file
	FILE *outFile = fopen("symtab.txt", "w");
	if (outFile == NULL) {
		perror("Couldn't write SymTab to file");
		return;
	}

	fprintf(outFile, "Line\tLabel\n");
	fprintf(outFile, "---------------------\n");

	for (int i = 0; i < symCount; i++) {
        	fprintf(outFile, "%d\t%s\n", SYMTAB[i].lineNumber, SYMTAB[i].label);
	}

	fclose(outFile);
	printf("\nSymbol table successfully written to symtab.txt\n");
}

// writes final obj codew into a file? 
void writeObjTabintoFile(string locArr[], string symbolsArr[], string instArr[],
                         string refArr[], string objArr[], int size) {
	ofstream outFile("program.l");
	if (outFile.is_open()) {
		outFile << "LOC\tSYMBOL\tINSTRUCTION\tREFERENCE\tOBJECT\n";
		outFile << "--------------------------------------------------------\n";
		for (int i = 0; i < size; i++) {
			outFile << locArr[i] << '\t'
			<< symbolsArr[i] << '\t'
			<< instArr[i] << '\t'
			<< refArr[i] << '\t'
			<< objArr[i] << endl;
		}
		outFile.close();
		cout << "\nObject table successfully written to program.l" << endl;
	} else {
        cout << "Couldn't Write ObjTab to file" << endl;
    }
}

// hex string to int base 16
int convertHexToDec(const string& hexString) {
    // throws std::invalid_argument / std::out_of_range on bad input
	return std::stoi(hexString, nullptr, 16);
}

// decimal string to int
int convertStringToInt(const string& str) {
	return std::stoi(str);
}

// int to uppercase hex string 
string convertDecToHex(int decimal) {
	std::ostringstream oss;
	oss << std::uppercase << std::hex << decimal;
	return oss.str();
}

//
//                                         // -------------------------------
//                                         // MAIN
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

	writeSymTabIntoFile();

	return EXIT_SUCCESS;
}
