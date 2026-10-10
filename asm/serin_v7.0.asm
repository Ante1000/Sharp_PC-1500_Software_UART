; =====================================================================
; SERIN v7.0 - software UART receiver 8N1 for the Sharp PC-1500(A),
; 19200 bps, input PB0 (pin 9) or PB2 (CMT-IN, pin 27), normal polarity
; (TTL: idle = high) or inverted (idle = low) - chosen in the installer.
; CPU LH5801 @ 1.3 MHz.
;
; Call from BASIC (as in v6.x):
;   CALL SI          receive up to 255 characters
;   CALL SI,M        receive at most M characters (1..255; 0 or >255 = 255)
; Result:
;   RC               number of characters received, 1 byte, RC = RX-1
;   RX..RX+254       the characters (RX = start of a memory page, &4200)
;   M                (only with CALL SI,M) = number of characters received
; Time limits: the first character must come within ~30 s, the next ones
; within ~0.5 s of each other; after the M-th (or 255th) character the
; routine returns at once.
;
; Timing (MAME cycle table, 1 cycle = 0.769 us; 19200 bps = 67.7 cycles):
; - Bit loop: RB = 51 + PAD R cycles (R = 12..20: RB = 63..71, default 66).
;   Both paths (bit 0 / bit 1) take the same time. Counter: sentinel in A
;   (LDI A,&80; after 8 ROR the 1 leaves into C), so no DEC/branch per bit.
; - Start bit: the waiting loop polls the line every 33 cycles (with the
;   time-out counter) - half a bit at 19200, so the edge is known to about
;   +-1/4 bit. After each character 7 fast polls without a counter follow
;   (every 22 cycles, edge known to about +-1/6 bit): characters sent
;   back to back are caught there. FSTART adds 5 cycles for the fast
;   polls, so both entries sample at the same place.
; - PAD Q (8 or 12..20 cycles, default 12) moves all samples: the first
;   data bit is read about 1.5 bits after the start edge.
; - The middle of the start bit is checked (noise filter), as in v6.x.
; - DEC sets C when the value was not 0 before, and BII does not change C,
;   so polls can sit between DEC and the BCS that tests it: no long gaps.
; Branches that depend on the line are pseudo instructions whose opcode
; the installer writes (BZR and BZS take the same time):
;   BMK+ = FM  forward if mark   (normal BZR+ &89 / inverted BZS+ &8B)
;   BSP+ = FS  forward if space  (normal BZS+ &8B / inverted BZR+ &89)
;   BMK- = RM  back if mark      (normal BZR- &99 / inverted BZS- &9B)
;   BSP- = RS  back if space     (normal BZS- &9B / inverted BZR- &99)
; Other BASIC variables written by the installer:
;   DB = &FE (PB0) / &FB (PB2)   mask for the direction register DDB
;   BM = 1 (PB0) / 4 (PB2)       bit mask of the input in port B (&F00F)
;   Q1..Q4, R1..R4               timing pads (sample position, bit length)
;   RP = page of the RX buffer, CP = RP-1 (page of RC and of this code)
;
; Load address: SI = RAM+&130 (&4130). M is written into the CPI XL,M
; below (the code changes one of its own bytes, at SI+CMPM+1).
; =====================================================================
        RIE                     ; no interrupts (timing!)
        LDA  XH                 ; no variable (ROM address) or M > 255
        BZR+ MAXM               ;   -> 255 characters
        LDA  XL                 ; CALL SI,M: X = M
        BZR+ SETM               ; M = 1..255
MAXM:   LDI  A,&FF              ; M = 0 -> 255 (whole buffer)
SETM:   STA  (CP,&30+CMPM+1)    ; M into CPI XL,M (SI = RAM+&130)
        LDI  XH,RP              ; X = RX (start of a page)
        LDI  XL,0
        LDI  YH,&F0             ; Y = &F00D (DDB, direction register of port B)
        LDI  YL,&0D
        ANI  #(Y),DB            ; PB0 or PB2 = input
        LDI  YL,&0F             ; Y = &F00F (port B)
        LDI  UH,255             ; time-out of the 1st character:
        LDI  A,17               ; 18*256*256 polls, about 30 s
WH:     BII  #(Y),BM            ; wait for the idle level (mark)
        BMK+ WL
        LOP  UL,WH
        DEC  UH
        BCS- WH
        DEC  A
        BCS- WH
        BCH+ DONE               ; time-out
WL:     BII  #(Y),BM            ; wait for the start bit (space), poll every 33 cycles
        BSP+ SSTART
        LOP  UL,WL
        BII  #(Y),BM            ; UL ran out: keep polling between the counters
        BSP+ SSTART
        DEC  UH
        BII  #(Y),BM
        BSP+ SSTART
        BCS- WL
        DEC  A
        BII  #(Y),BM
        BSP+ SSTART
        BCS- WL
        BCH+ DONE               ; time-out
FSTART: NOP                     ; entry from the fast polls (edge known better)
SSTART: BII  #(Y),BM            ; middle of the start bit: still space?
        BMK- WL                 ; no: noise, wait again
        LDI  A,&80              ; sentinel: leaves into C after 8 bits
        PAD  4,Q                ; timing: position of the samples
BIT:    PAD  4,R                ; timing: length of a bit
        BII  #(Y),BM            ; read the bit
        BSP+ ZERO
        SEC                     ; mark: 1
        BCH+ JOIN
ZERO:   REC                     ; space: 0
        NOP                     ; (both paths 34 cycles)
JOIN:   ROR                     ; LSB first; C = sentinel after the 8th bit
        BCR- BIT
        SIN  X                  ; store the character, X++
CMPM:   CPI  XL,0               ; M characters received? (M written at the start)
        BZS+ DONE
        LDI  UH,76              ; time-out between characters:
        LDI  A,0                ; 77*256 polls, about 0.5 s
        BII  #(Y),BM            ; 7 fast polls for the next start bit; the
        BSP- FSTART             ; first one about 1.2 bits after the last
        BII  #(Y),BM            ; data bit was read, before the next start
        BSP- FSTART             ; bit of characters sent back to back
        BII  #(Y),BM
        BSP- FSTART
        BII  #(Y),BM
        BSP- FSTART
        BII  #(Y),BM
        BSP- FSTART
        BII  #(Y),BM
        BSP- FSTART
        BII  #(Y),BM
        BSP- FSTART
        BCH- WL                 ; then the polls with the time-out
DONE:   LDA  XL                 ; number of characters = XL (RX starts a page)
        STA  (CP,&FF)           ; RC = number of characters
        LDI  XH,0               ; X = number of characters ...
        SEC                     ; ... C=1: BASIC puts X into the variable of CALL SI,M
        RTN
