# CS530Assignment2

Names: Mandy Liu, Jasmine Rodelas
Accounts: cssc2119, cssc1454

Class: CS480 - Operating Systems
Assignment: Programming Assignment #2
File: README
Instructor: Guy Leonard

Test Account: cssc2119

File Manifest: 
- asx.cpp: Source code
- Makefile: Compiles asx
- program.sic: Example SIC/XE input source file
- program.l: Outputted listing file (location, opcode, and object code)
- symtab.st: Outputted shared symtab and littab
- README

Compile Instructions:
1. ssh cssc2119@edoras.sdsu.edu
2. cd a2
3. Compile using make

Operating Instructions:
1. Run using ./asx program.sic (Can input multiple files)

Significant Design Decisions:
- 2 pass assembly: Pass 1 builds symtab and computes  addresses. Pass 2 generats object code for each instruction
- Address mode parsing
- Used map to store optab
- Used END-only literal pool to implement literal handling

Extra Features:
- Proper error handling with graceful exits
- Handles multiple .sic inputs

Known Deficiencies:
- Limited to our current subset of XE instructions (can be fixed by adding more)

Lessons learned:
- Gained deeper understanding of the 2 pass assembler
- Designed data structures for symtab, littab, and opcode tables
- Demonstrated understanding of format lengths, addressing modes, and relocation handling
