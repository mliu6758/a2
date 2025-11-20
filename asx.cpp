#include <iostream> 
#include <fstream> 
#include <string> 
#include <sstream> 
#include <iomanip> 
#include <cctype> 
#include <cstring> 
#include <map>
#include <set>
#include <vector>
#include <algorithm>  

#define MAX_LINE_LEN 256
#define MAX_TOKENS   10
#define MAX_SYMBOLS  1000
#define MAX_LEVEL 32
#define MAX_OPCODE 16 
#define MAX_OPERAND 128 
#define MAX_LINES 1000
#define MAX_SYMS 1000

using namespace std;


// SYMTAB STRUCTURE

typedef struct {
	int lineNumber;
    unsigned int addr;   // address in hex
	char label[50];
} Symbol;

typedef struct {
	int lineNumber; 
	int hasLabel;
	int  hasOperand;
	char label[MAX_LINE_LEN]; //  empty if no label
	char opcode[MAX_OPCODE];
	char operand[MAX_OPERAND]; //  empty if no operand
} ParsedLine;

// global SYMTAB
static Symbol SYMTAB[MAX_SYMBOLS];
static int symCount = 0;
// flags 
int n = 0, i = 0, x = 0, b = 0, p = 0, e = 0; 

// control section tracking (for symtab.st formatting) 
static std::string gCsectName = "";
static unsigned gStartAddr = 0;   // Value (start) column for CSect
static unsigned gEndAddr   = 0;   // used to compute LENGTH = end - start

// OPEN FILE FUNCTION

FILE* openFile(const char *filename) { // method to open file 
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror("Error opening file");
	return NULL;
}
	return fp;
}

// Simple literal support (END-only pool)

struct Lit { string key; vector<unsigned char> bytes; unsigned addr=0; bool assigned=false; };
static map<string, Lit> LITTAB; // keyed by literal text

// writes symtab into a file
// Pass 2 - now use the object column
void writeListingFile(const string &listingName,
                      const vector<string> &locArr,
                      const vector<string> &symbolsArr,
                      const vector<string> &opArr,
                      const vector<string> &operandArr,
                      const vector<string> &objArr) {
	ofstream outFile(listingName);
	if (outFile.is_open()) {
		outFile << left << setw(6) << "LOC"
		        << setw(10) << "SYMBOL"
		        << setw(12) << "OPCODE"
		        << setw(18) << "OPERAND"
		        << "OBJECT" << "\n";
		outFile << "----------------------------------------------------------------\n";
		for (size_t i = 0; i < locArr.size(); i++) {
			outFile << left << setw(6) << locArr[i] << setw(10) << symbolsArr[i]
			        << setw(12) << opArr[i] << setw(18) << operandArr[i]
			        << objArr[i] << "\n";
		}
		outFile.close();
		cout << "\nListing successfully written to " << listingName << endl;
	} else {
        cout << "Couldn't Write listing file" << endl;
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

// Helpers for Pass 1 
static inline string trim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static vector<string> splitWS(const string &line) {
    vector<string> v;
    istringstream iss(line);
    string t;
    while (iss >> t) v.push_back(t);
    return v;
}

static bool isComment(const string &s) {
    // period in col 1 = a comment + also allow empty lines
    string t = trim(s);
    return t.empty() || t[0]=='.';
}

static bool isDirective(const string &op) {
    string u = op;
    for (auto &c: u) c = toupper(c);
    return (u=="START" || u=="END" || u=="BYTE" || u=="WORD" || u=="RESB" || u=="RESW" || u=="BASE");
}

static string stemOf(const string &path) {
    // drop directories and extension
    auto slash = path.find_last_of("/\\");
    string name = (slash==string::npos) ? path : path.substr(slash+1);
    auto dot = name.find_last_of('.');
    return (dot==string::npos) ? name : name.substr(0, dot);
}

// Minimal OPTAB sizing for Pass 1 (formats)
static const set<string> F1 = {
    "FIX","FLOAT","HIO","NORM","SIO","TIO"
};
static const set<string> F2 = {
    "ADDR","CLEAR","COMPR","DIVR","MULR","RMO","SHIFTL","SHIFTR","SUBR","SVC","TIXR"
};


// if prefaced with + - F4, otherwise F3

// Determine size in bytes for the opcode/directive for pass 1
static int instrSizeBytes(const string &opcodeRaw, const string &operandRaw) {
    string op = opcodeRaw;
    for (auto &c: op) c = toupper(c);
    if (op=="WORD") return 3;
    if (op=="RESW") return 3 * convertStringToInt(operandRaw);
    if (op=="RESB") return convertStringToInt(operandRaw);
    if (op=="BYTE") {
        // BYTE C'...' or X'...'
        if (operandRaw.size()>=3 && (operandRaw[0]=='C' || operandRaw[0]=='c') && operandRaw[1]=='\'' ) {
            auto pos = operandRaw.find_last_of('\'');
            int nChars = (pos==string::npos) ? 0 : (int)pos - 2;
            return nChars;
        }
        if (operandRaw.size()>=3 && (operandRaw[0]=='X' || operandRaw[0]=='x') && operandRaw[1]=='\'' ) {
            auto pos = operandRaw.find_last_of('\'');
            int nHex = (pos==string::npos) ? 0 : (int)pos - 2;
            return (nHex+1)/2; // 2 hex chars per byte
        }
        return 1; // handle
    }
    if (op=="BASE") return 0; // BASE doesn't change LOCCTR
    // Non-directive: instruction
    bool fmt4 = (!op.empty() && op[0]=='+');
    string baseOp = fmt4 ? op.substr(1) : op;
    if (F1.count(baseOp)) return 1;
    if (F2.count(baseOp)) return 2;
    return fmt4 ? 4 : 3; // default F3/F4
}

// 
// PASS 2 helpers
// 
struct OpInfo { unsigned opcode; int defFmt; };

static map<string, OpInfo> buildOptab() {
    map<string, OpInfo> m;
    // Add general opcodes
    m["LDA"]  = {0x00, 3}; m["LDX"]  = {0x04, 3}; m["LDL"]  = {0x08, 3};
    m["STA"]  = {0x0C, 3}; m["STX"]  = {0x10, 3}; m["STL"]  = {0x14, 3};
    m["ADD"]  = {0x18, 3}; m["SUB"]  = {0x1C, 3}; m["MUL"]  = {0x20, 3};
    m["DIV"]  = {0x24, 3}; m["COMP"] = {0x28, 3}; m["TIX"]  = {0x2C, 3};
    m["JEQ"]  = {0x30, 3}; m["JGT"]  = {0x34, 3}; m["JLT"]  = {0x38, 3};
    m["J"]    = {0x3C, 3}; m["JSUB"] = {0x48, 3}; m["RSUB"] = {0x4C, 3};
    m["LDCH"] = {0x50, 3}; m["STCH"] = {0x54, 3};
    m["LDB"]  = {0x68, 3}; m["LDS"]  = {0x6C, 3}; m["LDT"]  = {0x74, 3};
    m["STB"]  = {0x78, 3}; m["STS"]  = {0x7C, 3}; m["STT"]  = {0x84, 3};
    m["TD"]   = {0xE0, 3}; m["RD"]   = {0xD8, 3}; m["WD"]   = {0xDC, 3};
    return m;
}

static int lookupSymAddr(const string &lbl, unsigned &addrOut) {
    for (int i=0;i<symCount;i++) {
        if (lbl == string(SYMTAB[i].label)) { addrOut = SYMTAB[i].addr; return 1; }
    }
    return 0;
}

// encode WORD and BYTE to hex string
static string encodeData(const string &op, const string &operand) {
    string uop = op; for (auto &c: uop) c = toupper(c);
    if (uop=="WORD") {
        long val = 0;
        if (!operand.empty() && (operand[0]=='-' || isdigit((unsigned char)operand[0]))) {
            val = stol(operand);
        } else {
            // maybe symbol
            unsigned a=0; if (lookupSymAddr(operand, a)) val = (long)a;
        }
        unsigned v = (unsigned)(val & 0xFFFFFF); // 3 bytes
        ostringstream oss; oss<<uppercase<<hex<<setfill('0')<<setw(6)<<v;
        return oss.str();
    }
    if (uop=="BYTE") {
        if (operand.size()>=3 && (operand[0]=='C'||operand[0]=='c') && operand[1]=='\'') {
            auto pos = operand.find_last_of('\'');
            string txt = (pos==string::npos) ? "" : operand.substr(2, pos-2);
            ostringstream oss; oss<<uppercase<<hex<<setfill('0');
            for (unsigned char ch : txt) oss<<setw(2)<<(int)ch;
            return oss.str();
        }
        if (operand.size()>=3 && (operand[0]=='X'||operand[0]=='x') && operand[1]=='\'') {
            auto pos = operand.find_last_of('\'');
            string hexdata = (pos==string::npos) ? "" : operand.substr(2, pos-2);
            for (auto &c: hexdata) c = toupper(c);
            return hexdata;
        }
    }
    return "";
}

// Literal helpers 
static bool parseLiteral(const string& lit, vector<unsigned char>& out) {
    out.clear();
    if (lit.size()<4 || lit[0]!='=' || (toupper(lit[1])!='C' && toupper(lit[1])!='X') || lit[2]!='\'') return false;
    size_t end = lit.find_last_of('\''); if (end==string::npos) return false;
    string body = lit.substr(3, end-3);
    if (toupper(lit[1])=='C') {
        for (unsigned char ch: body) out.push_back(ch);
        return true;
    } else {
        string hex = body; for (auto &c: hex) c = toupper(c);
        if (hex.size()%2) hex = "0" + hex;
        for (size_t i=0;i<hex.size();i+=2) {
            unsigned v=0; std::istringstream(hex.substr(i,2)) >> std::hex >> v;
            out.push_back((unsigned char)v);
        }
        return true;
    }
}

static bool lookupLiteralAddr(const string& key, unsigned& addrOut) {
    auto it = LITTAB.find(key);
    if (it==LITTAB.end() || !it->second.assigned) return false;
    addrOut = it->second.addr;
    return true;
}

// Parses operand for addressing: returns flags n,i,x and the target/address/immediate value
static void parseAddressing(const string &raw, bool &nOut, bool &iOut, bool &xOut,
                            string &symOrEmpty, bool &isImmNumber, long &immVal) {
    nOut = iOut = true; // default simple
    xOut = false;
    symOrEmpty.clear();
    isImmNumber = false; immVal = 0;

    string s = raw;
    // handle ,X
    auto cpos = s.find(',');
    if (cpos != string::npos) {
        string suffix = s.substr(cpos+1);
        auto ttrim=[&](string& ss){ size_t b=ss.find_first_not_of(" \t"); size_t e=ss.find_last_not_of(" \t"); ss=(b==string::npos)?"":ss.substr(b,e-b+1); };
        ttrim(suffix);
        if (!suffix.empty() && (suffix=="X" || suffix=="x")) xOut = true;
        s = s.substr(0, cpos);
    }
    // # immediate? @ indirect?
    if (!s.empty() && s[0]=='#') {
        nOut=false; iOut=true;
        string rest = s.substr(1);
        if (!rest.empty() && (rest[0]=='-' || isdigit((unsigned char)rest[0]))) {
            isImmNumber = true; immVal = stol(rest);
        } else {
            symOrEmpty = rest;
        }
    } else if (!s.empty() && s[0]=='@') {
        nOut=true; iOut=false;
        symOrEmpty = s.substr(1);
    } else {
        symOrEmpty = s;
    }
}

// Encodes format 3/4
static string encodeInstr(const map<string,OpInfo>& OPTAB, const string &opcodeRaw,
                          const string &operandRaw, unsigned /*currLoc*/, unsigned nextLoc,
                          unsigned baseReg, bool baseValid, bool &ok, string &errMsg) {
    ok = true; errMsg.clear();

    // Directives produce no object code
    string up = opcodeRaw; for (auto &c: up) c=toupper(c);
    if (up=="WORD" || up=="BYTE" || up=="RESW" || up=="RESB" || up=="START" || up=="END" || up=="BASE") return "";

    bool fmt4 = (!up.empty() && up[0]=='+');
    string mnem = fmt4 ? up.substr(1) : up;
    auto it = OPTAB.find(mnem);
    if (it==OPTAB.end()) { ok=false; errMsg="Unknown opcode: "+mnem; return "****"; }
    unsigned op = it->second.opcode & 0xFC; // clear low 2 bits

    // RSUB - no operand, simple n=i=1, disp=0
    if (mnem=="RSUB") {
        unsigned n=1,i=1,x=0,b=0,p=0,e=(fmt4?1:0);
        if (fmt4) {
            unsigned byte1=(op|(n<<1)|i);
            unsigned xbpe=((x?1:0)<<3)|(b<<2)|(p<<1)|e;
            unsigned byte2=(xbpe<<4);
            unsigned byte3=0, byte4=0;
            ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3<<setw(2)<<byte4;
            return oss.str();
        } else {
            unsigned byte1=(op|(n<<1)|i);
            unsigned xbpe=((x?1:0)<<3)|(b<<2)|(p<<1)|e;
            unsigned byte2=(xbpe<<4);
            unsigned byte3=0;
            ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3;
            return oss.str();
        }
    }

    bool nFlag,iFlag,xFlag; string sym; bool isImmNum; long immVal;
    parseAddressing(operandRaw, nFlag, iFlag, xFlag, sym, isImmNum, immVal);

    unsigned nbit = nFlag?1:0, ibit=iFlag?1:0, xbit=xFlag?1:0, bbit=0, pbit=0, ebit=fmt4?1:0;

    unsigned target = 0;
    bool haveTarget = false;

    if (isImmNum) {
        // immediate numeric constant
        if (fmt4) {
            target = (unsigned) (immVal & 0xFFFFF);
            haveTarget = true;
        } else {
            long v = immVal;
            if (v < 0 || v > 0xFFF) {
                ok=false; errMsg="Immediate constant out of 12-bit range"; 
                return "****";
            }
            bbit=0; pbit=0;
            unsigned byte1 = (op | (nbit<<1) | ibit);
            unsigned xbpe = ((xbit?1:0)<<3) | (bbit<<2) | (pbit<<1) | ebit;
            unsigned byte2 = (xbpe<<4) | ((unsigned)v >> 8 & 0xF);
            unsigned byte3 = (unsigned)v & 0xFF;
            ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3;
            return oss.str();
        }
    } else if (!sym.empty()) {
        unsigned addr=0;
        if (sym[0]=='=') {
            if (!lookupLiteralAddr(sym, addr)) {
                ok=false; errMsg="Undefined symbol: "+sym; return "****";
            }
        } else {
            if (!lookupSymAddr(sym, addr)) {
                ok=false; errMsg="Undefined symbol: "+sym; return "****";
            }
        }
        target = addr; haveTarget = true;
    } else {
        // else no operand provided
        haveTarget = false;
    }

    if (fmt4) {
        // Format 4 
        unsigned byte1 = (op | (nbit<<1) | ibit);
        unsigned xbpe = ((xbit?1:0)<<3) | (bbit<<2) | (pbit<<1) | ebit;
        unsigned addr20 = haveTarget ? (target & 0xFFFFF) : 0;
        unsigned byte2 = (xbpe<<4) | ((addr20>>16)&0xF);
        unsigned byte3 = (addr20>>8)&0xFF;
        unsigned byte4 = addr20 & 0xFF;
        ostringstream oss; oss<<uppercase<<hex<<setfill('0')
            <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3<<setw(2)<<byte4;
        return oss.str();
    } else {
        // PC-relative first
        if (haveTarget) {
            long disp = (long)target - (long)nextLoc;
            if (disp >= -2048 && disp <= 2047) {
                pbit=1; bbit=0;
                unsigned udisp = (unsigned)(disp & 0xFFF);
                unsigned byte1 = (op | (nbit<<1) | ibit);
                unsigned xbpe = ((xbit?1:0)<<3) | (bbit<<2) | (pbit<<1) | ebit;
                unsigned byte2 = (xbpe<<4) | ((udisp>>8)&0xF);
                unsigned byte3 = udisp & 0xFF;
                ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                    <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3;
                return oss.str();
            }
            // Then BASE-relative if BASE is set
            if (baseValid) {
                long disp = (long)target - (long)baseReg;
                if (disp >= 0 && disp <= 4095) {
                    pbit=0; bbit=1;
                    unsigned udisp = (unsigned)(disp & 0xFFF);
                    unsigned byte1 = (op | (nbit<<1) | ibit);
                    unsigned xbpe = ((xbit?1:0)<<3) | (bbit<<2) | (pbit<<1) | ebit;
                    unsigned byte2 = (xbpe<<4) | ((udisp>>8)&0xF);
                    unsigned byte3 = udisp & 0xFF;
                    ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                        <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3;
                    return oss.str();
                }
            }
            // Otherwise error
            ok=false; errMsg="Invalid displacement";
            return "****";
        } else {
            // No operand; encode with disp=0
            unsigned byte1 = (op | (nbit<<1) | ibit);
            unsigned xbpe = ((xbit?1:0)<<3) | (bbit<<2) | (pbit<<1) | ebit;
            unsigned byte2 = (xbpe<<4);
            unsigned byte3 = 0;
            ostringstream oss; oss<<uppercase<<hex<<setfill('0')
                <<setw(2)<<byte1<<setw(2)<<byte2<<setw(2)<<byte3;
            return oss.str();
        }
    }
}


// 
// Build SYMTAB + LOCCTR & write files
// 
static int assemblePass1Pass2(const string &inputPath) {
	// reset SYMTAB per file
	symCount = 0;
    LITTAB.clear();        
    gCsectName.clear();
    gStartAddr = gEndAddr = 0;

	ifstream in(inputPath);
	if (!in) {
		cerr << "Error opening " << inputPath << "\n";
		return EXIT_FAILURE;
	}

	string stem = stemOf(inputPath);
	string listingName = stem + ".l";

	vector<string> locArr, labelArr, opArr, operandArr, objArr;

	unsigned int LOCCTR = 0;
	bool started = false;
	bool ended = false;
	int lineNum = 0;

    // Size per line
    vector<int> sizeArr;
	
	printf("Processing %s and generating %s...\n\n", inputPath.c_str(), listingName.c_str());
	string line;
	while (std::getline(in, line)) {
		lineNum++;
        // copy entire line into listing?
		if (isComment(line)) continue;

		// tokenize line by spaces 
		auto toks = splitWS(line);
		if (toks.empty()) continue;

		string label = "";
		string opcode = "";
        string operand = "";

		// if there are 1~3+ elements: [label] opcode [operand...]
		if (toks.size() == 1) {
			opcode = toks[0];
		} else if (toks.size() == 2) {
			opcode = toks[0];
			operand = toks[1];
		} else {
			label = toks[0];
			opcode = toks[1];
			ostringstream oss;
			for (size_t k=2; k<toks.size(); k++) {
				if (k>2) oss << " ";
				oss << toks[k];
			}
			operand = oss.str();
		}

if (opcode == "*" && !operand.empty() && operand[0] == '=') {
    label = "*";
    opcode = operand;
    operand.clear();
}

		// Handle START (initialize LOCCTR)
		{
			string up = opcode; for (auto &c: up) c=toupper(c);
			if (!started && up=="START") {
				unsigned int startAddr = 0;
				if (!operand.empty()) {
					// accept hex or decimal; assume hex if contains [A-F]
					bool hasHex = operand.find_first_of("ABCDEFabcdef") != string::npos;
					startAddr = hasHex ? convertHexToDec(operand) : (unsigned int)convertStringToInt(operand);
				}
				LOCCTR = startAddr;
				started = true;
                gStartAddr = LOCCTR;
                gCsectName = label.empty() ? "DEFAULT" : label;

				// Listing row for START (no object code)
				locArr.push_back(convertDecToHex(LOCCTR));
				labelArr.push_back(label);
				opArr.push_back(opcode);
				operandArr.push_back(operand);
                sizeArr.push_back(0);
				continue;
			}
		}

		if (!started) { started = true; gStartAddr = 0; gCsectName = "DEFAULT"; } // default start at 0 if no START

		// If label present, enter into SYMTAB
		if (!label.empty()) {
			// check duplicate
			bool dup = false;
			for (int i=0;i<symCount;i++) {
				if (label == string(SYMTAB[i].label)) { dup=true; break; }
			}
			if (dup) {
				cerr << "Error: Duplicate symbol '"<<label<<"' at line "<<lineNum<<"\n";
			} else if (symCount < MAX_SYMBOLS) {
				SYMTAB[symCount].lineNumber = lineNum;
				SYMTAB[symCount].addr = LOCCTR;
				strncpy(SYMTAB[symCount].label, label.c_str(), sizeof(SYMTAB[symCount].label)-1);
				SYMTAB[symCount].label[sizeof(SYMTAB[symCount].label)-1] = '\0';
				symCount++;
			}
		}

        // If operand is a literal 
        if (!operand.empty() && operand[0]=='=' && !LITTAB.count(operand)) {
            vector<unsigned char> b;
            if (parseLiteral(operand, b)) {
                LITTAB[operand] = Lit{operand, std::vector<unsigned char>(b.begin(), b.end()), 0u, false};
            } else {
                cerr << "Line " << lineNum << ": malformed literal " << operand << "\n";
            }
        }

// opcode literal statement (=C'...' / =X'...') emits bytes here
            if (!opcode.empty() && opcode[0] == '=') {
    vector<unsigned char> b;
    if (!parseLiteral(opcode, b)) {
        cerr << "Line " << lineNum << ": malformed literal " << opcode << "\n";
    } else {
          std::ostringstream obj; obj<<std::uppercase<<std::hex<<std::setfill('0');
        for (auto c: b) obj<<std::setw(2)<<static_cast<unsigned>(c);

        locArr.push_back(convertDecToHex(LOCCTR));
        labelArr.push_back(label.empty() ? "*" : label);
        opArr.push_back(opcode);
        operandArr.push_back("");
        objArr.push_back(obj.str());
        sizeArr.push_back((int)b.size());

        LITTAB[opcode] = Lit{opcode, b, LOCCTR, true};
        LOCCTR += (unsigned)b.size();
    }
    continue; // skip normal opcode/dir size handling for this line
}

		// Record current LOC in listing before increment
		locArr.push_back(convertDecToHex(LOCCTR));
		labelArr.push_back(label);
		opArr.push_back(opcode);
		operandArr.push_back(operand);

		// END?
		{
			string up = opcode; for (auto &c: up) c=toupper(c);
			if (up=="END") {
                // Assign addresses to all UNASSIGNED literals at END (END-only pool)
                for (auto &kv : LITTAB) {
                    auto &lit = kv.second;
                    if (!lit.assigned) {
                        lit.addr = LOCCTR;
                        lit.assigned = true;
                        // add listing row for literal
                        std::ostringstream obj; obj<<std::uppercase<<std::hex<<std::setfill('0');
                        for (auto c: lit.bytes) obj<<std::setw(2)<<(int)c;

                        locArr.push_back(convertDecToHex(lit.addr));
                        labelArr.push_back("*");
                        opArr.push_back(lit.key);
                        operandArr.push_back("");
                      //  objArr.push_back(obj.str());
                        sizeArr.push_back((int)lit.bytes.size());

                        LOCCTR += (unsigned)lit.bytes.size();
                    }
                }
                ended = true; 
                sizeArr.push_back(0);
                break; 
            }
		}

		// Advance LOCCTR
		string upop = opcode; for (auto &c: upop) c=toupper(c);
        int sz = 0;
		if (upop=="START") {
			// already handled
		} else if (isDirective(upop)) {
			sz = instrSizeBytes(upop, operand);
            LOCCTR += sz;
		} else {
			sz = instrSizeBytes(opcode, operand);
            LOCCTR += sz;
		}
        sizeArr.push_back(sz);
	}

    // set end address for CSect length
    gEndAddr = LOCCTR;



	// PASS 2: fill object code
    auto OPTAB = buildOptab();
    unsigned baseReg = 0; bool baseValid = false;

    for (size_t i=0;i<opArr.size();i++) {
        string opcode = opArr[i]; string operand = operandArr[i];
        string up = opcode; for (auto &c: up) c=toupper(c);

        if (up=="START" || up=="END") { 
            if (i >= objArr.size()) objArr.push_back(""); 
            else objArr[i] = ""; 
            continue; 
        }

        if (up=="BASE") {
            // set BASE register value from symbol
            unsigned addr=0;
            if (!operand.empty() && lookupSymAddr(operand, addr)) {
                baseReg = addr; baseValid = true;
            } else {
                // allow numeric base
                if (!operand.empty() && (isdigit((unsigned char)operand[0]) || operand[0]=='-')) {
                    long v = stol(operand); if (v>=0) { baseReg=(unsigned)v; baseValid=true; }
                }
            }
            if (i >= objArr.size()) objArr.push_back(""); else objArr[i] = "";
            continue;
        }

        if (up=="RESW" || up=="RESB") { 
            if (i >= objArr.size()) objArr.push_back(""); else objArr[i] = "";
            continue; 
        }
        if (up=="WORD" || up=="BYTE") {
            string data = encodeData(up, operand);
            if (i >= objArr.size()) objArr.push_back(data); else objArr[i] = data;
            continue;
        }

if (!opcode.empty() && opcode[0] == '=') {
    if (i >= objArr.size()) {
     auto it = LITTAB.find(opcode);
        if (it != LITTAB.end()) {
            std::ostringstream obj; obj<<std::uppercase<<std::hex<<std::setfill('0');
            for (auto c: it->second.bytes) obj<<std::setw(2)<<static_cast<unsigned>(c);
            objArr.push_back(obj.str());
        } else {
            objArr.push_back("");
        }
    }
    continue;
}

        // Compute current and next LOCs from arrays
        unsigned curr = 0, next = 0;
        try { curr = convertHexToDec(locArr[i]); } catch(...) { curr = 0; }
        if (i+1 < locArr.size()) {
            try { next = convertHexToDec(locArr[i+1]); } catch(...) { next = curr; }
        } else {
            next = curr + (sizeArr[i] > 0 ? sizeArr[i] : 0);
        }

        bool ok=true; string err;
        string obj = encodeInstr(OPTAB, opcode, operand, curr, next, baseReg, baseValid, ok, err);
        if (!ok && !err.empty()) {
            cerr << "Line " << (i+1) << ": " << err << "\n";
        }
        if (i >= objArr.size()) objArr.push_back(obj); else objArr[i] = obj;
    }

	// write files
	writeListingFile(listingName, locArr, labelArr, opArr, operandArr, objArr);

    // write symtab in required format to ONE shared symtab.st
    // (appends a block per input; main() truncates once at program start)
    // writes symtab into a single shared file: symtab.st (append per source file)
    {
        const std::string outName = "symtab.st";
        FILE *outFile = fopen(outName.c_str(), "a"); // APPEND
        if (outFile == NULL) {
            perror("Couldn't write SymTab to file");
        } else {
            // Header block per your format
            fprintf(outFile, "CSect   Symbol  Value   LENGTH  Flags:\n");
            fprintf(outFile, "--------------------------------------\n");

            unsigned length = (gEndAddr >= gStartAddr) ? (gEndAddr - gStartAddr) : 0;

            // CSect summary line (name + value + length)
            fprintf(outFile, "%-8s %-7s %06X  %06X\n",
                    gCsectName.empty() ? "DEFAULT" : gCsectName.c_str(),
                    "", gStartAddr, length);

            // symbols sorted by address
            std::vector<Symbol> syms(SYMTAB, SYMTAB + symCount);
            std::sort(syms.begin(), syms.end(),
                      [](const Symbol &a, const Symbol &b) { return a.addr < b.addr; });

            for (const auto &s : syms) {
                fprintf(outFile, "%-8s %-7s %06X  %-6s  %s\n",
                        "",             // (empty CSect column for symbol rows)
                        s.label,        // Symbol
                        s.addr,         // Value
                        "",             // LENGTH column blank for symbol rows
                        "R");           // Flags
            }

            fprintf(outFile, "\n");
            // Literal table section
            fprintf(outFile, "Literal Table \n");
            fprintf(outFile, "Name  Operand   Address  Length:\n");
            fprintf(outFile, "--------------------------------\n");

            // Print collected literals (END-only pool)
            for (const auto &kv : LITTAB) {
                const auto &lit = kv.second;
                // Name: try to show a simple name like EOF for =C'EOF'; otherwise use the literal without "=X'/=C'"
                std::string key = lit.key;
               { 
               size_t p = key.find_first_not_of(" \t");
        if (p != std::string::npos) key.erase(0, p);

        if (!key.empty() && key[0] == '*') {
size_t eq = key.find('=');
            if (eq != std::string::npos) {
                key = key.substr(eq);
            }
         } 
       }
                std::string operandHex;
                std::string name = key;
                if (key.size()>=4 && key[0]=='=' && (key[1]=='C' || key[1]=='c') && key[2]=='\'') {
                    // name: content, operand: hex of bytes
                    size_t end = key.find_last_of('\'');
                    name = (end==string::npos) ? key : key.substr(3, end-3);
                } else if (key.size()>=4 && key[0]=='=' && (key[1]=='X' || key[1]=='x') && key[2]=='\'') {
                    size_t end = key.find_last_of('\'');
                    name = (end==string::npos) ? key : key.substr(3, end-3);
                }

                std::ostringstream hex; hex<<std::uppercase<<std::hex<<std::setfill('0');
                for (auto c: lit.bytes) hex<<std::setw(2)<<(int)c;
                operandHex = hex.str();

                fprintf(outFile, "%-5s %-8s %06X   %d\n",
                        name.c_str(), operandHex.c_str(), lit.addr, (int)lit.bytes.size());
            }

            fprintf(outFile, "\n");
            fclose(outFile);
            printf("\nSymbol table appended to %s\n", outName.c_str());
        }
    }

	if (!ended) {
		cerr << "Warning: no END directive encountered in " << inputPath << "\n";
	}

	return EXIT_SUCCESS;
}

//  MAIN

int main(int argc, char* argv[]) {
	// supports multiple inputs
	if (argc < 2) {
		cerr << "Usage: asx <file1.sic> [file2.sic ...]\n";
		cerr << "No input files provided; exiting.\n";
		return EXIT_FAILURE;
	}

    //produces symtab 
    { std::ofstream trunc("symtab.st", std::ios::trunc); }

	int overallRC = 0;
	for (int i=1; i<argc; i++) {
		string path = argv[i];
		cout << "Processing " << path << " ...\n";
		int rc = assemblePass1Pass2(path);
		if (rc != 0) overallRC = rc;
	}

	return (overallRC==0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
