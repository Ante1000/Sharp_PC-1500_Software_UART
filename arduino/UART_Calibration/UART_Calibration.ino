/*
  UART_Calibration.ino - calibration of SERIN/SEROUT v7.0 (19200 bps) on a
  Sharp PC-1500(A) with an Arduino UNO. The LCD Keypad Shield is optional
  (it only shows the progress).

  Wiring (the same as the I2C tester; everything at 5 V):
    PC-1500 PC7 (TX)  pin 10    ---|<|---  A5   Schottky diode, cathode (stripe) to PC7
                                           (or a 1 kOhm resistor instead of the diode)
    PC-1500 PB2 (RX)  pin 27    --[470]--  A4
    PC-1500 GND       pin 52-55 --------  GND
    optional: 4.7 kOhm from A5 to 5 V (faster rising edges with the diode)
  The PC-1500 must be out of the CE-150. Install SERINOUT v7.0 with normal
  polarity (INVERSION = NO) and RX port PB2.

  Use:
    1. Upload this sketch, open the serial monitor at 115200 bps.
    2. On the PC-1500 run pc1500_uart19200_calibration-v1.0.txt.
    3. Wait until the report is printed (less than a minute), copy it from the
       serial monitor and send it. Run the PC-1500 program again after a
       new installation to test new constants.

  What it measures:
    - TX: the PC-1500 sends 64 x 'U' (0x55). The analog comparator (bandgap
      1.1 V against A5) gives every falling edge to the input capture of
      Timer1 (62.5 ns resolution); falling-to-falling intervals inside a
      frame are 2 bits, across frames bit 7 + the stop bit. Everything the
      PC-1500 sends is then received with this measured bit length, so the
      calibration also works when its TX is several per cent off. The
      header ("CAL", TB, RB, RQ) comes before the 'U' burst: the times of
      its edges are recorded (input capture) and decoded after the burst.
    - RX sample points: the Arduino sends probe frames: start bit, then the
      line stays at space until TAU and goes back to mark. The byte that the
      PC-1500 receives (and echoes) shows which of its 8 samples came before
      TAU. TAU runs in 1 us steps, 4 times, over 1..9 bits with frames back
      to back ("fast": the fast polls of SERIN find the start bit) and over
      1..9.5 bits with 4-bit pauses ("slow": the polls with the time-out).
    - Random data back to back and with random pauses, and the error count
      at baud rates from -6 % to +6 %.
  The PC-1500 echoes every block with SEROUT, so its TX is checked too.
  Times are measured with the clock of the UNO (its ceramic resonator may
  be off by up to about 0.5 %).
*/
#include <LiquidCrystal.h>
#include <math.h>

enum Kind : uint8_t { PROBE, DATA };   // frames of a block: probes or data

// ==== analysis begin: constants and statistics (no hardware access; the
// ==== same code is compiled on a PC for tests)
#define TICK_US 0.0625f                  // Timer1 of the UNO: 16 MHz
#define BIT_US (1000000.0f / 19200.0f)   // 52.083 us
#define CYCLE_US (1.0f / 1.3f)           // one LH5801 cycle at 1.3 MHz: 0.769 us
#define BIT_Q8 213333UL                  // one bit at 19200 bps: 833.333 ticks * 256

// Probe frames: the start bit begins at t = 0 and the line stays at space
// until TAU, then goes back to mark until the end of the frame. A sample of
// the PC-1500 taken before TAU reads 0, one taken after TAU reads 1, so the
// byte received is 0xFF << j: j samples came before TAU. TAU runs from 1.0
// bit in steps of 1 us, to 9.0 bits (frames back to back) or 9.5 bits
// (frames with pauses, for a PC-1500 whose samples come late; later than
// 9.5 bits the low line could look like a new start bit to SERIN).
#define TAU0 833        // ticks (1.0 bit)
#define TAU_STEP 16     // ticks (1 us)
#define NTAU_FAST 417   // 1.0 .. 9.0 bits
#define NTAU_SLOW 443   // 1.0 .. 9.5 bits

#define TAU_US(idx) ((TAU0 + (float)(idx) * TAU_STEP) * TICK_US)   // TAU of step idx (us)
#define IDEAL_US(k) ((1.5f + (k)) * BIT_US)                        // middle of data bit k (us)

struct ProbeStats {
  uint16_t ntau;        // number of TAU steps
  uint16_t frames;      // frames analysed
  uint16_t bad;         // received bytes that are not of the form 0xFF << j
  uint16_t lostBlocks;  // blocks whose echo had the wrong length
  uint16_t ones[8];     // frames in which bit k read 1 (its sample came after TAU)
  uint16_t lo[8];       // smallest TAU index with bit k = 0 (earliest sample)
  uint16_t hi[8];       // largest TAU index with bit k = 1 (latest sample)

  void reset(uint16_t n) {
    ntau = n;
    frames = bad = lostBlocks = 0;
    for (uint8_t k = 0; k < 8; k++) {
      ones[k] = 0;
      lo[k] = n;
      hi[k] = 0xFFFF;
    }
  }

  void add(uint16_t idx, uint8_t b) {
    frames++;
    uint8_t j = 0;
    while (j < 8 && !((b >> j) & 1)) j++;
    if ((uint8_t)(0xFF << j) != b) bad++;
    for (uint8_t k = 0; k < 8; k++) {
      if ((b >> k) & 1) {
        ones[k]++;
        if (hi[k] == 0xFFFF || idx > hi[k]) hi[k] = idx;
      } else if (idx < lo[k]) {
        lo[k] = idx;
      }
    }
  }

  bool valid() const { return frames >= ntau; }
  // mean sample time of bit k after the start edge (us): integral of P(sample > TAU)
  float meanUs(uint8_t k) const {
    return TAU_US(0) + TAU_STEP * TICK_US * ((float)ones[k] * ntau / frames - 0.5f);
  }
  float minUs(uint8_t k) const { return TAU_US(lo[k]) - 0.5f * TAU_STEP * TICK_US; }
  float maxUs(uint8_t k) const {
    return hi[k] == 0xFFFF ? TAU_US(0) : TAU_US(hi[k]) + 0.5f * TAU_STEP * TICK_US;
  }
  // least squares line through the mean sample times: first sample and period (us)
  void fit(float &first, float &period) const {
    float sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (uint8_t k = 0; k < 8; k++) {
      float y = meanUs(k);
      sx += k; sy += y; sxx += (float)k * k; sxy += k * y;
    }
    period = (8 * sxy - sx * sy) / (8 * sxx - sx * sx);
    first = (sy - period * sx) / 8;
  }
  // earliest / latest sample relative to the middle of its bit (us)
  float worstEarly() const {
    float w = 1e9f;
    for (uint8_t k = 0; k < 8; k++) if (minUs(k) - IDEAL_US(k) < w) w = minUs(k) - IDEAL_US(k);
    return w;
  }
  float worstLate() const {
    float w = -1e9f;
    for (uint8_t k = 0; k < 8; k++) if (maxUs(k) - IDEAL_US(k) > w) w = maxUs(k) - IDEAL_US(k);
    return w;
  }
};

struct TxStats {
  uint32_t inSum, bdSum;     // falling-to-falling intervals inside a 'U' frame (2 bits) / across frames
  uint16_t inN, bdN, inMin, inMax, bdMin, bdMax;

  void reset() {
    inSum = bdSum = 0;
    inN = bdN = 0;
    inMin = bdMin = 0xFFFF;
    inMax = bdMax = 0;
  }
  void addIn(uint16_t d) {
    inSum += d; inN++;
    if (d < inMin) inMin = d;
    if (d > inMax) inMax = d;
  }
  void addBd(uint16_t d) {
    bdSum += d; bdN++;
    if (d < bdMin) bdMin = d;
    if (d > bdMax) bdMax = d;
  }
  bool valid() const { return inN >= 100 && bdN >= 20; }
  float bitUs() const { return (float)inSum / inN * TICK_US / 2; }
  // the interval across frames is bit 7 + the stop bit
  float stopBits() const { return ((float)bdSum / bdN * TICK_US - bitUs()) / bitUs(); }
};

int cyclesOf(float us) { return (int)floorf(us / CYCLE_US + 0.5f); }

// ---- needed change in LH5801 cycles (not rounded, not limited to the v7.0 ranges)
float neededCycles(float measuredUs, float targetUs) { return (targetUs - measuredUs) / CYCLE_US; }

// ---- suggested constants (installer v7.0: TB 63..67, RB 63..71, RQ 8 or 12..20)
int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int suggestTB(int tb, float bitUs) {
  return clampInt(tb + cyclesOf(BIT_US - bitUs), 63, 67);
}

int suggestRB(int rb, float periodUs) {
  return clampInt(rb + cyclesOf(BIT_US - periodUs), 63, 71);
}

int snapRQ(int rq) {
  if (rq <= 10) return 8;
  return clampInt(rq, 12, 20);
}

// Change of RB moves sample k by (k+1)*dRB cycles. RQ moves all samples:
// choose it so that the earliest and the latest sample of all bits (both
// probe modes) are equally far from the middle of their bits.
void centerRQ(const ProbeStats *ps, uint8_t n, int dRB, float &shiftUs, float &early, float &late) {
  early = 1e9f;
  late = -1e9f;
  for (uint8_t m = 0; m < n; m++) {
    if (!ps[m].valid()) continue;
    for (uint8_t k = 0; k < 8; k++) {
      float mv = (k + 1) * dRB * CYCLE_US;
      float e = ps[m].minUs(k) + mv - IDEAL_US(k);
      float l = ps[m].maxUs(k) + mv - IDEAL_US(k);
      if (e < early) early = e;
      if (l > late) late = l;
    }
  }
  shiftUs = -(early + late) / 2;
}
// ==== analysis end

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);   // RS, E, D4, D5, D6, D7 (LCD Keypad Shield)

#define TX_MARK() (PORTC |= _BV(4))     // A4 -> PC-1500 PB2: mark (high)
#define TX_SPACE() (PORTC &= ~_BV(4))   // space (low)
#define LINE_LOW() (ACSR & _BV(ACO))    // A5 (PC-1500 PC7) below 1.1 V

const uint8_t SKETCH_VERSION = 3;
bool hdrGuess;                          // header not read: defaults assumed
uint8_t buf[256];                       // echo of one block / edge times of the header
uint32_t pcBitQ8 = BIT_Q8;              // TX bit of the PC-1500 as measured (ticks * 256)
uint32_t rng;
uint8_t hdr[7];                         // "CAL", TB, RB, RQ, version
ProbeStats probe[2];                    // 0 = fast (back to back), 1 = slow (pauses)
TxStats txs;
bool txOk;
uint16_t randBytes[2], randErr[2], randLost[2];
int8_t marginErr[2][13];                // -6..+6 %, errors (capped at 99), -1 = no answer
bool aborted;
// ---------------------------------------------------------------- timing helpers
static inline void waitUntil(uint16_t t) {
  while ((int16_t)(TCNT1 - t) < 0) {
  }
}

// wait about ms milliseconds with interrupts off (Timer1 overflows every 4.096 ms)
void waitMs(uint16_t ms) {
  uint32_t n = (uint32_t)ms * 1000 / 4096 + 1;
  TIFR1 = _BV(TOV1);
  while (n) {
    if (TIFR1 & _BV(TOV1)) {
      TIFR1 = _BV(TOV1);
      n--;
    }
  }
}

// wait until the line from the PC-1500 has been at mark for about 200 us,
// then forget older captures; false if it stays low for timeoutMs
bool waitIdle(uint32_t timeoutMs) {
  uint32_t n = timeoutMs * 1000 / 4096 + 1;
  uint16_t t = TCNT1;
  TIFR1 = _BV(TOV1);
  while ((uint16_t)(TCNT1 - t) < 3200) {
    if (LINE_LOW()) t = TCNT1;
    if (TIFR1 & _BV(TOV1)) {
      TIFR1 = _BV(TOV1);
      if (--n == 0) return false;
    }
  }
  TIFR1 = _BV(ICF1);
  return true;
}

// next falling edge from the PC-1500 (input capture); false after timeoutMs
bool waitFall(uint16_t *t, uint32_t timeoutMs) {
  uint32_t n = timeoutMs * 1000 / 4096 + 1;
  TIFR1 = _BV(TOV1);
  while (!(TIFR1 & _BV(ICF1))) {
    if (TIFR1 & _BV(TOV1)) {
      TIFR1 = _BV(TOV1);
      if (--n == 0) return false;
    }
  }
  *t = ICR1;
  TIFR1 = _BV(ICF1);
  return true;
}

// receive one byte from the PC-1500, with its measured bit length pcBitQ8
// (the line must be at mark); false on time-out
bool rxByte(uint8_t *out, uint32_t timeoutMs) {
  uint16_t t0;
  if (!waitFall(&t0, timeoutMs)) return false;
  uint8_t b = 0;
  for (uint8_t k = 0; k < 8; k++) {
    waitUntil(t0 + (uint16_t)(((2UL * k + 3) * pcBitQ8) >> 9));  // middle of bit k
    if (!LINE_LOW()) b |= 1 << k;
  }
  waitUntil(t0 + (uint16_t)((19UL * pcBitQ8) >> 9));            // middle of the stop bit
  uint16_t ts = TCNT1;                                          // framing error: wait for
  while (LINE_LOW() && (uint16_t)(TCNT1 - ts) < 32000) {        // mark, at most 2 ms
  }
  TIFR1 = _BV(ICF1);                                            // forget edges inside the frame
  *out = b;
  return true;
}

uint8_t rnd8() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng >> 24;
}

// ---------------------------------------------------------------- one block

// Sends n frames and receives the echo; returns the number of bytes echoed,
// or -1 if the PC-1500 did not send "R" (ready).
// PROBE: frame i of the block is probe number first+i; DATA: random bytes
// from seed. gapBits: pause after each frame (0..6 bits, 7 = varying 0..6).
int16_t block(Kind kind, uint16_t first, uint16_t n, uint32_t seed, uint8_t gapBits, int8_t pct,
              uint16_t ntau) {
  uint8_t r;
  Serial.flush();
  cli();
  if (!waitIdle(2000) || !rxByte(&r, 40000) || r != 'R') {
    sei();
    return -1;
  }
  waitMs(200);                              // BASIC goes on to CALL SI
  uint32_t bitQ8 = BIT_Q8 * 100 / (100 + pct);
  uint32_t frameTicks = (10 * bitQ8) >> 8;
  rng = seed;
  uint16_t base = TCNT1 + 400;
  uint32_t vt = 0;
  TX_MARK();
  for (uint16_t i = 0; i < n; i++) {
    uint16_t t0 = base + (uint16_t)vt;
    if (kind == PROBE) {
      uint16_t tau = TAU0 + ((first + i) % ntau) * TAU_STEP;
      waitUntil(t0);
      TX_SPACE();
      waitUntil(t0 + tau);
      TX_MARK();
    } else {
      uint8_t b = rnd8();
      waitUntil(t0);
      TX_SPACE();
      for (uint8_t k = 0; k < 9; k++) {     // data bits 0..7, then the stop bit
        uint16_t te = t0 + (uint16_t)(((k + 1) * bitQ8) >> 8);
        bool one = k == 8 || ((b >> k) & 1);
        waitUntil(te);
        if (one) TX_MARK();
        else TX_SPACE();
      }
    }
    uint8_t gap = gapBits == 7 ? (uint8_t)((i * 5 + (i >> 3)) % 7) : gapBits;   // (not from rnd8)
    vt += frameTicks + (uint32_t)gap * ((bitQ8 + 128) >> 8);
  }
  waitUntil(base + (uint16_t)vt);
  // echo: the first byte comes after SERIN returns (at once after 255 bytes,
  // else after its 0.5 s time-out), then back to back
  uint16_t got = 0;
  if (waitIdle(2000) && rxByte(&buf[0], 3000)) {
    got = 1;
    while (got < n && rxByte(&buf[got], 6)) got++;
  }
  sei();
  return got;
}

void show(const __FlashStringHelper *a, int v, int of) {
  lcd.setCursor(0, 1);
  lcd.print(a);
  lcd.print(' ');
  lcd.print(v);
  lcd.print('/');
  lcd.print(of);
  lcd.print(F("      "));
}

// ---------------------------------------------------------------- the tests
void runProbes(uint8_t mode) {
  ProbeStats &ps = probe[mode];
  const uint16_t ntau = mode ? NTAU_SLOW : NTAU_FAST;
  ps.reset(ntau);
  const uint16_t total = 4 * ntau;
  uint16_t nb = (total + 254) / 255;
  for (uint16_t blk = 0; blk < nb && !aborted; blk++) {
    uint16_t first = blk * 255;
    uint16_t n = total - first < 255 ? total - first : 255;
    show(mode ? F("Probe slow") : F("Probe fast"), blk + 1, nb);
    int16_t got = block(PROBE, first, n, 0, mode ? 4 : 0, 0, ntau);
    if (got < 0) {
      aborted = true;
      return;
    }
    if (got != (int16_t)n) {
      ps.lostBlocks++;
      continue;
    }
    // fast: the first frame of a block is found by the slow polls; skip it
    for (uint16_t i = mode ? 0 : 1; i < n; i++) ps.add((first + i) % ntau, buf[i]);
  }
}

uint16_t compareData(uint32_t seed, uint16_t n, int16_t got) {
  rng = seed;
  uint16_t err = 0;
  for (uint16_t i = 0; i < n; i++) {
    uint8_t b = rnd8();
    if ((int16_t)i >= got || buf[i] != b) err++;
  }
  return err;
}

void runData() {
  for (uint8_t mode = 0; mode < 2 && !aborted; mode++) {
    randBytes[mode] = randErr[mode] = randLost[mode] = 0;
    for (uint8_t blk = 0; blk < 2 && !aborted; blk++) {
      show(mode ? F("Data pauses") : F("Data fast"), blk + 1, 2);
      uint32_t seed = 0x1234567UL + mode * 1000 + blk;
      int16_t got = block(DATA, 0, 255, seed, mode ? 7 : 0, 0, 1);
      if (got < 0) {
        aborted = true;
        return;
      }
      randBytes[mode] += 255;
      randErr[mode] += compareData(seed, 255, got);
      if (got != 255) randLost[mode]++;
    }
  }
}

void runMargins() {
  for (uint8_t mode = 0; mode < 2 && !aborted; mode++) {
    for (int8_t p = -6; p <= 6 && !aborted; p++) {
      show(mode ? F("Margin pause") : F("Margin fast"), p + 7, 13);
      uint32_t seed = 0x89ABCDEUL + mode * 100 + p;
      int16_t got = block(DATA, 0, 255, seed, mode ? 3 : 0, p, 1);
      if (got < 0) {
        aborted = true;
        return;
      }
      uint16_t e = compareData(seed, 255, got);
      marginErr[mode][p + 6] = e > 99 ? 99 : e;
    }
  }
}

// The header comes before the 'U' burst, so its bit length is not known yet:
// record the times of all its edges (input capture, switched between falling
// and rising) and decode it later with the bit length measured on the burst.
uint16_t *const edgeDt = (uint16_t *)buf;   // ticks from one edge to the next (128 entries)
uint8_t nEdges;                             // edge 0 = first falling edge, then alternating

bool recordHeader() {
  Serial.flush();
  cli();
  uint16_t prev, t;
  bool ok = waitIdle(60000) && waitFall(&prev, 60000);   // start bit of the first byte
  nEdges = 0;
  bool low = true;
  while (ok && nEdges < 128) {
    waitUntil(prev + 160);                   // 10 us after an edge: no chatter of slow edges
    if (low) TCCR1B &= ~_BV(ICES1);          // next: line rises (comparator output falls)
    else TCCR1B |= _BV(ICES1);               // next: line falls
    TIFR1 = _BV(ICF1);
    if (!waitFall(&t, 10)) break;            // (any selected edge) none for 8-12 ms: the end
    edgeDt[nEdges++] = t - prev;
    prev = t;
    low = !low;
  }
  TCCR1B |= _BV(ICES1);                      // back to falling edges only
  TIFR1 = _BV(ICF1);
  sei();
  return ok;
}

// line level at t ticks after the first edge: high after an even number of edges
bool levelAt(uint32_t t) {
  uint32_t te = 0;
  uint8_t c = 1;
  for (uint8_t j = 0; j < nEdges; j++) {
    te += edgeDt[j];
    if (te > t) break;
    c++;
  }
  return !(c & 1);
}

// decode up to 7 bytes from the recorded edges with the bit length pcBitQ8
uint8_t decodeHeader() {
  float bit = pcBitQ8 / 256.0f;              // ticks
  uint32_t ts = 0;                           // start of the current frame
  uint8_t n = 0;
  while (n < 7) {
    uint8_t b = 0;
    for (uint8_t k = 0; k < 8; k++)
      if (levelAt(ts + (uint32_t)((k + 1.5f) * bit))) b |= 1 << k;
    hdr[n++] = b;
    // next start bit: the first falling edge after the middle of the stop bit
    uint32_t after = ts + (uint32_t)(9.5f * bit), te = 0;
    bool found = false;
    for (uint8_t j = 0; j < nEdges; j++) {
      te += edgeDt[j];                       // time of edge j+1; even edges are falling
      if (!((j + 1) & 1) && te > after) {
        ts = te;
        found = true;
        break;
      }
    }
    if (!found) break;
  }
  return n;
}

void measureTx() {
  txs.reset();
  Serial.flush();
  cli();
  uint16_t prev, t;
  txOk = waitIdle(2000) && waitFall(&prev, 15000);
  for (uint16_t i = 1; txOk && i < 64 * 5; i++) {
    if (!waitFall(&t, 5)) break;
    uint16_t d = t - prev;
    prev = t;
    if (i % 5 == 0) txs.addBd(d);
    else txs.addIn(d);
  }
  sei();
  txOk = txOk && txs.valid();
}

// ---------------------------------------------------------------- report
void printUs(float v, uint8_t width) {
  char s[12];
  dtostrf(v, width, 2, s);
  Serial.print(s);
}

void printProbe(uint8_t mode) {
  ProbeStats &ps = probe[mode];
  Serial.print(mode ? F("RX probe slow (4-bit pauses)") : F("RX probe fast (back to back)"));
  Serial.print(F(": frames "));
  Serial.print(ps.frames);
  Serial.print(F(", bad "));
  Serial.print(ps.bad);
  Serial.print(F(", lost blocks "));
  Serial.println(ps.lostBlocks);
  if (!ps.valid()) {
    Serial.println(F("  not enough frames"));
    return;
  }
  Serial.println(F("  bit  ideal    min    mean     max   (us after the start edge)"));
  for (uint8_t k = 0; k < 8; k++) {
    Serial.print(F("   "));
    Serial.print(k);
    printUs(IDEAL_US(k), 8);
    printUs(ps.minUs(k), 8);
    printUs(ps.meanUs(k), 8);
    printUs(ps.maxUs(k), 8);
    Serial.println();
  }
  float first, period;
  ps.fit(first, period);
  Serial.print(F("  needed change of RB: "));
  printUs(neededCycles(period, BIT_US), 1);
  Serial.println(F(" cycles"));
  Serial.print(F("  fit: bit 0 at "));
  printUs(first, 1);
  Serial.print(F(" us (ideal 78.13), period "));
  printUs(period, 1);
  Serial.print(F(" us ("));
  printUs((period / BIT_US - 1) * 100, 1);
  Serial.println(F(" %)"));
  Serial.print(F("  worst sample vs middle of bit: "));
  printUs(ps.worstEarly() / BIT_US, 1);
  Serial.print(F(" .. "));
  printUs(ps.worstLate() / BIT_US, 1);
  Serial.println(F(" bits (limit +-0.50)"));
}

void report() {
  Serial.println();
  Serial.println(F("=== PC-1500 SERINOUT v7.0 19200 calibration report ==="));
  Serial.print(F("sketch v"));
  Serial.print(SKETCH_VERSION);
  Serial.print(F(", PC-1500 program v"));
  Serial.print(hdr[6]);
  Serial.print(hdrGuess ? F(", header not read, assumed TB=") : F(", installed TB="));
  Serial.print(hdr[3]);
  Serial.print(F(" RB="));
  Serial.print(hdr[4]);
  Serial.print(F(" RQ="));
  Serial.println(hdr[5]);
  if (txOk) {
    Serial.print(F("TX 64 x 'U': bit "));
    printUs(txs.bitUs(), 1);
    Serial.print(F(" us (ideal 52.08, "));
    printUs((txs.bitUs() / BIT_US - 1) * 100, 1);
    Serial.print(F(" %), 2 bits "));
    printUs(txs.inMin * TICK_US, 1);
    Serial.print(F(".."));
    printUs(txs.inMax * TICK_US, 1);
    Serial.print(F(" us, stop bit "));
    printUs(txs.stopBits(), 1);
    Serial.println(F(" bits"));
    Serial.print(F("   = "));
    printUs(txs.bitUs() / CYCLE_US, 1);
    Serial.print(F(" cycles at 1.3 MHz for TB="));
    Serial.print(hdr[3]);
    Serial.print(F("; needed change of TB: "));
    printUs(neededCycles(txs.bitUs(), BIT_US), 1);
    Serial.println(F(" cycles"));
  } else {
    Serial.println(F("TX: no clean 'U' burst received"));
  }
  if (aborted) Serial.println(F("ABORTED: no 'R' from the PC-1500 (results below are partial)"));
  printProbe(0);
  printProbe(1);
  for (uint8_t m = 0; m < 2; m++) {
    Serial.print(m ? F("Random data with pauses 0-6 bits: ") : F("Random data back to back: "));
    Serial.print(randBytes[m]);
    Serial.print(F(" bytes, errors "));
    Serial.print(randErr[m]);
    Serial.print(F(", short echoes "));
    Serial.println(randLost[m]);
  }
  for (uint8_t m = 0; m < 2; m++) {
    Serial.print(m ? F("Baud margin, 3-bit pauses (errors/255): ") : F("Baud margin, back to back (errors/255): "));
    for (int8_t p = -6; p <= 6; p++) {
      if (p > -6) Serial.print(' ');
      if (p > 0) Serial.print('+');
      Serial.print(p);
      Serial.print(F("%:"));
      Serial.print(marginErr[m][p + 6]);
    }
    Serial.println();
  }
  // suggestions
  int tb = hdr[3], rb = hdr[4], rq = hdr[5];
  int ntb = txOk ? suggestTB(tb, txs.bitUs()) : tb;
  int nrb = rb, nrq = rq;
  if (probe[0].valid() || probe[1].valid()) {
    float f, p, sp = 0;
    uint8_t c = 0;
    for (uint8_t m = 0; m < 2; m++) {
      if (probe[m].valid()) {
        probe[m].fit(f, p);
        sp += p;
        c++;
      }
    }
    nrb = suggestRB(rb, sp / c);
    float shift, early, late;
    centerRQ(probe, 2, nrb - rb, shift, early, late);
    nrq = snapRQ(rq + cyclesOf(shift));
    float moved = (nrq - rq) * CYCLE_US;
    Serial.print(F("Expected with the suggestion: worst sample "));
    printUs((early + moved) / BIT_US, 1);
    Serial.print(F(" .. "));
    printUs((late + moved) / BIT_US, 1);
    Serial.println(F(" bits"));
  }
  Serial.print(F("SUGGESTED: TB="));
  Serial.print(ntb);
  Serial.print(F(" RB="));
  Serial.print(nrb);
  Serial.print(F(" RQ="));
  Serial.print(nrq);
  Serial.print(F("   (installed TB="));
  Serial.print(tb);
  Serial.print(F(" RB="));
  Serial.print(rb);
  Serial.print(F(" RQ="));
  Serial.print(rq);
  Serial.println(F(")"));
  if (txOk && (ntb != tb + (int)floorf(neededCycles(txs.bitUs(), BIT_US) + 0.5f)))
    Serial.println(F("NOTE: the TX change is outside the range of v7.0 (TB 63..67): code change needed"));
  if (nrb == 63 || nrb == 71 || nrq == 8 || nrq == 20)
    Serial.println(F("NOTE: an RX value is at the end of its range: code change may be needed"));
  Serial.println(F("=== end of report: please copy everything from the first === line ==="));
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  lcd.begin(16, 2);
  pinMode(A4, OUTPUT);                 // to the PC-1500 RX (PB2)
  digitalWrite(A4, HIGH);              // idle = mark
  pinMode(A5, INPUT_PULLUP);           // from the PC-1500 TX (PC7), through the diode
  ADCSRA &= ~_BV(ADEN);                // ADC off: its multiplexer feeds the comparator
  ADCSRB |= _BV(ACME);
  ADMUX = (ADMUX & 0xF0) | 5;          // comparator "-" input = A5
  ACSR = _BV(ACBG) | _BV(ACIC);        // "+" = bandgap 1.1 V, output -> Timer1 input capture
  TCCR1A = 0;                          // Timer1: normal mode, 16 MHz,
  TCCR1B = _BV(ICNC1) | _BV(ICES1) | _BV(CS10);   // capture when the line falls (noise canceler)
  TCCR1C = 0;
  TIMSK1 = 0;
  delay(10);
  Serial.print(F("PC-1500 SERINOUT v7.0 (19200 bps) calibration, Arduino sketch v"));
  Serial.println(SKETCH_VERSION);
}

void loop() {
  lcd.clear();
  lcd.print(F("UART 19200 calib"));
  lcd.setCursor(0, 1);
  lcd.print(F("Waiting PC-1500"));
  uint16_t lowFor = 0;
  while (LINE_LOW() && lowFor < 200) {
    delay(1);
    lowFor++;
  }
  if (lowFor >= 200) Serial.println(F("Warning: the line from PC7 (A5) stays low - wiring, diode, polarity?"));
  Serial.println(F("Waiting for the PC-1500: RUN the calibration program and press ENTER."));
  if (!recordHeader()) return;
  lcd.setCursor(0, 1);
  lcd.print(F("TX timing...    "));
  measureTx();
  pcBitQ8 = txOk ? (uint32_t)((txs.inSum * 128ULL) / txs.inN) : BIT_Q8;   // 2 bits per interval
  uint8_t n = decodeHeader();
  if (txOk) {
    Serial.print(F("PC-1500 TX bit: "));
    printUs(txs.bitUs(), 1);
    Serial.print(F(" us ("));
    printUs((txs.bitUs() / BIT_US - 1) * 100, 1);
    Serial.println(F(" % against 19200 bps); the PC-1500 is received with this bit length"));
  } else {
    Serial.println(F("No 'U' burst after the header: the bit length could not be measured."));
  }
  hdrGuess = n < 7 || hdr[0] != 'C' || hdr[1] != 'A' || hdr[2] != 'L';
  if (hdrGuess) {
    Serial.print(F("Header not understood, received:"));
    for (uint8_t i = 0; i < n; i++) {
      Serial.print(' ');
      Serial.print(hdr[i], HEX);
    }
    Serial.println();
    if (!txOk) {
      Serial.println(F("No TX measurement either - check the wiring and that SERINOUT v7.0 is installed"));
      Serial.println(F("with normal polarity. (If the PC-1500 program still runs: BREAK, then RUN 210.)"));
      return;
    }
    Serial.println(F("Going on with the measurement; TB=66 RB=66 RQ=12 assumed."));
    hdr[3] = 66;
    hdr[4] = 66;
    hdr[5] = 12;
    hdr[6] = 0;
  }
  Serial.print(F("PC-1500: TB="));
  Serial.print(hdr[3]);
  Serial.print(F(" RB="));
  Serial.print(hdr[4]);
  Serial.print(F(" RQ="));
  Serial.println(hdr[5]);
  aborted = false;
  for (uint8_t m = 0; m < 2; m++) {
    probe[m].reset(m ? NTAU_SLOW : NTAU_FAST);
    for (uint8_t i = 0; i < 13; i++) marginErr[m][i] = -1;
    randBytes[m] = randErr[m] = randLost[m] = 0;
  }
  Serial.println(F("Measuring, less than a minute..."));
  runProbes(0);
  if (!aborted) runProbes(1);
  if (!aborted) runData();
  if (!aborted) runMargins();
  report();
  lcd.setCursor(0, 1);
  lcd.print(F("Done: see report"));
  delay(3000);
}
