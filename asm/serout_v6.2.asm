; SEROUT v6.2: the code UART7.3 installs for 1200..9600 bps.
; Machine code identical to v6.1 (v6.2 only changed BASIC prompts and tests).
;
; =====================================================================
; SEROUT v6.1 - software UART transmitter 8N1 for the Sharp PC-1500(A),
; 1200 / 2400 / 4800 / 9600 bps, normal or inverted polarity
; (chosen in UART7.3). CPU LH5801 @ 1.3 MHz, output PC7 (&F008 bit 7).
;   normal (TTL):  idle and bit "1" = high, start and "0" = low
;   inverted:      idle and bit "1" = low,  start and "0" = high
;
; Call from BASIC:
;   CALL SO          send 1 character from TX+0
;   CALL SO,N        send N characters from TX+0.. (1..255; 0 = 1 char, >255 = 255)
; The variable N is NOT changed (return with C=0).
; TX buffer: 255 bytes from the start of a memory page (TX = RAM+&300 = &4300..&43FE).
;
; Code as in v6.0; the writes to PC7 are pseudo-instructions MARK / SPACE (FD op imm),
; whose opcode and operand UART7.3 writes:
;   MARK  #(Y) = FD OM VM   normal ORI #(Y),&80, inverted ANI #(Y),&7F
;   SPACE #(Y) = FD OS VS   normal ANI #(Y),&7F, inverted ORI #(Y),&80
; ANI and ORI take the same time (17 cycles), so the timing does not change.
; Only the constants of the LOP loops depend on the speed;
; UART7.3 writes them (BASIC variables in the POKE lines):
;   KB = n   data bit: 79 + 11*n cycles (MAME table; instruction +3)
;   KS = n+2 start bit (as long as a data bit)
;   KT       stop bit: frame (start + 8 bits + stop) >= 10.1 bits
;   KI = n+7 idle before the first character (about 1 bit)
;   bps    1 bit [cycles]  KB   KT
;   1200      1083.3       91  106
;   2400       541.7       42   50
;   4800       270.8       17   26
;   9600       135.4        5   10   (= v55, tested on a PC-1500A)
; Both branches (bit 0 and bit 1) have the same length, and the port is
; written in them at the same moment (+-1 cycle).
; Bit counter: a sentinel - SEC+ROR puts a 1 into bit 7, after the
; eighth SHR the accumulator = 0. Z is tested with BII A,&FF, because
; according to the LH5801 manual SHR/ROR do not set the Z flag.
; Character counter in UH (1..255), no variables in memory.
;
; Load address: SO = RAM+&0C5 (&40C5).
; Symbols filled in by BASIC: TP = high byte of the TX buffer address,
; KI, KS, KB, KT = timing constants, OM/VM/OS/VS = polarity (see above).
; The POKE lines of UART7.3 were generated from this file.
; =====================================================================
        RIE                 ; no interrupts (timing!)
        LDA  XH
        BZS+ SMALL          ; N = 0..255
        BII  A,&80          ; CALL SO without a variable: X = ROM address (XH>=&80)
        BZR+ N1             ;   -> 1 character
        LDI  A,&FF          ; N = 256..32767 -> 255
        BCH+ NOK
SMALL:  LDA  XL
        BZR+ NOK            ; N = 1..255
N1:     LDI  A,1            ; N = 0 -> 1 character
NOK:    STA  UH             ; UH = number of characters (1..255)
        LDI  XH,TP          ; X = TX buffer (start of a page)
        LDI  XL,0
        LDI  YH,&F0         ; Y = &F008 (port C)
        LDI  YL,&08
        MARK #(Y)           ; PC7 = idle (mark) ...
        LDI  UL,KI          ; ... for ~1 bit before the first start bit
IDLE:   LOP  UL,IDLE
CHAR:   LIN  X              ; A = character, X++
        SPACE #(Y)          ; start bit (space)
        SEC
        ROR                 ; C = bit 0, sentinel 1 -> bit 7 of A
        LDI  UL,KS          ; length of the start bit
SDLY:   LOP  UL,SDLY
        NOP
OUT:    BCS+ ONE            ; send the bit in C
        REC                 ; (alignment: the write in the same cycle)
        SPACE #(Y)          ; bit 0 (space)
        BCH+ BDLY
ONE:    MARK #(Y)           ; bit 1 (mark)
        NOP                 ; (equal branch lengths)
        SEC
BDLY:   LDI  UL,KB          ; length of a data bit
DLY:    LOP  UL,DLY
        REC                 ; (fine tuning: +4 cycles)
        SHR                 ; C = next bit
        BII  A,&FF          ; A = 0 -> 8 bits sent
        BZR- OUT
        NOP                 ; (alignment of the last data bit)
        NOP
        REC
        MARK #(Y)           ; stop bit (mark)
        LDI  UL,KT          ; length of the stop bit (about 1.1 bits)
TDLY:   LOP  UL,TDLY
        DEC  UH
        BZR- CHAR           ; next character
        REC                 ; C=0: variable N unchanged
        RTN
