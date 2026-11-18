// ============================================================================
// VintageRack DSP core (C++) — line-by-line port of web/dsp.js
//   Pre73  : Class-A mic-pre + inductor EQ (Classic / Classic+)
//   Fet76  : FET compressor (Early / Low-noise / Late)
//   Opto3A : solid-state opto leveler
// No JUCE dependency, so it can be unit-tested on its own.
// ============================================================================
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <vector>
#include <tuple>
#include <cstdint>

namespace vr
{
constexpr double kPi = 3.14159265358979323846;
inline double dbToLin (double d) { return std::pow (10.0, d / 20.0); }
inline double linToDb (double l) { return 20.0 * std::log10 (std::max (l, 1e-9)); }
template <typename T> inline T clampv (T v, T a, T b) { return std::min (b, std::max (a, v)); }

// ----------------------------------------------------------------- specs
namespace spec
{
    // index 0 = OFF where applicable
    inline const std::array<double, 5> hpf      { 0, 50, 80, 160, 300 };
    inline const std::array<double, 4> low      { 35, 60, 110, 220 };
    inline const std::array<double, 6> mid      { 360, 700, 1600, 3200, 4800, 7200 };
    inline const std::array<double, 3> highPlus { 10000, 12000, 16000 };
    constexpr double highClassic = 12000;
    inline const std::array<double, 5> lpfPlus  { 0, 18000, 10000, 8000, 6000 };
    // continuous ("無段階") ranges
    constexpr double contHpf[2] { 10, 600 }, contLow[2] { 20, 500 }, contMid[2] { 150, 10000 }, contHigh[2] { 3000, 20000 },
                     contLpf[2] { 2000, 22000 }, contSc[2] { 500, 10000 };
    constexpr double hpfOffBelow = 16, lpfOffAbove = 21000;
    inline int micGainDb  (int step) { return 20 + 5 * clampv (step, 0, 12); }   // 20..80
    inline int lineGainDb (int step) { return -10 + 5 * clampv (step, 0, 6); }   // -10..+20
}

// ----------------------------------------------------------------- biquad
struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coefs peak (double fs, double f, double q, double g)
{
    const double A = std::pow (10.0, g / 40), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al / A;
    return { (1 + al * A) / a0, -2 * c / a0, (1 - al * A) / a0, -2 * c / a0, (1 - al / A) / a0 };
}
inline Coefs lowShelf (double fs, double f, double q, double g)
{
    const double A = std::pow (10.0, g / 40), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w), s = 2 * std::sqrt (A) * al;
    const double a0 = (A + 1) + (A - 1) * c + s;
    return { A * ((A + 1) - (A - 1) * c + s) / a0, 2 * A * ((A - 1) - (A + 1) * c) / a0, A * ((A + 1) - (A - 1) * c - s) / a0,
             -2 * ((A - 1) + (A + 1) * c) / a0, ((A + 1) + (A - 1) * c - s) / a0 };
}
inline Coefs highShelf (double fs, double f, double q, double g)
{
    const double A = std::pow (10.0, g / 40), w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w), s = 2 * std::sqrt (A) * al;
    const double a0 = (A + 1) - (A - 1) * c + s;
    return { A * ((A + 1) + (A - 1) * c + s) / a0, -2 * A * ((A - 1) + (A + 1) * c) / a0, A * ((A + 1) + (A - 1) * c - s) / a0,
             2 * ((A - 1) - (A + 1) * c) / a0, ((A + 1) - (A - 1) * c - s) / a0 };
}
inline Coefs hp2 (double fs, double f, double q)
{
    const double w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
    return { (1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - al) / a0 };
}
inline Coefs lp2 (double fs, double f, double q)
{
    const double w = 2 * kPi * f / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
    return { (1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0, -2 * c / a0, (1 - al) / a0 };
}
inline Coefs hp1 (double fs, double f) { const double k = std::tan (kPi * f / fs), a0 = k + 1; return { 1 / a0, -1 / a0, 0, (k - 1) / a0, 0 }; }
inline Coefs lp1 (double fs, double f) { const double k = std::tan (kPi * f / fs), a0 = k + 1; return { k / a0, k / a0, 0, (k - 1) / a0, 0 }; }

inline double magDb (const Coefs& c, double fs, double f)
{
    const double w = 2 * kPi * f / fs;
    const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
    const double nr = c.b0 + c.b1 * cr + c.b2 * c2r, ni = c.b1 * ci + c.b2 * c2i;
    const double dr = 1 + c.a1 * cr + c.a2 * c2r, di = c.a1 * ci + c.a2 * c2i;
    return 10 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
}

struct Biquad
{
    Coefs c; double z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

struct DCBlock
{
    double R = 0.999, x1 = 0, y1 = 0;
    void prepare (double fs) { R = 1 - 2 * kPi * 5 / fs; x1 = y1 = 0; }
    inline double process (double x) noexcept { const double y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

inline double sat (double x, double ceil, double bias) noexcept
{
    const double u = x / ceil, tb = std::tanh (bias);
    return ceil * (std::tanh (u + bias) - tb) / (1 - tb * tb);
}
inline double coef (double ms, double fs) { return std::exp (-1.0 / (std::max (ms, 0.001) * 0.001 * fs)); }

// ================================================================= circuit components ("CIRCUIT" panel)
// Transformer: fc/lf = how strongly low frequencies drive the core, d = core drive, ceil/b = clip & asymmetry.
// Response fields = small-signal frequency fingerprint. Modeled tendencies, not measurements. Mirrors web/dsp.js.
namespace circuit
{
    struct X { double fc, lf, d, ceil, b; };
    struct In73  { const char* name; X x; double hp, bumpF, bumpG, hsF, hsG; };
    struct Amp73 { const char* name; double b, ceil, floor; };
    struct Out73 { const char* name; X x; double hsF, hsG; };
    inline const In73 in73[4] = {
        { "Vintage (nickel core)", { 90, 1.0, 0.7, 1.8, 0.10 }, 12, 60, 0.9, 17000, -0.8 },
        { "Mid-era",            { 90, 0.75, 0.57, 2.1, 0.08 }, 10, 60, 0.7, 18000, -0.6 },
        { "Modern reissue", { 100, 0.6, 0.48, 2.4, 0.06 }, 10, 70, 0.5, 19000, -0.4 },
        { "Transformerless",    { 100, 0, 0.001, 3, 0 }, 5, 60, 0, 20000, 0 } };
    inline const Amp73 amp73[3] = {
        { "Vintage NOS transistors", 0.14, 1.0, -92 }, { "Current transistors", 0.10, 1.1, -96 }, { "Modern low-noise", 0.05, 1.3, -104 } };
    inline const Out73 out73[3] = {
        { "Vintage output",   { 120, 0.5, 0.53, 2.7, 0.04 }, 16000, -0.5 },
        { "Modern reissue", { 120, 0.4, 0.44, 3.3, 0.03 }, 18000, -0.3 },
        { "Transformerless",    { 120, 0, 0.001, 4, 0 }, 20000, 0 } };

    // FET comp: index 0 = "Model standard" (resolved from the selected model through std76)
    struct Fet   { const char* name; double thd, bias; };
    struct In76  { const char* name; X x; double hp, lsF, lsG, pkF, pkG, hsF, hsG; };
    struct Out76 { const char* name; double ceil, b, floor; bool hasX; X x; double pkF, pkG, hsF, hsG, lsG; };
    inline const Fet fet76[5] = { { "Model standard", 1, 1 },
        { "Early FET (original)", 1.0, 1.0 }, { "Later FET", 0.7, 0.6 }, { "NOS unselected (rough)", 1.8, 1.6 }, { "Selected low-distortion", 0.4, 0.4 } };
    inline const In76 in76[4] = { { "Model standard", { 0, 0, 0, 1, 0 }, 0, 0, 0, 0, 0, 0, 0 },
        { "Nickel core (vintage)", { 80, 1.0, 0.62, 1.95, 0.08 }, 15, 45, 0.6, 3000, 0, 11000, -1.2 },
        { "Steel core",            { 90, 0.7, 0.48, 2.4, 0.05 }, 12, 45, 0, 3000, 0.3, 15000, -0.6 },
        { "Transformerless",       { 90, 0, 0.001, 3, 0 }, 6, 45, 0, 3000, 0, 20000, 0 } };
    inline const Out76 out76[4] = { { "Model standard", 1, 1, -88, false, { 0, 0, 0, 1, 0 }, 0, 0, 0, 0, 0 },
        { "Class-A + transformer",   1.0, 1.0, -88, true,  { 120, 0.5, 0.44, 3.0, 0.04 }, 5000, 0, 16000, -0.3, 0 },
        { "Class-A transformerless", 1.15, 0.8, -92, false, { 0, 0, 0, 1, 0 }, 5000, 0.6, 16000, 0, 0 },
        { "Hot-rodded (high gain)",  0.8, 1.6, -84, true,  { 120, 0.65, 0.57, 2.7, 0.06 }, 5000, 0, 14000, -0.8, 0.4 } };
    inline const int std76[3][3] = { { 1, 1, 1 }, { 1, 2, 1 }, { 2, 2, 2 } };   // [revision] → fet, inX, out

    struct Cell  { const char* name; double atk, rel, slow; };
    struct Amp3A { const char* name; double ceil, b, pkG, floor; };
    struct Out3A { const char* name; X x; double hp, lsG, hsF, hsG; };
    inline const Cell cell3a[3] = { { "Opto cell (new)", 1.0, 1.0, 1.0 }, { "Opto cell (aged)", 1.3, 1.4, 1.8 }, { "Fast cell (modified)", 0.6, 0.6, 0.6 } };
    inline const Amp3A amp3a[2] = { { "Discrete Class-A (original)", 1.8, 0.05, 0.8, -94 }, { "Later IC amp", 2.4, 0.02, 0.3, -100 } };
    inline const Out3A out3a[3] = {
        { "Original transformer", { 110, 0.9, 1.1, 3.0, 0.04 }, 18, -0.4, 14000, -0.8 },
        { "Current transformer",  { 110, 0.7, 0.85, 3.6, 0.03 }, 15, -0.2, 16000, -0.5 },
        { "Transformerless",      { 110, 0, 0.001, 4, 0 }, 5, 0, 20000, 0 } };
}

// fixed-capacity biquad cascade (no allocation on the audio thread)
template <size_t N> struct Cascade
{
    std::array<Biquad, N> s; int n = 0;
    void set (const std::vector<Coefs>& c) { n = (int) std::min (c.size(), N); for (int k = 0; k < n; ++k) s[(size_t) k].c = c[(size_t) k]; }
    inline double process (double x) noexcept { for (int k = 0; k < n; ++k) x = s[(size_t) k].process (x); return x; }
    void reset() { for (auto& b : s) b.reset(); }
};

// Transformer core: LF-weighted saturation, unity gain at small signal. IRON 0..200 % (progressive, squared).
struct Xfmr
{
    double l = 0, a = 0, lf = 0, d = 1, ceil = 1, b = 0; bool on = false;
    void set (const circuit::X* x, double fs, double iron, double ceilScale)
    {
        on = x != nullptr && x->d > 0.01 && iron > 0;
        if (! on) return;
        a = 1 - std::exp (-2 * kPi * x->fc / fs); lf = x->lf;
        d = std::max (x->d * std::pow (iron, 2.0), 1e-3); ceil = x->ceil * ceilScale; b = x->b;
    }
    inline double process (double x) noexcept
    {
        if (! on) return x;
        l += a * (x - l);
        const double u = x + lf * l;
        return x + (sat (u * d, ceil, b) / d - u);
    }
    void reset() { l = 0; }
};

// Hiss + 50 Hz mains hum (odd harmonics). Deterministic LCG, identical to the JS version.
struct Noise
{
    uint32_t s = 1; double n = 0, ph = 0, a = 0, dph = 0, hiss = 0, hum = 0;
    void prepare (double fs, uint32_t seed) { s = seed; n = ph = 0; a = 1 - std::exp (-2 * kPi * 6000 / fs); dph = 2 * kPi * 50 / fs; }
    void set (double levelDb, bool on)
    {
        if (! on) { hiss = hum = 0; return; }
        const double norm = 0.57735 * std::sqrt (a / (2 - a));
        hiss = dbToLin (levelDb) / norm; hum = dbToLin (levelDb - 10);
    }
    inline double next() noexcept
    {
        if (hiss == 0) return 0;
        s = s * 1664525u + 1013904223u;
        const double w = (double) (s >> 8) / 16777216.0 * 2 - 1;
        n += a * (w - n);
        ph += dph; if (ph > 2 * kPi) ph -= 2 * kPi;
        return n * hiss + hum * (std::sin (ph) + 0.5 * std::sin (3 * ph) + 0.25 * std::sin (5 * ph));
    }
};
inline double noiseDb (double floor, double noise, double age) { return floor + (noise - 50) * 0.4 + 6 * age; }

// ================================================================= Pre / EQ
struct Pre73Params
{
    int variant = 0;          // 0 = Classic, 1 = Classic+ (Hi-Q, LPF, 3 HF points)
    bool mic = false;
    int gainDb = 0;           // actual front-panel dB (mic 20..80, line -10..20)
    int hpf = 0, lowFreq = 1, midFreq = 2, highFreq = 1, lpf = 0;
    double lowGain = 0, midGain = 0, highGain = 0, output = 0;
    bool hiQ = false, eqIn = true, phase = false;
    bool analog = true, cont = false;
    double hpfC = 80, lowC = 60, midC = 1600, highC = 12000, lpfC = 22000;   // continuous-mode frequencies
    int inX = 0, amp = 0, outX = 0;                                          // circuit
    double bias = 0, headroom = 24, iron = 100, age = 0, noise = 0;
    auto tie() const { return std::tie (variant, mic, gainDb, hpf, lowFreq, midFreq, highFreq, lpf, lowGain, midGain, highGain, output, hiQ, eqIn, phase,
                                        analog, cont, hpfC, lowC, midC, highC, lpfC, inX, amp, outX, bias, headroom, iron, age, noise); }
    bool operator== (const Pre73Params& o) const { return tie() == o.tie(); }
    bool operator!= (const Pre73Params& o) const { return ! (*this == o); }
    bool isPlus() const { return variant == 1; }
    double hpfHz()  const { if (cont) return hpfC < spec::hpfOffBelow ? 0.0 : hpfC; return spec::hpf[(size_t) clampv (hpf, 0, 4)]; }
    double lowHz()  const { return cont ? lowC : spec::low[(size_t) clampv (lowFreq, 0, 3)]; }
    double midHz()  const { return cont ? midC : spec::mid[(size_t) clampv (midFreq, 0, 5)]; }
    double highHz() const { if (cont) return highC; return isPlus() ? spec::highPlus[(size_t) clampv (highFreq, 0, 2)] : spec::highClassic; }
    double lpfHz()  const { if (! isPlus()) return 0.0; if (cont) return lpfC >= spec::lpfOffAbove ? 0.0 : lpfC; return spec::lpfPlus[(size_t) clampv (lpf, 0, 4)]; }
};

enum Band { kHPF = 0, kLow, kMid, kHigh, kLPF, kNumBands };
struct BandDesign { std::vector<Coefs> secs; double freq = 0; };

// skew: component-tolerance factor applied to every band frequency (AGE, right channel)
inline std::array<BandDesign, kNumBands> design73 (const Pre73Params& p, double fs, double skew = 1.0)
{
    std::array<BandDesign, kNumBands> b;
    const double ny = fs * 0.45;
    const double fH = p.hpfHz() * skew, fL = p.lowHz() * skew, fM = p.midHz() * skew, fHi = p.highHz() * skew, fLp = p.lpfHz() * skew;
    if (fH > 0) { b[kHPF].freq = fH; b[kHPF].secs = { hp1 (fs, fH), hp2 (fs, fH, 1.0) }; }
    b[kLow].freq = fL;
    if (std::abs (p.lowGain) > 0.01) b[kLow].secs = { lowShelf (fs, fL, 0.9, p.lowGain) };
    double mg = p.midGain;
    if (p.isPlus() && ! p.hiQ) mg = clampv (mg, -12.0, 12.0);
    const double q = (p.isPlus() && p.hiQ) ? 2.0 + 0.05 * std::abs (mg) : 0.55 + 0.035 * std::abs (mg);
    b[kMid].freq = fM;
    if (std::abs (mg) > 0.01) b[kMid].secs = { peak (fs, fM, q, mg) };
    b[kHigh].freq = fHi;
    if (std::abs (p.highGain) > 0.01) b[kHigh].secs = { highShelf (fs, std::min (fHi, ny), 0.55, p.highGain) };
    if (fLp > 0) { const double f = std::min (fLp, ny); b[kLPF].freq = fLp; b[kLPF].secs = { lp1 (fs, f), lp2 (fs, f, 1.0) }; }
    return b;
}

// ================================================================= FET / Opto parameter structs
struct Fet76Params
{
    int variant = 1;              // 0 Early (Blue), 1 Low-noise (Black), 2 Late (Silver)
    double input = 6, output = 0, attack = 3, release = 5;
    int ratio = 0;                // 0..3 = 4/8/12/20, 4 = ALL
    bool analog = true;
    int fet = 0, inX = 0, out = 0;                                           // circuit (0 = revision standard)
    double bias = 0, headroom = 24, detector = 50, iron = 100, age = 0, noise = 0;
    auto tie() const { return std::tie (variant, input, output, attack, release, ratio, analog, fet, inX, out, bias, headroom, detector, iron, age, noise); }
    bool operator== (const Fet76Params& o) const { return tie() == o.tie(); }
    bool operator!= (const Fet76Params& o) const { return ! (*this == o); }
    int fetIdx() const { return fet > 0 ? clampv (fet, 1, 4) : circuit::std76[clampv (variant, 0, 2)][0]; }
    int inIdx()  const { return inX > 0 ? clampv (inX, 1, 3) : circuit::std76[clampv (variant, 0, 2)][1]; }
    int outIdx() const { return out > 0 ? clampv (out, 1, 3) : circuit::std76[clampv (variant, 0, 2)][2]; }
};
struct Opto3AParams
{
    double peak = 40, gain = 50, hf = 0, scHz = 2500;
    bool limit = false, analog = true, cont = false;
    int cell = 0, amp = 0, outX = 0;                                         // circuit
    double memory = 50, headroom = 24, iron = 100, age = 0, noise = 0;
    double scFreq() const { return cont ? scHz : 2500.0; }
    auto tie() const { return std::tie (peak, gain, hf, scHz, limit, analog, cont, cell, amp, outX, memory, headroom, iron, age, noise); }
    bool operator== (const Opto3AParams& o) const { return tie() == o.tie(); }
    bool operator!= (const Opto3AParams& o) const { return ! (*this == o); }
};
inline Coefs design3aSC (const Opto3AParams& p, double fs) { return highShelf (fs, std::min (p.scFreq(), fs * 0.45), 0.6, p.hf / 100.0 * 10.0); }

// ----------------------------------------------------------------- analog coloration (small-signal)
inline std::vector<Coefs> analogTail (std::vector<Coefs> s, double age, double fs)
{
    if (age > 0) s.push_back (highShelf (fs, std::min (9000.0, fs * 0.45), 0.6, -1.5 * age));   // aged caps / tired iron
    return s;
}
inline std::vector<Coefs> designAnalog (const Pre73Params& p, double fs)
{
    if (! p.analog) return {};
    using namespace circuit;
    const auto& X = in73[clampv (p.inX, 0, 3)]; const auto& O = out73[clampv (p.outX, 0, 2)];
    auto hs = [fs] (double f, double q, double g) { return highShelf (fs, std::min (f, fs * 0.45), q, g); };
    std::vector<Coefs> s { hp1 (fs, X.hp) };
    if (X.bumpG != 0) s.push_back (peak (fs, X.bumpF, 0.8, X.bumpG));
    if (X.hsG != 0) s.push_back (hs (X.hsF, 0.5, X.hsG));
    if (O.hsG != 0) s.push_back (hs (O.hsF, 0.5, O.hsG));
    return analogTail (s, p.age / 100.0, fs);
}
inline std::vector<Coefs> designAnalog (const Fet76Params& p, double fs)
{
    if (! p.analog) return {};
    using namespace circuit;
    const auto& X = in76[p.inIdx()]; const auto& O = out76[p.outIdx()];
    auto hs = [fs] (double f, double q, double g) { return highShelf (fs, std::min (f, fs * 0.45), q, g); };
    std::vector<Coefs> s { hp1 (fs, X.hp) };
    if (X.lsG != 0) s.push_back (lowShelf (fs, X.lsF, 0.7, X.lsG));
    if (X.pkG != 0) s.push_back (peak (fs, X.pkF, 0.6, X.pkG));
    if (X.hsG != 0) s.push_back (hs (X.hsF, 0.5, X.hsG));
    if (O.lsG != 0) s.push_back (lowShelf (fs, 100, 0.7, O.lsG));
    if (O.pkG != 0) s.push_back (peak (fs, O.pkF, 0.6, O.pkG));
    if (O.hsG != 0) s.push_back (hs (O.hsF, 0.5, O.hsG));
    return analogTail (s, p.age / 100.0, fs);
}
inline std::vector<Coefs> designAnalog (const Opto3AParams& p, double fs)
{
    if (! p.analog) return {};
    using namespace circuit;
    const auto& A = amp3a[clampv (p.amp, 0, 1)]; const auto& O = out3a[clampv (p.outX, 0, 2)];
    auto hs = [fs] (double f, double q, double g) { return highShelf (fs, std::min (f, fs * 0.45), q, g); };
    std::vector<Coefs> s { hp1 (fs, O.hp) };
    if (O.lsG != 0) s.push_back (lowShelf (fs, 80, 0.7, O.lsG));
    if (A.pkG != 0) s.push_back (peak (fs, 2200, 0.7, A.pkG));
    if (O.hsG != 0) s.push_back (hs (O.hsF, 0.5, O.hsG));
    return analogTail (s, p.age / 100.0, fs);
}

// ================================================================= Pre / EQ
class Pre73
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (size_t c = 0; c < 2; ++c) { auto& st = ch[c]; st.dc.prepare (fs); st.secs.reset(); st.inX.reset(); st.outX.reset(); st.noise.prepare (fs, 12345u + 777u * (uint32_t) c); }
        update();
    }
    void setParams (const Pre73Params& np) { if (np != p) { p = np; update(); } }
    const Pre73Params& params() const { return p; }

    void process (float* const* chans, int numCh, int n) noexcept
    {
        float pk = 0;
        for (int c = 0; c < numCh && c < 2; ++c)
        {
            auto& st = ch[(size_t) c]; float* buf = chans[c];
            for (int i = 0; i < n; ++i)
            {
                double x = buf[i] * inG;
                if (p.analog) { x = st.inX.process (x); x = sat (x, ampCeil, ampBias); }          // input transformer → Class-A amp
                x = st.secs.process (x);
                if (p.analog) { x = sat (x, outCeil, ampBias * 0.5); x = st.outX.process (x); x += st.noise.next(); }   // output amp → transformer
                x = st.dc.process (x) * outG;
                buf[i] = (float) x;
                pk = std::max (pk, (float) std::abs (x));
            }
        }
        outPeak = pk;
    }
    float outPeak = 0;
    float grMeter = 0;

private:
    void update()
    {
        using namespace circuit;
        const auto& X = in73[clampv (p.inX, 0, 3)]; const auto& A = amp73[clampv (p.amp, 0, 2)]; const auto& O = out73[clampv (p.outX, 0, 2)];
        const double age = p.age / 100, iron = p.iron / 100, V = std::pow (p.headroom / 24, 1.5), ceilAge = 1 - 0.25 * age;
        const auto ana = designAnalog (p, fs);
        for (size_t c = 0; c < 2; ++c)
        {
            auto& st = ch[c];
            const auto bands = design73 (p, fs, c ? 1 + 0.03 * age : 1.0);   // AGE: right channel drifts
            std::vector<Coefs> all = ana;
            for (int b = 0; b < kNumBands; ++b)
                if (p.eqIn || b == kHPF || b == kLPF)
                    for (auto& k : bands[(size_t) b].secs) all.push_back (k);
            st.secs.set (all);
            st.inX.set (&X.x, fs, iron, ceilAge); st.outX.set (&O.x, fs, iron, ceilAge);
            st.noise.set (noiseDb (A.floor, p.noise, age), p.analog && p.noise > 0);
        }
        ampCeil = 1.6 * A.ceil * V * ceilAge; outCeil = 2.2 * A.ceil * V * ceilAge;
        ampBias = A.b + p.bias / 100 * 0.25;
        const double g = p.mic ? p.gainDb - 50 : p.gainDb;     // mic: assume mic-level source (−50 dB re line)
        inG = dbToLin (g) * (p.phase ? -1.0 : 1.0);
        outG = dbToLin (p.output);
    }
    struct Chan { Cascade<12> secs; DCBlock dc; Xfmr inX, outX; Noise noise; };
    std::array<Chan, 2> ch;
    Pre73Params p;
    double fs = 48000, inG = 1, outG = 1, ampCeil = 1.6, outCeil = 2.2, ampBias = 0.14;
};

// ================================================================= FET compressor
class Fet76
{
public:
    struct Var { double atkScale, relScale, knee, thd, bias, thr; };
    static const Var& var (int i) { static const Var v[3] = { { 1.25, 1.1, 8, 0.10, 0.18, -17 }, { 1.0, 1.0, 6, 0.06, 0.10, -18 }, { 0.9, 0.9, 4, 0.035, 0.06, -19 } }; return v[clampv (i, 0, 2)]; }

    void prepare (double sampleRate)
    {
        fs = sampleRate; env = grDb = slow = 0;
        for (size_t c = 0; c < 2; ++c) { dc[c].prepare (fs); col[c].reset(); inX[c].reset(); outX[c].reset(); noise[c].prepare (fs, c ? 24233u : 23456u); }
        update();
    }
    void setParams (const Fet76Params& np) { if (np != p) { p = np; update(); } }

    double gain (double xDb) const
    {
        const double o = xDb - thr; double gr;
        if (2 * o < -knee) gr = 0;
        else if (2 * std::abs (o) <= knee) gr = (1 - 1 / ratio) * (o + knee / 2) * (o + knee / 2) / (2 * knee);
        else gr = (1 - 1 / ratio) * o;
        if (all && o > 0) gr += 0.35 * o;
        return gr;
    }
    double inputDb() const { return p.input; }

    void process (float* const* chans, int numCh, int n) noexcept
    {
        const auto& v = var (p.variant);
        float* L = chans[0]; float* R = numCh > 1 ? chans[1] : chans[0];
        float pk = 0, grMax = 0;
        for (int i = 0; i < n; ++i)
        {
            double l = L[i] * inG, r = R[i] * inG;
            if (p.analog) { l = inX[0].process (l); r = inX[1].process (r); }
            const double lvl = linToDb (std::max (std::abs (l), std::abs (r)));
            const double target = gain (lvl);
            slow = slowC * slow + (1 - slowC) * grDb;
            if (target > grDb) grDb = aA * grDb + (1 - aA) * target;
            else { const double ar = coef (trMs * (1 + pd * slow / 12), fs); grDb = ar * grDb + (1 - ar) * target; }
            const double g = dbToLin (-grDb);
            double yl = l * g, yr = r * g;
            if (p.analog)
            {
                const double drive = v.thd * fetThd * (1 + grDb / 6) * (all ? 2.2 : 1);
                const double b = v.bias * fetBias * (1 + grDb / 10);
                yl = sat (yl * (1 + drive), outCeil, b) / (1 + drive);
                yr = sat (yr * (1 + drive), outCeil, b) / (1 + drive);
                yl = col[0].process (outX[0].process (yl)) + noise[0].next();
                yr = col[1].process (outX[1].process (yr)) + noise[1].next();
            }
            yl = dc[0].process (yl) * outG; yr = dc[1].process (yr) * outG;
            L[i] = (float) yl; if (numCh > 1) R[i] = (float) yr;
            pk = std::max (pk, (float) std::max (std::abs (yl), std::abs (yr)));
            grMax = std::max (grMax, (float) grDb);
        }
        outPeak = pk; grMeter = grMax;
    }
    float outPeak = 0, grMeter = 0;

private:
    void update()
    {
        using namespace circuit;
        const auto& v = var (p.variant);
        const double ta = 0.8 * std::pow (0.02 / 0.8, (p.attack - 1) / 6) * v.atkScale;     // ms: 800 µs → 20 µs
        const double tr = 1100 * std::pow (50.0 / 1100, (p.release - 1) / 6) * v.relScale;  // ms: 1100 → 50
        all = p.ratio == 4;
        static const double ratios[5] = { 4, 8, 12, 20, 20 };
        ratio = ratios[clampv (p.ratio, 0, 4)];
        knee = all ? 14 : v.knee * (p.ratio == 0 ? 1.4 : 1);
        thr = v.thr + (all ? -2 : 0);
        aA = coef (all ? ta * 0.6 : ta, fs);
        trMs = all ? tr * 1.3 : tr;
        inG = dbToLin (p.input); outG = dbToLin (p.output);
        slowC = coef (400, fs);
        const auto& F = fet76[p.fetIdx()]; const auto& X = in76[p.inIdx()]; const auto& O = out76[p.outIdx()];
        const double age = p.age / 100, iron = p.iron / 100, V = std::pow (p.headroom / 24, 1.5), ceilAge = 1 - 0.25 * age;
        fetThd = F.thd; fetBias = F.bias * (1 + p.bias / 100) * O.b;
        outCeil = 1.4 * O.ceil * V * ceilAge;
        pd = p.detector / 50;
        for (size_t c = 0; c < 2; ++c)
        {
            inX[c].set (&X.x, fs, iron, ceilAge); outX[c].set (O.hasX ? &O.x : nullptr, fs, iron, ceilAge);
            noise[c].set (noiseDb (O.floor, p.noise, age), p.analog && p.noise > 0);
        }
        const auto cc = designAnalog (p, fs); col[0].set (cc); col[1].set (cc);
    }
    Fet76Params p;
    double fs = 48000, env = 0, grDb = 0, slow = 0, ratio = 4, knee = 6, thr = -18, aA = 0, trMs = 100, inG = 1, outG = 1, slowC = 0;
    double fetThd = 1, fetBias = 1, outCeil = 1.4, pd = 1;
    bool all = false;
    DCBlock dc[2]; Cascade<8> col[2]; Xfmr inX[2], outX[2]; Noise noise[2];
};

// ================================================================= Opto leveler (solid-state)
class Opto3A
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; fast = slowS = mem = 0;
        for (size_t c = 0; c < 2; ++c) { sc[c].reset(); dc[c].prepare (fs); col[c].reset(); outX[c].reset(); noise[c].prepare (fs, c ? 35344u : 34567u); }
        update();
    }
    void setParams (const Opto3AParams& np) { if (np != p) { p = np; update(); } }
    double gain (double xDb) const
    {
        const double o = xDb - thr;
        if (2 * o < -knee) return 0;
        if (2 * std::abs (o) <= knee) return (1 - 1 / ratio) * (o + knee / 2) * (o + knee / 2) / (2 * knee);
        return (1 - 1 / ratio) * o;
    }
    double scDriveDb() const { return p.peak * 0.4; }

    void process (float* const* chans, int numCh, int n) noexcept
    {
        float* L = chans[0]; float* R = numCh > 1 ? chans[1] : chans[0];
        float pk = 0, grMax = 0;
        for (int i = 0; i < n; ++i)
        {
            const double sl = sc[0].process (L[i] * scG), sr = sc[1].process (R[i] * scG);
            const double t = gain (linToDb (std::max (std::abs (sl), std::abs (sr))));
            const double atk = coef (clampv (1.5 * 6 / (6 + std::max (t - fast, 0.0)) * 2, 0.4, 3.0) * cellAtk, fs);
            const double memTarget = clampv (slowS / 12, 0.0, 1.0);
            mem = memTarget > mem ? memA * mem + (1 - memA) * memTarget : memR * mem + (1 - memR) * memTarget;
            const double relSlow = coef ((170 + 1500 * mem * memScale) * cellSlow, fs);
            fast  = t > fast  ? atk * fast  + (1 - atk) * t : relFast * fast + (1 - relFast) * t;
            slowS = t > slowS ? atk * slowS + (1 - atk) * t : relSlow * slowS + (1 - relSlow) * t;
            const double gr = 0.6 * fast + 0.4 * slowS;
            const double g = dbToLin (-gr) * outG;
            double yl = L[i] * g, yr = R[i] * g;
            if (p.analog)
            {
                yl = sat (yl, ampCeil, ampBias); yr = sat (yr, ampCeil, ampBias);
                yl = col[0].process (outX[0].process (yl)) + noise[0].next();
                yr = col[1].process (outX[1].process (yr)) + noise[1].next();
            }
            yl = dc[0].process (yl); yr = dc[1].process (yr);
            L[i] = (float) yl; if (numCh > 1) R[i] = (float) yr;
            pk = std::max (pk, (float) std::max (std::abs (yl), std::abs (yr)));
            grMax = std::max (grMax, (float) gr);
        }
        outPeak = pk; grMeter = grMax;
    }
    float outPeak = 0, grMeter = 0;

private:
    void update()
    {
        using namespace circuit;
        const auto c = design3aSC (p, fs); for (auto& b : sc) b.c = c;
        scG = dbToLin (p.peak * 0.4);
        outG = dbToLin ((p.gain - 50) * 0.5);
        ratio = p.limit ? 12 : 3; knee = p.limit ? 6 : 14; thr = -12;
        const auto& C = cell3a[clampv (p.cell, 0, 2)]; const auto& A = amp3a[clampv (p.amp, 0, 1)]; const auto& O = out3a[clampv (p.outX, 0, 2)];
        cellAtk = C.atk; cellSlow = C.slow;
        relFast = coef (60 / std::log (6.0) * C.rel, fs);
        memA = coef (1500, fs); memR = coef (6000, fs);
        memScale = p.memory / 50;
        const double age = p.age / 100, iron = p.iron / 100, V = std::pow (p.headroom / 24, 1.5), ceilAge = 1 - 0.25 * age;
        ampCeil = A.ceil * V * ceilAge; ampBias = A.b;
        for (size_t k = 0; k < 2; ++k)
        {
            outX[k].set (&O.x, fs, iron, ceilAge);
            noise[k].set (noiseDb (A.floor, p.noise, age), p.analog && p.noise > 0);
        }
        const auto cc = designAnalog (p, fs); col[0].set (cc); col[1].set (cc);
    }
    Opto3AParams p;
    double fs = 48000, fast = 0, slowS = 0, mem = 0, scG = 1, outG = 1, ratio = 3, knee = 14, thr = -12, relFast = 0, memA = 0, memR = 0;
    double cellAtk = 1, cellSlow = 1, memScale = 1, ampCeil = 1.8, ampBias = 0.05;
    Biquad sc[2]; DCBlock dc[2]; Cascade<8> col[2]; Xfmr outX[2]; Noise noise[2];
};

// ================================================================= harmonic analysis (CIRCUIT panel)
// Sine through a fresh copy of the module (noise off); H2..H7 in dB re the fundamental, THD in %.
struct Harmonics { std::array<double, 6> h {}; double thd = 0, gr = 0; };
template <class Module, class Params>
inline Harmonics measureHarmonics (Params p, double fs, double f0, double levelDb)
{
    p.noise = 0;
    Module m; m.prepare (fs); m.setParams (p);
    const int n = (int) std::lround (fs * 0.3), N = (int) std::lround (fs * 0.1);
    const double A = dbToLin (levelDb);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = (float) (A * std::sin (2 * kPi * f0 * i / fs));
    for (int i = 0; i < n; i += 256) { float* ch[2] = { L.data() + i, R.data() + i }; m.process (ch, 2, std::min (256, n - i)); }
    auto mag = [&] (double f)
    {
        double re = 0, im = 0;
        for (int i = 0; i < N; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * kPi * i / (N - 1)), x = L[(size_t) (n - N + i)] * w, ph = 2 * kPi * f * i / fs;
            re += x * std::cos (ph); im += x * std::sin (ph);
        }
        return std::hypot (re, im);
    };
    Harmonics out; const double h1 = std::max (mag (f0), 1e-12); double sum = 0;
    for (int k = 2; k <= 7; ++k)
    {
        const double v = f0 * k < fs * 0.45 ? mag (f0 * k) : 0.0;
        sum += v * v; out.h[(size_t) (k - 2)] = 20 * std::log10 (std::max (v / h1, 1e-7));
    }
    out.thd = std::sqrt (sum) / h1 * 100; out.gr = m.grMeter;
    return out;
}

} // namespace vr
