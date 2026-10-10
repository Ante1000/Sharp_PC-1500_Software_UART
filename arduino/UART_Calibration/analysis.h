// analysis.h - statistics and suggested timing constants of the
// UART_Calibration sketch. No hardware access, so the same code also
// builds on a PC for tests.
#ifndef UART_CALIBRATION_ANALYSIS_H
#define UART_CALIBRATION_ANALYSIS_H

#include <stdint.h>
#include <math.h>

#define TICK_US 0.0625f                  // Timer1 of the UNO: 16 MHz
#define BIT_US (1000000.0f / 19200.0f)   // 52.083 us
#define CYCLE_US (1.0f / 1.3f)           // one LH5801 cycle at 1.3 MHz: 0.769 us
#define BIT_Q8 213333UL                  // one bit at 19200 bps: 833.333 ticks * 256

// Probe frames: the start bit begins at t = 0 and the line stays at space
// until TAU, then goes back to mark until the end of the frame. A sample of
// the PC-1500 taken before TAU reads 0, one taken after TAU reads 1, so the
// byte received is 0xFF << j: j samples came before TAU. TAU runs from 1.0
// bit in steps of 1 us, to 9.0 bits (frames back to back) or 10.0 bits
// (frames with pauses, for a PC-1500 whose samples come late).
#define TAU0 833        // ticks (1.0 bit)
#define TAU_STEP 16     // ticks (1 us)
#define NTAU_FAST 417   // 1.0 .. 9.0 bits
#define NTAU_SLOW 469   // 1.0 .. 10.0 bits

static inline float tauUs(uint16_t idx) { return (TAU0 + (float)idx * TAU_STEP) * TICK_US; }
static inline float idealUs(uint8_t k) { return (1.5f + k) * BIT_US; }   // middle of data bit k
static inline int cyclesOf(float us) { return (int)floorf(us / CYCLE_US + 0.5f); }

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
    return tauUs(0) + TAU_STEP * TICK_US * ((float)ones[k] * ntau / frames - 0.5f);
  }
  float minUs(uint8_t k) const { return tauUs(lo[k]) - 0.5f * TAU_STEP * TICK_US; }
  float maxUs(uint8_t k) const {
    return hi[k] == 0xFFFF ? tauUs(0) : tauUs(hi[k]) + 0.5f * TAU_STEP * TICK_US;
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
    for (uint8_t k = 0; k < 8; k++) if (minUs(k) - idealUs(k) < w) w = minUs(k) - idealUs(k);
    return w;
  }
  float worstLate() const {
    float w = -1e9f;
    for (uint8_t k = 0; k < 8; k++) if (maxUs(k) - idealUs(k) > w) w = maxUs(k) - idealUs(k);
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

// ---- needed change in LH5801 cycles (not rounded, not limited to the v7.0 ranges)
static inline float neededCycles(float measuredUs, float idealUs) { return (idealUs - measuredUs) / CYCLE_US; }

// ---- suggested constants (installer v7.0: TB 63..67, RB 63..71, RQ 8 or 12..20)
static inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static inline int suggestTB(int tb, float bitUs) {
  return clampInt(tb + cyclesOf(BIT_US - bitUs), 63, 67);
}

static inline int suggestRB(int rb, float periodUs) {
  return clampInt(rb + cyclesOf(BIT_US - periodUs), 63, 71);
}

static inline int snapRQ(int rq) {
  if (rq <= 10) return 8;
  return clampInt(rq, 12, 20);
}

// Change of RB moves sample k by (k+1)*dRB cycles. RQ moves all samples:
// choose it so that the earliest and the latest sample of all bits (both
// probe modes) are equally far from the middle of their bits.
static inline void centerRQ(const ProbeStats *ps, uint8_t n, int dRB, float &shiftUs, float &early,
                            float &late) {
  early = 1e9f;
  late = -1e9f;
  for (uint8_t m = 0; m < n; m++) {
    if (!ps[m].valid()) continue;
    for (uint8_t k = 0; k < 8; k++) {
      float mv = (k + 1) * dRB * CYCLE_US;
      float e = ps[m].minUs(k) + mv - idealUs(k);
      float l = ps[m].maxUs(k) + mv - idealUs(k);
      if (e < early) early = e;
      if (l > late) late = l;
    }
  }
  shiftUs = -(early + late) / 2;
}

#endif
