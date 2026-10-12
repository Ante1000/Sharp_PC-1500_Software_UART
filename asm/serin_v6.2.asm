; SERIN v6.2: the code UART7.3 installs for 1200..9600 bps.
; Machine code identical to v6.1 (v6.2 only changed BASIC prompts and tests).
; Update (October 2026): SIE before the final RTN switches the interrupts on
; again, as BASIC had them before the CALL (RIE at the start switches them off
; for the timing). 2 bytes more; the timing and all other bytes are unchanged.
;
; =====================================================================
; SERIN v6.1 - software UART receiver 8N1 for the Sharp PC-1500(A),
; 1200 / 2400 / 4800 / 9600 bps, input PB0 (pin 9) or PB2 (CMT-IN,
; pin 27), normal polarity (TTL: idle = high) or
; inverted (idle = low) - chosen in UART7.3.
; CPU LH5801 @ 1.3 MHz.
;
; Call from BASIC:
;   CALL SI          receive up to 255 characters
;   CALL SI,M        receive at most M characters (1..255; 0 or >255 = 255)
; Result:
;   RC               number of characters received, 1 byte (0..255), RC = RX-1
;   RX..RX+254       received characters (RX = start of a memory page, &4200)
;   M                (only with CALL SI,M) = number of characters received
;                    (return with C=1 -> the ROM writes X into the variable of CALL)
; Time:
;   the first character must come within ~30 s, the next ones < ~0.5 s apart;
;   after the M-th (or 255th) character it returns at once. The time-outs do
;   not depend on the baud rate.
;
; Code as in v6.0; the jumps that depend on the line state are pseudo-instructions
; whose opcode UART7.3 writes (BZR and BZS take the same time, timing unchanged):
;   BMK+ = FM   jump forward,  if line = mark   (BZR+ &89 / inv. BZS+ &8B)
;   BSP+ = FS   jump forward,  if line = space  (BZS+ &8B / inv. BZR+ &89)
;   BMK- = RM   jump backward, if line = mark   (BZR- &99 / inv. BZS- &9B)
; Other BASIC variables written by UART7.3:
;   DB = &FE (PB0) / &FB (PB2)  mask of the direction register DDB (bit = 0 -> input)
;   BM = 1 (PB0) / 4 (PB2)      mask of the input bit in port B (&F00F)
;   KB                          bit loop: 79.5 + 11*KB cycles (MAME; instruction +3)
;   KH                          delay to the middle of the start bit
;   bps    KB   KH   (KH chosen in the simulator: middle of the tolerance for
;   1200   91   47    both cycle tables; 9600 = v55, tested on a PC-1500A)
;   2400   42   21
;   4800   17   12
;   9600    5    3
;
; Load address: SI = RAM+&130 (&4130).
; Symbols filled in by BASIC: RP = page of the RX buffer (RX = RP*256),
; CP = RP-1 (counter RC = CP*256+&FF = RX-1).
; The RX buffer starts at the start of a page, so the number of characters = XL.
; Jumps: the LH5801 has separate opcodes for forward (+) and backward (-) jumps;
; the offset is ALWAYS positive (0..255), counted from the next instruction.
; The POKE lines of UART7.3 were generated from this file.
; =====================================================================
        RIE                 ; no interrupts (timing!)
        LDA  XH             ; no variable (ROM address) or M>255
        BZR+ MAXM           ;   -> 255 characters
        LDA  XL             ; CALL SI,M: X = M
        BZS+ MAXM           ; M = 0 -> 255 characters
        DEC  A              ; r = M-1 (0..254)
        BCH+ SETM
MAXM:   LDI  A,&FE          ; r = 254 -> 255 characters (whole buffer)
SETM:   STA  (CP,&FF)       ; RC = r (r+1 characters left)
        LDI  XH,RP          ; X = RX (start of a page)
        LDI  XL,0
        LDI  YH,&F0         ; Y = &F00D (DDB, direction register of port B)
        LDI  YL,&0D
        ANI  #(Y),DB        ; PB0 or PB2 = input
        INC  Y
        INC  Y              ; Y = &F00F (port B)
        LDI  UH,&00         ; time-out of the 1st character: 18*256*256 loops ~ 30 s
        LDI  A,18
WH:     BII  #(Y),BM        ; wait for the idle state (1 = mark)
        BMK+ WL             ; line = mark -> wait for the start bit
        LOP  UL,WH          ; 256 loops x 33 cycles
        DEC  UH
        BZR- WH
        DEC  A
        BZR- WH
        BCH+ DONE           ; time-out
WL:     BII  #(Y),BM        ; wait for the edge 1->0 (start bit)
        BSP+ START          ; line = space -> start bit
        LOP  UL,WL
        DEC  UH
        BZR- WL
        DEC  A
        BZR- WL
        BCH+ DONE           ; time-out
START:  LDI  UL,KH          ; ~half of the start bit
HALF:   LOP  UL,HALF
        BII  #(Y),BM        ; middle of the start bit: still 0?
        BMK- WL             ; no -> noise, keep waiting
        LDI  UH,8           ; 8 data bits
BIT:    LDI  UL,KB          ; bit loop (KB as in SEROUT)
DLY:    LOP  UL,DLY
        NOP                 ; (fine tuning: +10 cycles)
        NOP
        BII  #(Y),BM
        SEC
        BMK+ ONE            ; mark -> bit = 1
        REC
ONE:    ROR                 ; LSB first
        DEC  UH
        BZR- BIT
        SIN  X              ; store the character, X++
        LDA  (CP,&FF)       ; r = 0 -> that was the last allowed character
        BZS+ DONE
        DEC  A
        STA  (CP,&FF)
        LDI  UH,77          ; time-out between characters: 77*256 loops ~ 0.5 s
        LDI  A,1
        BCH- WH             ; wait for the stop bit and the next character
DONE:   LDA  XL             ; number of characters = XL (RX starts a page)
        STA  (CP,&FF)       ; RC = number of characters received
        LDI  XH,0           ; X = number of characters ...
        SEC                 ; ... C=1: BASIC writes X into the variable of CALL SI,M
        SIE                 ; interrupts on again for BASIC
        RTN
