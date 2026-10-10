# SERINOUT v7.1: 19200 bps, and its calibration with an Arduino UNO

> **Status: calibrated on a real PC-1500.** SERINOUT v7.0 was calibrated once on a real
> PC-1500; the real PC-1500 was slower than the cycle table used for v7.0, by more than the
> timing ranges of v7.0 could correct, so SEROUT and SERIN were changed to **v7.1**. The second
> calibration (v7.1 on the same PC-1500) gave the defaults **TB = 61, RB = 65, RQ = 16**, and the
> third one confirmed them: TX bit +0.13 %, no errors in random data, no errors when the sender
> is 4 % slower to 3 % faster (characters back to back) or 3 % slower to 2 % faster (with
> pauses). The times were measured with the ceramic resonator of the Arduino UNO (up to about
> 0.5 % off); a check with a crystal-controlled device (a USB-UART cable at 19200 bps) is
> recommended.

SERINOUT v7.1 is used exactly like v6.2: the same `CALL`s, buffers and addresses
([README](README.md#commands)), but at **19200 bps only** (8N1, normal or inverted polarity, RX on
PB2 or PB0). It moves up to about 1.9 KB/s, twice the 9600 bps of v6.2.

## Files

| File | Contents |
|---|---|
| `Basic/pc1500_uart19200_calibration-v1.1.txt` | PC-1500 calibration program: installs SERINOUT v7.1 itself, talks to the Arduino, can set new timing constants |
| `arduino/UART_Calibration/` | Arduino UNO sketch v4 (one file, `UART_Calibration.ino`) that measures the timing and prints the report |
| `Basic/pc1500_uart_installer-v7.1_19200.txt` | BASIC installer of SERINOUT v7.1 (19200 bps), with the TX/RX test of v6.2: for use after the calibration (other polarity, PB0) |
| `asm/serout_v7.1.asm`, `asm/serin_v7.1.asm` | LH5801 sources (and `.lst` listings) of SEROUT and SERIN v7.1 |

## Why 19200 needs new code

At 19200 bps a bit lasts 52.08 µs, that is 67.7 cycles of the LH5801 at 1.3 MHz. The bit loops of
v6.x need at least 79 cycles (they contain a delay loop `LDI`/`LOP` and a bit counter), so they
cannot go faster than about 16 000 bps. v7.x has new loops without a delay loop:

- **SEROUT:** both branches (bit 0 / bit 1) take 33 cycles, then a 3-byte timing pad (8 or 10–15
  cycles), `SHR` and `LOP UL` (the bit counter): one bit = **TB = 50 + pad = 58 or 60…65
  cycles** (MAME's cycle table). The start bit and the last data bit have the same length; the
  stop bit is about 1 bit. The character counter is `XL` (the TX buffer starts a page) with a
  `CPI XL,N` whose N the routine writes into its own code at the start.
- **SERIN:** the bit counter is a sentinel bit in A (after 8 `ROR` it leaves into C), the two
  paths of a bit take the same time, and a 4-byte pad sets the length: **RB = 51 + pad =
  63…71 cycles**. A fixed 8-cycle jump and another 4-byte pad, **RQ = 8 + pad = 16 or 20…28
  cycles**, move all 8 samples.
- **Finding the start bit** is the hard part of a software UART: the line is polled, so the
  edge is only known to within one poll. The loop with the 30 s / 0.5 s time-out polls every 33
  cycles (half a bit at 19200). After each character SERIN therefore polls 7 times without a
  counter, every 22 cycles (a third of a bit): characters that are sent back to back, as most
  devices do, are found there, more precisely. The middle of the start bit is checked (noise
  filter), as in v6.x.

38400 bps is not possible: a bit would have only 34 cycles.

## The first calibration (SERINOUT v7.0) and what changed in v7.1

v7.0 was written with MAME's cycle table plus 1.9 extra cycles per `ORI`/`ANI` on a port (measured
with the I2C master). The first report from a real PC-1500 (sketch v3) showed:

| | v7.0 setting | measured | in cycles |
|---|---|---|---|
| TX bit | TB = 67 | 56.37 µs (+8.2 %) | 73.3 real cycles: **6.3 more** than the table |
| RX bit (time between samples) | RB = 70 | 56.44 µs (+8.4 %) | 73.4 real cycles: **3.4 more** than the table |
| first RX sample | RQ = 15 | 78.1 µs after the start edge | (ideal 78.1 µs) |

The TX would have needed TB ≈ 61.4, but v7.0 could not go below 63; the RX needed RB ≈ 64,
which moves the first sample about 6 cycles earlier, and a later RQ than v7.0 had. With RB = 70
the samples drifted late, so characters sent back to back were not received (the "fast" probes
were lost) and random data had errors.

v7.1 therefore has:

- **SEROUT v7.1:** a shorter bit loop (bit counter `LOP UL` instead of a sentinel and
  `BII A,&FF`, 7 cycles less), TB = 58 or 60…65 (first default TB = 62).
- **SERIN v7.1:** a fixed 8-cycle jump before the RQ pad, so RQ = 16 or 20…28 (first defaults
  RB = 64, RQ = 22).

The first defaults came from models of the PC-1500 fitted to the first report (extra cycles for
the port writes, `SHR`, `ROR`, `BII`, `NOP` in different proportions). The second calibration
showed where the models were wrong (next section).

## The second calibration (SERINOUT v7.1)

Sketch v4 with calibration program v1.1, TB = 62, RB = 64, RQ = 22:

| | measured | |
|---|---|---|
| TX bit | 52.92 µs (+1.6 %) | 68.8 real cycles for TB = 62: TB = 61 gives about 52.15 µs (+0.1 %) |
| TX 2-bit intervals | start+b0 104.06, b1+b2 / b3+b4 / b5+b6 106.43, b7+stop 105.74 µs | stop bit 1.00 bit |
| RX bit (time between samples) | 51.20 µs back to back, 51.33 µs after pauses (−1.7 / −1.4 %) | 66.6 real cycles for RB = 64: RB = 65 gives about 52.0 µs |
| first RX sample (fit) | 82.2 / 81.3 µs (ideal 78.1) | 4 µs late |
| samples vs middle of bit | −0.18 … +0.22 bit back to back, −0.23 … +0.24 bit after pauses | limit ±0.5 |
| random data | 0 errors in 510 bytes back to back and 510 bytes with pauses | |
| baud margin (sender) | 0 errors from −3 % to +3 % back to back, −2 % to +3 % with pauses (1 error at −3 %) | |
| suggested | TB = 61, RB = 65, RQ = 16 | wanted 60.9, 65.1, 17.1 |

The 2-bit intervals of the TX burst show which instruction is slow: start bit + bit 0 contain an
`LDI UL` and a `NOP` where two data bits contain a second taken `LOP`; they take the same 124
cycles in MAME's table, but start + bit 0 was 3.1 cycles shorter on the PC-1500. So a taken
`LOP` takes about 3 cycles more than MAME's 11 (if `LDI` and `NOP` take no extra time). The start bit is thus 2.4 µs (0.05 bit) short;
receivers do not notice that, so the code was not changed.

RQ = 17 is not possible with the 4-byte pad (only 16 or 20…28); RQ = 16 puts the samples 1 cycle
earlier than wanted and, as computed by the sketch, within −0.26 … +0.22 bit of the middle of the
bits. The new defaults are therefore **TB = 61, RB = 65, RQ = 16** (installer v7.1 and line 60 of
the calibration program v1.1).

## The third calibration: the defaults confirmed

Sketch v4 with calibration program v1.1, TB = 61, RB = 65, RQ = 16:

| | measured | |
|---|---|---|
| TX bit | 52.15 µs (+0.13 %) | as predicted; needed change of TB −0.09 cycles |
| TX 2-bit intervals | start+b0 102.51, b1+b2 / b3+b4 / b5+b6 104.90, b7+stop 104.99 µs | as predicted (102.52 / 104.89); stop bit 1.01 bit |
| RX bit (time between samples) | 51.72 µs back to back, 52.01 µs after pauses (−0.7 / −0.1 %) | |
| first RX sample (fit) | 79.6 / 77.2 µs (ideal 78.1) | |
| samples vs middle of bit | −0.17 … +0.16 bit back to back, −0.25 … +0.22 bit after pauses | limit ±0.5 |
| random data | 0 errors in 510 bytes back to back and 510 bytes with pauses | |
| baud margin (sender) | 0 errors from −4 % to +3 % back to back, −3 % to +2 % with pauses (7 errors at +3 %) | |
| suggested | TB = 61, RB = 65, RQ = 16 (= installed) | wanted 60.9, 65.3, 17.1 |

The calibration has converged. The receiver tolerates senders from −3 % to +2 % in all cases,
more than enough for devices with a crystal or a calibrated oscillator (USB-UART adapters, other
microcontrollers, a second PC-1500 with SERINOUT v7.1).

The simulator reproduces these margins when a taken `LOP` takes 3.1 cycles more than in MAME's
table (models A and B of the first report plus this `LOP`): back to back the reception breaks
down at +4 % because the next start bit then comes before the first fast poll and the error
adds up from character to character. Starting the fast polls 12 cycles earlier (the two `LDI`
of the time-out moved behind them) brought +4 % back to back from 219 to 23 errors per 255 in
the simulator and did not change the margins with pauses, so the code was left as it is.

## Timing constants

| Constant | Meaning | Values | Default |
|---|---|---|---|
| TB | length of a TX bit in MAME cycles | 58, 60…65 | 61 |
| RB | length of an RX bit (time between two samples) in MAME cycles | 63…71 | 65 |
| RQ | position of the RX samples (larger = later), cycles | 16, 20…28 | 16 |

The calibration program uses the defaults the first time and keeps the values of an installed
v7.1 after that. Answer `1` at `NEW TIMING (1=YES)?` to type other values. The values are stored
at RAM+&1FC…&1FE. The installer uses the defaults when you press ENTER at
`TIMING (ENTER=STD,1=SET)`; answer `1` to type the calibrated values.

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
sketch also switches on the internal pull-up. The diode can stay: the sketch measures the
times on the falling edges, which the diode makes sharp. The PC-1500 must be out of the
CE-150.

### Steps

1. On the PC-1500, in PRO mode: `NEW0`, then `NEW &4400` (other RAM configurations:
   [HELP_NEW_memory.md](HELP_NEW_memory.md)).
2. Type in `Basic/pc1500_uart19200_calibration-v1.1.txt`. The installer is not needed: the
   calibration program installs SERINOUT v7.1 (normal polarity, RX on PB2) every time it runs.
3. Upload `arduino/UART_Calibration/UART_Calibration.ino` (sketch v4) to the UNO and open the
   serial monitor at **115200 bps**.
4. `RUN` the calibration program on the PC-1500. It shows TB, RB and RQ, asks
   `NEW TIMING (1=YES)?` (press ENTER the first time), shows `INSTALLING v7.1...` (a few seconds)
   and asks `ARDUINO READY? ENTER`.
5. The PC-1500 sends a header and 64 × 'U', then answers the Arduino block by block
   (`BLOCKS: 10`, `20`, …, less than a minute). About 30 s after the last block it shows
   `END: 44 BLOCKS`. The Arduino prints the report as soon as it has finished.
6. Copy the report from the serial monitor, from the line
   `=== PC-1500 SERINOUT 19200 calibration report ===` to the line `=== end of report ...`,
   and send it.
7. To try other constants (for example the `SUGGESTED` ones), `RUN` the calibration program
   again and answer `1` at `NEW TIMING`; it installs SERINOUT v7.1 with them and measures again.

After the calibration SERINOUT v7.1 stays installed with the last constants (normal polarity,
PB2) and can be used at once. For inverted polarity or PB0, run the installer v7.1 and type the
calibrated constants at `TIMING`.

If the program is stopped with BREAK during the echo loop, enter `RUN 210`: during the loop
SEROUT sends from the RX buffer, `RUN 210` switches it back to the TX buffer.

### The report

| Line | Meaning |
|---|---|
| `TX 64 x 'U': bit … us` | length of a PC-1500 TX bit (falling-to-falling edges, 2 bits each); ideal 52.08 µs; also in LH5801 cycles, with the change of TB that is needed |
| `2-bit intervals` | the mean of each of the 5 falling-to-falling intervals of a 'U' frame (start + bit 0, bits 1+2, 3+4, 5+6, bit 7 + stop bit); ideal 104.17 µs |
| `stop bit` | length of the stop bit in bits (about 1) |
| `RX probe fast` / `RX probe slow` | when SERIN reads each data bit, in µs after the start edge sent by the Arduino: earliest, mean and latest sample; ideal = middle of the bit. "fast" = characters back to back (fast polls), "slow" = pauses of 4 bits and a little more (polls with the time-out) |
| `needed change of RB` | change of the RX bit in cycles, not rounded |
| `fit` | first sample and the time between samples (the RX bit) from the mean values |
| `worst sample vs middle of bit` | the earliest and latest sample of all bits, in bits; the limit is ±0.5 |
| `Random data …` | errors in 1020 random bytes, back to back and with random pauses |
| `Baud margin …` | errors per 255 bytes when the Arduino sends 6 % slower … 6 % faster: the wider the range with 0, the better |
| `SUGGESTED: TB= RB= RQ=` | constants computed from the measurement, the nearest allowed values |
| `wanted, not rounded` | the same constants before rounding to an allowed value |
| `NOTE: … outside its range` | a wanted value does not fit into the range of v7.1: the code has to change (send the report) |

### How the measurement works

- **TX:** the comparator of the UNO (1.1 V against A5) passes every edge of PC7 to the input
  capture of Timer1, which stores the time with 62.5 ns resolution. Inside a 'U' (0x55) frame
  the falling edges are exactly 2 bits apart. Everything else the PC-1500 sends (the header, the
  "R" before each block, the echoes) is received with this measured bit length, so the
  calibration also works when the TX of the PC-1500 is several per cent off. The header comes
  before the 'U' burst, so the sketch records the times of all its edges (input capture,
  switched between falling and rising edges) and decodes the header after the burst; if the
  header cannot be read, the measurement goes on with TB = 62, RB = 64, RQ = 22 assumed. The last byte of
  the header tells the sketch which SERINOUT runs (1 = v7.0 with program v1.0, 2 = v7.1).
- **RX:** the Arduino sends "probe" frames: a start bit, then the line stays at space until a
  time τ and goes back to mark. Each sample of SERIN before τ reads 0, each sample after τ
  reads 1, so the received byte (0xFF shifted left by the number of early samples) shows which
  samples came before τ. τ runs in 1 µs steps, four times, from 1 to 9 bits with frames back
  to back and from 1 to 9.5 bits with pauses (for samples that come late). The pauses are 4 bits
  plus 0…64 µs, different for each probe, so that the start edges fall on all phases of the
  polling loop of SERIN (it polls every 25 µs). The PC-1500 echoes every block with SEROUT.
- **Error tests:** random data back to back, with pauses of 0–6 bits, and at baud rates from
  −6 % to +6 %.
- The times are measured with the clock of the UNO. Its ceramic resonator may be off by up to
  about 0.5 %, so the result is checked once more with a crystal-controlled device (for example
  a USB-UART cable at 19200 bps and the RX/TX test of the installer, `RUN 530`).

The calibration was tested in the simulator as a whole (calibration program in a BASIC
interpreter, SEROUT/SERIN v7.1 in the LH5801 simulator, probe frames, echo, the analysis code
of the sketch) with the three PC-1500 models fitted to the first report. Starting from the
first defaults (TB = 62, RB = 64, RQ = 22), the suggestion was TB = 61, RB = 64, RQ = 20 for two of the models and TB = 62,
RB = 66, RQ = 16 for the third; a second calibration with the suggested constants gave the same
suggestion for the first two and RB = 65, RQ = 20 for the third. With the suggested constants
the error-free range was about −3 % … +3 % (sender baud rate) back to back and with pauses.
The same simulation of v7.0 with RB = 70 and RQ = 15 lost the fast probes, as the real PC-1500
did. The real PC-1500 then suggested TB = 61, RB = 65, RQ = 16 (second calibration above): close
to the third model, but none of the models has the slow `LOP` that its TX intervals show.
