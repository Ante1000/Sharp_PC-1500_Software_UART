SERIN / SEROUT v6.2 and v7.1 - LH5801 assembler sources for the Sharp PC-1500(A) software UART

serout_v6.2.asm / .lst   SEROUT (transmit), 84 bytes, loaded at RAM+&0C5 (&40C5 on a PC-1500A)
serin_v6.2.asm  / .lst   SERIN (receive), 118 bytes, loaded at RAM+&130 (&4130 on a PC-1500A)
serout_v7.1.asm / .lst   SEROUT v7.1, 19200 bps only, 82 bytes, at RAM+&0C5
serin_v7.1.asm  / .lst   SERIN v7.1, 19200 bps only, 162 bytes, at RAM+&130
                         (timing pads P1..P3, Q1..Q4, R1..R4 and the branches RS: see
                         CALIBRATION_19200.md)

The installer v7.2 (Basic/pc1500_uart_installer-v7.2.txt) POKEs the v6.2 code for 1200..9600 bps
and the v7.1 code for 19200 bps; both use the same addresses and the same CALLs.

The machine code of v6.2 is the same as v6.1. The .lst files show the address offset
and the bytes of every instruction.

Some operands are BASIC variables that the installer fills in when it POKEs the code:
  KB, KH, KT, KS, KI   timing constants for 1200/2400/4800/9600 bps (v7.1: KI, KT)
  OM VM / OS VS        MARK/SPACE writes to PC7 (normal or inverted polarity)
  FM, FS, RM           branch opcodes for line = mark/space (normal or inverted)
  DB, BM               port B direction mask and input bit (PB0 or PB2)
  TP, RP, CP           high bytes of the TX buffer, RX buffer and RC counter
  P1..P3, Q1..Q4, R1..R4   v7.1 timing pads (TX bit, RX sample point, RX bit)
See the comment blocks at the top of each source for details.
