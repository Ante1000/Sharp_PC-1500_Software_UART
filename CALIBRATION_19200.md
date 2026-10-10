# SERINOUT v7.0: 19200 bps, and its calibration with an Arduino UNO

> **Status: test version.** SEROUT and SERIN v7.0 have so far only run in an LH5801 simulator
> (cycle counts from MAME, plus the extra time of port instructions measured on a real PC-1500,
> see below). Before they are used, their three timing constants are measured and set on a real
> PC-1500 with the Arduino calibration on this page. Please send the calibration report.

SERINOUT v7.0 is used exactly like v6.2: the same `CALL`s, buffers and addresses
([README](README.md#commands)), but at **19200 bps only** (8N1, normal or inverted polarity, RX on
PB2 or PB0). It moves up to about 1.9 KB/s, twice the 9600 bps of v6.2.

## Files

| File | Contents |
|---|---|
| `Basic/pc1500_uart_installer-v7.0_19200.txt` | BASIC installer of SERINOUT v7.0 (19200 bps), with the TX/RX test of v6.2 |
| `Basic/pc1500_uart19200_calibration-v1.0.txt` | PC-1500 calibration program: talks to the Arduino, can set new timing constants |
| `arduino/UART_Calibration/` | Arduino UNO sketch that measures the timing and prints the report (`UART_Calibration.ino`, `analysis.h`) |
| `asm/serout_v7.0.asm`, `asm/serin_v7.0.asm` | LH5801 sources (and `.lst` listings) of SEROUT and SERIN v7.0 |

## Why 19200 needs new code

At 19200 bps a bit lasts 52.08 µs, that is 67.7 cycles of the LH5801 at 1.3 MHz. The bit loops of
v6.x need at least 79 cycles (they contain a delay loop `LDI`/`LOP` and a bit counter), so they
cannot go faster than about 16 000 bps. v7.0 has new loops without a delay loop:

- **SEROUT:** both branches (bit 0 / bit 1) take 33 cycles, then a 2-byte timing pad (6–10
  cycles), `SHR`, `BII A,&FF` and the branch: one bit = **TB = 57 + pad = 63…67 cycles**. The
  start bit and the last data bit have the same length; the stop bit is 1.03 bits.
- **SERIN:** the bit counter is a sentinel bit in A (after 8 `ROR` it leaves into C), the two
  paths of a bit take the same time, and a 4-byte pad sets the length: **RB = 51 + pad =
  63…71 cycles**. Another 4-byte pad, **RQ** (8 or 12…20 cycles), moves all 8 samples.
- **Finding the start bit** is the hard part of a software UART: the line is polled, so the
  edge is only known to within one poll. The loop with the 30 s / 0.5 s time-out polls every 33
  cycles (half a bit at 19200). After each character SERIN therefore polls 7 times without a
  counter, every 22 cycles (a third of a bit): characters that are sent back to back, as most
  devices do, are found there, more precisely. The middle of the start bit is checked (noise
  filter), as in v6.x.

**Port instructions are slower on a real PC-1500.** The I2C master measured on a real PC-1500
showed that every `ORI`/`ANI` on a port takes about 1.9 cycles longer than in MAME's table (I2C
v1.2 and v1.3 have the same number of port instructions in the measured transfer and were both
slower by the same 1397 cycles, although v1.3 has 144 instructions less). One TX bit has one
such instruction, so the default **TB = 66** gives 67.9 real cycles: 52.2 µs, +0.3 %. For the
`BII` that reads the RX line the extra time is not known yet; the defaults **RB = 66, RQ = 12**
assume the same 1.9 cycles. This is what the calibration measures.

In the simulator (with these 1.9 cycles) SERIN v7.0 receives without errors when the sender is
3 % slower to 2 % faster than 19200 bps; the samples lie within ±0.19 bit of the middle of the
bits for characters sent back to back and within ±0.26 bit after pauses. 38400 bps is not
possible: a bit would have only 34 cycles.

## Timing constants

| Constant | Meaning | Values | Default |
|---|---|---|---|
| TB | length of a TX bit in MAME cycles | 63…67 | 66 |
| RB | length of an RX bit (time between two samples) in MAME cycles | 63…71 | 66 |
| RQ | position of the RX samples (larger = later), cycles | 8, 12…20 | 12 |

The installer uses the defaults when you press ENTER at `TIMING (ENTER=STD,1=SET)`; answer `1` to
type other values. The calibration program can also change them in the installed code (answer
`1` at `NEW TIMING`), so the installer does not have to be typed in again. The values are stored
at RAM+&1FC…&1FE.

## Calibration

### What you need

- Arduino UNO (5 V); the LCD Keypad Shield is optional, it shows the progress
- the wiring of the I2C tester ([I2C interface](https://github.com/Ante1000/Sharp_PC-1500_I2C_interface)):

| PC-1500 (60-pin) | In between | Arduino UNO |
|---|---|---|
| PC7 (TX), pin 10 | Schottky diode (BAT43/BAT85), **cathode (stripe) toward PC7**, or a 1 kΩ resistor | **A5** |
| PB2 (RX), pin 27 | 470 Ω (1–10 kΩ) | **A4** |
| GND, pins 52–55 | – | GND |

With the diode, a 4.7 kΩ resistor from A5 to 5 V is recommended (faster rising edges); the
sketch also switches on the internal pull-up. The PC-1500 must be out of the CE-150.

### Steps

1. On the PC-1500, in PRO mode: `NEW0`, then `NEW &4400` (other RAM configurations:
   [HELP_NEW_memory.md](HELP_NEW_memory.md)).
2. Type in `Basic/pc1500_uart_installer-v7.0_19200.txt` and `RUN` it: RX port ENTER (PB2),
   inversion ENTER (no), timing ENTER (defaults). The TX/RX tests can be skipped with `0`.
3. In PRO mode enter `NEW` (removes the installer, the code stays), then type in
   `Basic/pc1500_uart19200_calibration-v1.0.txt`.
4. Upload `arduino/UART_Calibration/UART_Calibration.ino` (with `analysis.h` in the same folder)
   to the UNO and open the serial monitor at **115200 bps**.
5. `RUN` the calibration program on the PC-1500. It shows the installed TB, RB and RQ, asks
   `NEW TIMING (1=YES)?` (press ENTER the first time) and `ARDUINO READY? ENTER`.
6. The PC-1500 sends a header and 64 × 'U', then answers the Arduino block by block
   (`BLOCKS: 10`, `20`, …, less than a minute). About 30 s after the last block it shows
   `END: 44 BLOCKS`. The Arduino prints the report as soon as it has finished.
7. Copy the report from the serial monitor, from the line
   `=== PC-1500 SERINOUT v7.0 19200 calibration report ===` to the line `=== end of report ...`,
   and send it.
8. To try other constants (for example the `SUGGESTED` ones), `RUN` the calibration program
   again and answer `1` at `NEW TIMING`; it writes them into SEROUT/SERIN and measures again.

If the program is stopped with BREAK during the echo loop, enter `RUN 210`: during the loop
SEROUT sends from the RX buffer, `RUN 210` switches it back to the TX buffer.

### The report

| Line | Meaning |
|---|---|
| `TX 64 x 'U': bit … us` | length of a PC-1500 TX bit (falling-to-falling edges, 2 bits each); ideal 52.08 µs |
| `stop bit` | length of the stop bit in bits (about 1.03) |
| `RX probe fast` / `RX probe slow` | when SERIN reads each data bit, in µs after the start edge sent by the Arduino: earliest, mean and latest sample; ideal = middle of the bit. "fast" = characters back to back (fast polls), "slow" = 4-bit pauses (polls with the time-out) |
| `fit` | first sample and the time between samples (the RX bit) from the mean values |
| `worst sample vs middle of bit` | the earliest and latest sample of all bits, in bits; the limit is ±0.5 |
| `Random data …` | errors in 1020 random bytes, back to back and with random pauses |
| `Baud margin …` | errors per 255 bytes when the Arduino sends 6 % slower … 6 % faster: the wider the range with 0, the better |
| `SUGGESTED: TB= RB= RQ=` | constants computed from the measurement |

### How the measurement works

- **TX:** the comparator of the UNO (1.1 V against A5) passes every edge of PC7 to the input
  capture of Timer1, which stores the time with 62.5 ns resolution. Inside a 'U' (0x55) frame
  the falling edges are exactly 2 bits apart.
- **RX:** the Arduino sends "probe" frames: a start bit, then the line stays at space until a
  time τ and goes back to mark. Each sample of SERIN before τ reads 0, each sample after τ
  reads 1, so the received byte (0xFF shifted left by the number of early samples) shows which
  samples came before τ. τ runs from 1 to 9 bits in 1 µs steps, four times, back to back and
  with pauses. The PC-1500 echoes every block with SEROUT.
- **Error tests:** random data back to back, with pauses of 0–6 bits, and at baud rates from
  −6 % to +6 %.
- The times are measured with the clock of the UNO. Its ceramic resonator may be off by up to
  about 0.5 %, so the result is checked once more with a crystal-controlled device (for example
  a USB-UART cable at 19200 bps and the RX/TX test of the installer, `RUN 530`).

The calibration method was tested in the simulator as a whole (probe frames, echo, the
analysis code of the sketch): with a model of the PC-1500 whose `BII` is 0.5 cycles slower than
MAME and whose clock is 0.3 % fast, the defaults received without errors only from 0 % to +6 %
(sender baud rate); the report suggested RB = 67 and RQ = 16, and with them the range became
−3 % … +4 % back to back and −2 % … +3 % with pauses. With 3 extra cycles per port instruction
and a clock 0.4 % slow, the TX bit came out 2.3 % long; the suggestion TB = 64 brought it to
−0.6 %.
