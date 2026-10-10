; =====================================================================
; SEROUT v7.1 - software UART transmitter 8N1 for the Sharp PC-1500(A),
; 19200 bps, normal or inverted polarity (chosen in the installer).
; CPU LH5801 @ 1.3 MHz, output PC7 (&F008 bit 7).
;   normal (TTL):  idle and bit "1" = high, start bit and "0" = low
;   inverted:      idle and bit "1" = low,  start bit and "0" = high
;
; Call from BASIC (as in v6.x):
;   CALL SO          send 1 character from TX+0
;   CALL SO,N        send N characters from TX+0.. (1..255; 0 = 1, >255 = 255)
; The variable N is NOT changed (returns with C=0).
; TX buffer: 255 bytes at the start of a memory page (TX = RAM+&300).
;
; Timing. Cycle counts are those of MAME's LH5801 table (1 cycle = 0.769 us
; at 1.3 MHz); 19200 bps = 67.7 cycles per bit. The calibration on a real
; PC-1500 measured that a bit of SEROUT v7.0 took 73.3 real cycles where
; the MAME table gives 67, i.e. about 6.3 cycles more. v7.1 therefore has a
; shorter loop:
;   one data bit = TB = 50 + PAD cycles (MAME), PAD = 8, 10..15:
;   TB = 58, 60..65; default 62 (51.7..52.5 us on the measured PC-1500,
;   depending on which instructions take the extra time).
; - The bit counter is UL with LOP (8 bits), instead of a sentinel bit and
;   BII A,&FF + BZR (7 cycles less).
; - Both branches (bit 0 / bit 1) take 33 cycles; the port write comes 3
;   cycles later for a 1 bit than for a 0 bit (moves single edges by 2.3 us,
;   does not add up).
; - The character counter is XL (the buffer starts a page): CPI XL,N at the
;   end of the stop bit; N is written into it at the start (the code changes
;   one of its own bytes, at SO+CMPN+1). UH is free for the timing pad.
; - Start bit = TB (19 fixed cycles + the same PAD), last data bit = TB
;   (+11 cycles after the loop), stop bit = 55 + 11*KT cycles (KT = 1: 66).
; Writes to PC7 are the pseudo instructions MARK / SPACE (FD op imm):
;   MARK  #(Y) = FD OM VM   normal ORI #(Y),&80, inverted ANI #(Y),&7F
;   SPACE #(Y) = FD OS VS   normal ANI #(Y),&7F, inverted ORI #(Y),&80
;
; Load address: SO = RAM+&0C5 (&40C5).
; Symbols filled in by BASIC: TP = high byte of the TX buffer address,
; CP = high byte of RAM+&100 (page of CMPN), KI = idle before the first
; start bit, KT = stop bit, P1 P2 P3 = timing pad, OM/VM/OS/VS = polarity.
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
NOK:    STA  (CP,&C5+CMPN+1-&100)   ; N into CPI XL,N (SO = RAM+&0C5)
        LDI  XH,TP          ; X = TX buffer (start of a page)
        LDI  XL,0
        LDI  YH,&F0         ; Y = &F008 (port C)
        LDI  YL,&08
        MARK #(Y)           ; PC7 = idle (mark) ...
        LDI  UL,KI          ; ... for about 1 bit before the first start bit
IDLE:   LOP  UL,IDLE
CHAR:   LIN  X              ; A = character, X++
        SPACE #(Y)          ; start bit (space)
        PAD  3,P            ; start bit: the same pad as a data bit ...
        LDI  UL,7           ; ... plus 19 cycles; UL = 8 data bits for LOP
        BCH+ S1
S1:     NOP
        SHR                 ; C = bit 0
OUT:    BCS+ ONE            ; send the bit in C
        SPACE #(Y)          ; bit 0 (space)
        BCH+ NXT
ONE:    MARK #(Y)           ; bit 1 (mark)
        NOP                 ; (both branches 33 cycles)
NXT:    PAD  3,P            ; timing pad: 8 or 10..15 cycles
        SHR                 ; C = next bit
        LOP  UL,OUT         ; 8 bits
        CPI  UL,0           ; last data bit as long as the others
        REC                 ; (11 cycles)
        MARK #(Y)           ; stop bit (mark)
        LDI  UL,KT          ; length of the stop bit
TDLY:   LOP  UL,TDLY
CMPN:   CPI  XL,0           ; N characters sent? (N written at the start)
        BZR- CHAR           ; next character
        REC                 ; C=0: variable N unchanged
        RTN
