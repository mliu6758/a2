LOC   SYMBOL    OPCODE      OPERAND           OBJECT
----------------------------------------------------------------
0     SUM       START       0                 
0     FIRST     LDX         #0                050000
3               LDA         #0                010000
6               +LDB        #TABLE2           69101794
A               BASE        TABLE2            
A     LOOP      ADD         TABLE,X           1BA017
D               ADD         TABLE2,X          1BC000
10              TIX         COUNT             2F200E
13              JLT         LOOP              3B2FF4
16              +STA        TOTAL             0F102F04
1A              RSUB                          4F0000
1D    MYLIT     +LDA        =C'EOF'           03102F07
21    COUNT     RESW        1                 
24    TABLE     RESW        2000              
1794  TABLE2    RESW        2000              
2F04  TOTAL     RESW        1                 
2F07  *         =C'EOF'                       454F46
2F0A            END         FIRST             
