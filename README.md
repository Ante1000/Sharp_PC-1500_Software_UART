# Software UART for Sharp PC-1500(A)

**SERIN and SEROUT routines** by Alex F4VTS

This software creates a serial port (UART) in the Sharp PC-1500(A) pocket computer without a
dedicated interface (such as the CE-158) and without any hardware modification. An optional,
simple modification lets you use the UART while the PC-1500(A) is docked in the CE-150
printer/cassette interface.

| Document | Contents |
|---|---|
| [Instruction.pdf](Instruction.pdf) | full description and user manual |
| [Instruction_Francais.pdf](Instruction_Francais.pdf) | full description and user manual, in French |
| [Modification_for_use_with_CE-150_EN.pdf](Modification_for_use_with_CE-150_EN.pdf) | optional modification for use with the CE-150 |
| [Modification_for_use_with_CE-150_Francais.pdf](Modification_for_use_with_CE-150_Francais.pdf) | optional modification for use with the CE-150, in French |
| [HELP_NEW_memory.md](HELP_NEW_memory.md) | `NEW` values and routine/buffer addresses for every RAM configuration |
| [CALIBRATION_19200.md](CALIBRATION_19200.md) | SERINOUT v7.1 for 19200 bps and its calibration with an Arduino UNO |

## Features

- A BASIC installer puts the machine-language routines **SEROUT** (send) and **SERIN** (receive)
  into RAM. Together they take only about 200 bytes, plus two 255-byte buffers (TX and RX).
- The routines work by bit-banging: the signals are generated and read bit by bit in software,
  without a dedicated hardware circuit.
- Baud rate: **1200, 2400, 4800, 9600 or 19200 bps**, selected in the installer v7.2
  (`1`=1200, `2`=2400, `3`=4800, `4`, `0` or ENTER = 9600, `5`=19200). At 1200–9600 it installs
  SEROUT/SERIN v6.2, at 19200 SEROUT/SERIN v7.1, which was calibrated on a real PC-1500 with an
  Arduino UNO, see [CALIBRATION_19200.md](CALIBRATION_19200.md). The installer v6.2 (on the
  tape recording) offers 1200–9600 bps.
- Format **8N1** (8 data bits, no parity, 1 stop bit), no RTS/CTS. An INVERSION option is
  available for devices with inverted signal levels.
- **TTL/CMOS signal levels (+3.3 V / +5 V). This is NOT RS-232:** connecting the PC-1500(A) to
  an RS-232 port (up to ±15 V) will destroy its electronics.
- Tested with USB-UART (FTDI) cables, Sharp, Casio and NEC pocket computers, sensors and modules,
  Arduino boards (5 V) and ESP32 boards (3.3 V).
- No port to open, select or close (unlike the CE-158): one `CALL` sends, one `CALL` receives. Operates in packet mode, not continous mode.
- Runs on a bare **PC-1500A**. A **PC-1500** needs a RAM module (e.g. CE-155), because the
  installer does not fit in its 2 KB.

## Repository contents

| File | Contents |
|---|---|
| `Basic/pc1500_uart_installer-v7.2.txt` | BASIC installer v7.2: 1200, 2400, 4800, 9600 or 19200 bps |
| `Basic/pc1500_uart_test-v7.2.txt` | TX/RX tester for every speed (run it after the installer v7.2) |
| `Basic/pc1500_uart_installer-v6.2.txt` | BASIC installer and TX/RX tester, version 6.2 (1200–9600 bps) |
| `Basic/pc1500_uart_link-v1.0.txt` | link program for two PC-1500s: demo, send text, receive (1200–9600 bps) |
| `Basic/pc1500_uart_quote-v1.0.txt` | "quote of the day" demo: asks an Arduino UNO, see [below](#demo-quote-of-the-day-with-an-arduino-uno) |
| `Basic/pc1500_uart19200_calibration-v1.1.txt` | PC-1500 program for the 19200 bps calibration with an Arduino UNO (installs SERINOUT v7.1 itself) |
| `arduino/UART_Calibration/` | Arduino UNO sketch that measures the 19200 bps timing and prints a report |
| `arduino/UART_Quote/` | Arduino UNO sketch for the quote demo: answers with a sentence from its word lists |
| `asm/` | LH5801 assembler sources and listings of SEROUT and SERIN v6.2 and v7.1 (19200 bps) |
| `wav/SERINOUT.wav` | the installer v6.2 as a tape recording, for `CLOAD` |
| `wav/link.wav` | the link program as a tape recording, for `CLOAD` |
| `img/` | connector drawing, connection diagrams and photos |
| `Archive/` | older versions (installer v7.1, 19200 bps only) |

## Connection

The UART uses the 60-pin connector on the left side of the PC-1500(A):

| Signal | Pin | Use |
|---|---|---|
| PC7 | 10 | TX: PC-1500 output |
| PB2 (CMT IN) | 27 | RX: PC-1500 input (default) |
| PB0 | 9 | RX: alternative input, needs the CE-150 modification |
| GND | 52–55 | ground, connect to the ground of the other device |

![60-pin connector of the PC-1500 with the UART pins](img/pc1500_60pin_connector.png)

- Connect the PC-1500(A) output to the input of the other device and vice versa, through
  resistors of 1–10 kΩ. A 1.27 mm dual-row pin header (2 × 30 pins) fits the connector.
- The PC-1500 accepts about 2.4 V or more as a high level, so a 3.3 V device such as the
  ESP32 can drive it directly. In the PC-1500 → 3.3 V device direction use a resistor divider
  (47 kΩ in series, 100 kΩ to GND: about 3.3 V), because the ESP32 inputs tolerate at most
  about 3.6 V.
- PB2 is the cassette input of the CE-150, so without the modification the PC-1500(A) must be
  removed from the CE-150 while the UART is in use. With the modification, PB0 is used instead
  and the UART lines come out on a connector added to the CE-150.

![Connection 1: PC-1500A and a USB-UART (FTDI) cable](img/connection1_pc1500_ftdi.png)

![Connection 2: two PC-1500A computers](img/connection2_pc1500_pc1500%20%281%29.png)

![Connection 3: PC-1500A and an ESP32 board](img/connection3_pc1500_esp32.png)

## How to use

1. Switch off the PC-1500A, insert it into the CE-150 and connect the audio output of a
   computer or player to the EAR jack of the CE-150.
2. Switch on the PC-1500A, select PRO mode and type `NEW0` [ENTER], then `NEW&4400` [ENTER]
   (another value with a RAM module, see [HELP_NEW_memory.md](HELP_NEW_memory.md)), then
   `CLOAD` [ENTER].
3. Play `wav/SERINOUT.wav` and wait until the installer v6.2 has loaded (or type in the
   installer v7.2, which also offers 19200 bps).
4. Without the modification, switch off the PC-1500A and remove it from the CE-150.
5. Connect the other UART device as shown above (or to the connector added to the CE-150 if
   the units are modified).
6. Select RUN mode and type `RUN` [ENTER]. The installer asks for the baud rate, the RX port
   (default PB2; press `0` for PB0 with the modification) and the inversion (default no).
   ENTER alone selects the default.
   - Installer v7.2: it shows `1=1200 2=2400 3=4800 BPS`, then asks `4/ENTER=9600 5=19200?`:
     `1`, `2`, `3`, `4` (or `0`, or ENTER = 9600) or `5`. At 19200 bps it also asks
     `TIMING (ENTER=STD,1=SET)`: ENTER uses the calibrated constants.
   - Installer v6.2: `1`=1200, `2`=2400, `4`=4800 (ENTER), `9`=9600.
7. The installer v6.2 then offers a TX test and an RX test. Press `0` to skip them; the RX test
   can be started later with `RUN 530`. The installer v7.2 has no test (it would not fit into
   the memory of a PC-1500A together with the code of both versions): type `NEW` in PRO mode,
   then type in or load `Basic/pc1500_uart_test-v7.2.txt` and `RUN` it.
8. When you no longer need the installer, type `NEW` [ENTER] in PRO mode. The routines stay in
   memory and the rest of the memory is free for your BASIC program.

To change the baud rate, port or inversion, run the installer again.

## Commands

Addresses for a PC-1500A without a RAM module. The installer shows the addresses for your
configuration; see also [HELP_NEW_memory.md](HELP_NEW_memory.md).

| Command | Action |
|---|---|
| `CALL&40C5,m` | **send** `m` bytes (1–255) from the TX buffer; `CALL&40C5` sends only the first byte |
| `CALL&4130,m` | **receive** at most `m` bytes into the RX buffer; the received length is returned in `m` |
| `CALL&4130` | **receive** a packet of any length, up to 255 bytes |

| Address | Contents |
|---|---|
| &4300 | TX buffer: put the data to send here with `POKE` |
| &4200 | RX buffer: read the received data with `PEEK` |
| &41FF | length of the last received packet |

SERIN waits up to about 30 s for the first character and returns to BASIC when no further
character arrives within about 0.5 s.

## Demo: quote of the day with an Arduino UNO

The PC-1500 asks, in a loop and at random, `What's for today?`, `tomorrow?` or `yesterday?`,
and the Arduino answers with a short sentence made from its word lists, at most 26 characters,
one line of the display: `Luck finds a golden key.`, `A cat will fix the bus.`,
`The moon stole hot tea.` The PC-1500 shows each answer for about 8 seconds, then asks again.

| PC-1500 (60-pin) | In between | Arduino UNO |
|---|---|---|
| PC7 (TX), pin 10 | Schottky diode, cathode (stripe) toward PC7, or 1 kΩ | A5 |
| PB2 (RX), pin 27 | 470 Ω | A4 |
| GND, pins 52–55 | – | GND |

(The same wiring as for the 19200 bps calibration; a 4.7 kΩ resistor from A5 to 5 V is
recommended with the diode. The LCD Keypad Shield is optional.)

1. Upload `arduino/UART_Quote/UART_Quote.ino` (19200 bps; another speed: change `BAUD`).
2. On the PC-1500 run the installer v7.2: speed `5`, RX port PB2, inversion no. Then `NEW`
   in PRO mode and type in `Basic/pc1500_uart_quote-v1.0.txt`.
3. `RUN`. The program runs until you press BREAK (ON). Without an answer it waits about
   30 s, shows `NO ANSWER (ARDUINO?)` and asks again.

The word lists (34 subjects, 38 verbs, 40 objects) are at the top of the sketch and can be
changed freely: a sentence longer than one line is never sent. The serial monitor (115200 bps)
shows every question and answer.

## Author

Comments, bug reports, ideas and inquiries: alex.f4vts@gmail.com (I may not be able to respond
immediately).

I created the SERIN/SEROUT software UART myself, with help from Claude Code. It is based on the
idea of the SAVER/LOADER program published in the Japanese magazine I/O (1983).
