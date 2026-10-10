# Setting `NEW0` and NEW&nnnn (see in the table below) before loading the SEROUT/SERIN installer (v6.x)

The installer (`pc1500_uart_installer-v6.2.txt`) uses the **first 1 KB of RAM**,
counted from the start of RAM. Where RAM starts depends on the model and on the memory module.
Before loading the installer, enter `NEW` in PRO mode so that BASIC starts
**after this kilobyte**.

## The rule (works for every configuration)

1. Find the start of RAM (in RUN mode):
   ```
   PRINT PEEK &7863
   ```
   The result is the high byte of the RAM start address, e.g. `64` = &40, so RAM starts at &4000.
2. Compute **`NEW` = result × 256 + &400** and enter it in PRO mode.

| `PEEK &7863` | RAM start | Enter in PRO mode |
|---|---|---|
| 64 (&40) | &4000 | `NEW &4400` |
| 56 (&38) | &3800 | `NEW &3C00` |
| 32 (&20) | &2000 | `NEW &2400` |
| 0 (&00) | &0000 | `NEW &400` |

What the kilobyte contains (A0 = RAM start):

| Address | Contents |
|---|---|
| A0 … A0+&C4 | system area (RESERVE); a plain `NEW 0` starts BASIC right after it |
| A0+&C5 | SEROUT (TX call) |
| A0+&130 | SERIN (RX call) |
| A0+&1FF | RC: number of characters received |
| A0+&200 … A0+&2FE | RX buffer (255 characters) |
| A0+&300 … A0+&3FE | TX buffer (255 characters) |
| from A0+&400 | BASIC program |

The installer computes every address from `PEEK &7863` itself and checks that BASIC starts
at A0+&400 or later. **Note:** the v6.x messages always suggest "NEW &4400" (the value for
a PC-1500A without a module). With a module, use the table below.

## Configuration table

Confidence: ✅ confirmed (measurement or Sharp documentation), 🔶 inferred from the design of a
similar module (check `PEEK &7863`), ❓ uncertain.

| Configuration | Module | RAM start | RAM end | **`NEW`** | TX `CALL` (SO) | RX `CALL` (SI) | Free for BASIC after `NEW` | Confidence / notes |
|---|---|---|---|---|---|---|---|---|
| **PC-1500** (2 KB) | – | &4000 | &47FF | **`NEW &4400`** | &40C5 | &4130 | approx. 1 KB | ✅ The installer (v6.2 approx. 4.1 KB, v7.2 approx. 4.6 KB) does not fit, see "PC-1500 with 2 KB" below |
| **PC-1500A** (6 KB) | – | &4000 | &57FF | **`NEW &4400`** | &40C5 | &4130 | approx. 5 KB | ✅ configuration tested on real hardware |
| PC-1500 + **CE-151** | 4 KB RAM | &3800 | &4FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 5 KB | 🔶 same design as CE-155, only smaller |
| PC-1500A + **CE-151** | 4 KB RAM | &3800 | &5FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 9 KB | 🔶 |
| PC-1500 + **CE-155** | 8 KB RAM | &3800 | &5FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 9 KB | ✅ (`MEM` = 10042 without the installer) |
| PC-1500A + **CE-155** | 8 KB RAM | &3800 | &6FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 13 KB | ✅ (`MEM` = 14138 without the installer) |
| PC-1500 + **CE-157** | 4 KB RAM + Katakana ROM | &3800 | &4FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 5 KB | 🔶 Japanese module; its RAM is probably mapped like CE-151 |
| PC-1500A + **CE-157** | 4 KB RAM + Katakana ROM | &3800 | &5FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 9 KB | 🔶 |
| PC-1500 + **CE-159** | 8 KB battery-backed RAM | &3800 | &5FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 9 KB | ✅ electrically the same as CE-155 (service manual: "10K bytes including reserve area"). Turn write protection off while installing. One source gives a start of &2000: if `PEEK &7863` = 32, use `NEW &2400`. |
| PC-1500A + **CE-159** | 8 KB battery-backed RAM | &3800 | &6FFF | **`NEW &3C00`** | &38C5 | &3930 | approx. 13 KB | ✅ as above |
| PC-1500(A) + **CE-160** | approx. 7.6–7.8 KB, read-only | ? | ? | **do not install into CE-160** | – | – | – | ❓ Module with ready-made programs, written with the CE-165 programmer. The code cannot be written into it; the block must be in writable RAM. Check `PEEK &7863`: if it points inside the CE-160, the installation will not work. |
| PC-1500 + **CE-161** | 16 KB battery-backed RAM | &0000 | &47FF | **`NEW &400`** | &00C5 | &0130 | approx. 17 KB | ✅ (`PEEK &7863` = 0 verified on hardware). Note: `NEW &4400` from the installer message would waste the whole module. Write protection off. |
| PC-1500A + **CE-161** | 16 KB battery-backed RAM | &0000 | &57FF | **`NEW &400`** | &00C5 | &0130 | approx. 21 KB | ✅ as above |
| PC-1500 + **CE-163** | 2 × 16 KB RAM (banks) | &0000 | &47FF | **`NEW &400` in every bank you use** | &00C5 | &0130 | approx. 17 KB per bank | 🔶 See "Bank-switched modules" below. The bank is switched by a write to &5800/&5801. |
| PC-1500A + **CE-163** | 2 × 16 KB RAM (banks) | &0000 | &57FF | **`NEW &400` in every bank you use** | &00C5 | &0130 | approx. 21 KB per bank | 🔶 as above; the bank is switched by a write to &6800/&6801 |

"Free for BASIC" is approximate (end of RAM minus program start). After `NEW`, the `MEM` command
shows the exact value; variables take a little of this space.

## Installation order

1. PRO mode: `NEW` from the table (e.g. `NEW &3C00`).
2. `CLOAD` the installer (or type it in) and `RUN`.
3. Answer the questions about speed, port and inversion.
4. After installation the test program (`RUN 530`) finds the code at the right addresses by itself.

Do not add or remove a memory module after installation. The RAM start changes, and the
code and the BASIC program are lost or end up at different addresses.

## Addresses in your own programs

Do not hard-code `CALL &40C5`. Compute the addresses the way the test program does, and your
program will work in every configuration:
```
A0 = PEEK &7863 * 256 : SO = A0 + &C5 : SI = A0 + &130
RC = A0 + &1FF : RX = A0 + &200 : TX = A0 + &300
```

## PC-1500 with 2 KB

On a PC-1500 without a module, only approx. 1 KB is left for BASIC after `NEW &4400`, while the
tokenized v6.1 installer takes approx. 4.1 KB (approx. 2.8 KB installation part and approx.
1.3 KB test program), so it does not fit. The simplest way:

1. Install on a PC-1500A (or on any configuration **with the same RAM start &4000**, i.e.
   without a memory module).
2. Save only the code and buffers to tape: `CSAVE M "UART"; &40C5, &43FF`.
3. On the PC-1500: `NEW &4400` in PRO mode, then `CLOAD M "UART"`.

The code works only at the addresses where it was installed, because it contains the page
numbers of its buffers. So move a `CSAVE M` image only between configurations with the same
RAM start. Speed, port and inversion stay as selected during installation. A CE-150
(cassette interface) is required.

## Bank-switched modules (CE-163 and modern 64–128 KB modules)

- **Bank window.** The &0000–&3FFF window changes its contents on every bank switch. The
  SEROUT/SERIN block and buffers (&0000–&03FF) exist only in the bank where they were
  installed. After switching to another bank, `CALL &C5` would jump into random bytes. Install
  the code in every bank where you want to use it (switch the bank, `NEW &400`, `CLOAD`, `RUN`),
  or use only one bank.
- **Module firmware.** Modules with a bank-management program at the start of each bank need
  their own `NEW` offset (e.g. TRAMsoft: `NEW &100`). That program sits right after the system
  area, exactly where the v6.x installer puts SEROUT (A0+&C5). **v6.x would overwrite it.**
  Such a module needs an installer version with a shifted block (not available yet).

## Sources

- Memory map, `MEM` values, the rule `NEW 0` = RAM start + &C5 and the bank-switching
  mechanism: PC-1500 memory write-ups based on the Sharp Technical Reference Manual, service
  manuals and measurements (`PEEK &7863` = &40 on a PC-1500A, &00 with a 16 KB module).
- CE-159: Sharp service manual ("10K bytes including the reserve area").
- CE-157 and CE-160: module descriptions on
  [pc-1500.info](http://www.pc-1500.info/category/cat_family/cat_3modules/) and in
  [Wikipedia](https://en.wikipedia.org/wiki/Sharp_PC-1500). The CE-151 and CE-157 addresses are
  inferred from the CE-155 design, not measured.
- Installer size: computed by tokenizing the v6.1 lines (approximation, ±5%).

When in doubt, `PRINT PEEK &7863` on your own computer always decides.
