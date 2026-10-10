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
| [CALIBRATION_19200.md](CALIBRATION_19200.md) | **test version** SERINOUT v7.0 for 19200 bps and its calibration with an Arduino UNO |

## Features

- A BASIC installer puts the machine-language routines **SEROUT** (send) and **SERIN** (receive)
  into RAM. Together they take only about 200 bytes, plus two 255-byte buffers (TX and RX).
- The routines work by bit-banging: the signals are generated and read bit by bit in software,
  without a dedicated hardware circuit.
- Baud rate: **1200, 2400, 4800 or 9600 bps**, selected in the installer (default 4800 bps).
  **19200 bps:** test version SERINOUT v7.0 with its own installer, to be calibrated on a real
  PC-1500 with an Arduino UNO, see [CALIBRATION_19200.md](CALIBRATION_19200.md).
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
| `Basic/pc1500_uart_installer-v6.2.txt` | BASIC installer and TX/RX tester, version 6.2 |
| `Basic/pc1500_uart_link-v1.0.txt` | link program for two PC-1500s: demo, send text, receive |
| `Basic/pc1500_uart_installer-v7.0_19200.txt` | BASIC installer of the 19200 bps test version SERINOUT v7.0, with the TX/RX tester |
| `Basic/pc1500_uart19200_calibration-v1.0.txt` | PC-1500 program for the 19200 bps calibration with an Arduino UNO |
| `arduino/UART_Calibration/` | Arduino UNO sketch that measures the 19200 bps timing and prints a report |
| `asm/` | LH5801 assembler sources and listings of SEROUT and SERIN v6.2 and v7.0 (19200 bps) |
| `wav/SERINOUT.wav` | the installer as a tape recording, for `CLOAD` |
| `wav/link.wav` | the link program as a tape recording, for `CLOAD` |
| `img/` | connector drawing, connection diagrams and photos |

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
3. Play `wav/SERINOUT.wav` and wait until the installer has loaded.
4. Without the modification, switch off the PC-1500A and remove it from the CE-150.
5. Connect the other UART device as shown above (or to the connector added to the CE-150 if
   the units are modified).
6. Select RUN mode and type `RUN` [ENTER]. The installer asks for the baud rate (default
   4800), the RX port (default PB2; press `0` for PB0 with the modification) and the inversion
   (default no). ENTER alone selects the default.
7. The installer then offers a TX test and an RX test. Press `0` to skip them; the RX test can
   be started later with `RUN 530`.
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

## Author

Comments, bug reports, ideas and inquiries: alex.f4vts@gmail.com (I may not be able to respond
immediately).

I created the SERIN/SEROUT software UART myself, with help from Claude Code. It is based on the
idea of the SAVER/LOADER program published in the Japanese magazine I/O (1983).
