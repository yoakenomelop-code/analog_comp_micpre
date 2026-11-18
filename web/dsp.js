// ============================================================================
// VintageRack DSP core (JavaScript)  — Pre/EQ, FET Comp, Opto Leveler
// Shared by the AudioWorklet (audio) and the main thread (response curves).
// The C++ (JUCE) version in /juce/Source/DSP.h is a line-by-line port of this.
// ============================================================================
var VR = (function () {
  'use strict';
  const PI = Math.PI;
  const dbToLin = (d) => Math.pow(10, d / 20);
  const linToDb = (l) => 20 * Math.log10(Math.max(l, 1e-9));
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));

  // ---------------------------------------------------------------- specs
  const SPEC = {
    pre73: {
      variants: ['classic', 'plus'],
      labels: ['Classic', 'Classic+ (Hi-Q / LPF)'],
      hpf:  { 'classic': [0, 50, 80, 160, 300], 'plus': [0, 50, 80, 160, 300] },
      low:  [35, 60, 110, 220],
      mid:  [360, 700, 1600, 3200, 4800, 7200],
      high: { 'classic': [12000], 'plus': [10000, 12000, 16000] },
      lpf:  { 'classic': [0], 'plus': [0, 18000, 10000, 8000, 6000] },
      gainMic: [20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80],
      gainLine: [-10, -5, 0, 5, 10, 15, 20],
    },
    fet76: {
      variants: ['Early (Blue)', 'Low-noise (Black)', 'Late (Silver)'],
      ratios: ['4', '8', '12', '20', 'ALL'],
    },
    opto3a: { variants: ['Opto'] },
  };

  // ---------------------------------------------------------------- circuit components ("CIRCUIT" panel)
  // Transformer: fc/lf = how strongly low frequencies drive the core, d = core drive, ceil/b = clip & asymmetry,
  // hp/bump/hs = small-signal frequency fingerprint. Modeled tendencies, not measurements.
  const CIRCUIT = {
    pre73: {
      inX: [
        { name: 'Vintage (nickel core)', fc: 90, lf: 1.0, d: 0.7, ceil: 1.8, b: 0.10, hp: 12, bumpF: 60, bumpG: 0.9, hsF: 17000, hsG: -0.8 },
        { name: 'Mid-era', fc: 90, lf: 0.75, d: 0.57, ceil: 2.1, b: 0.08, hp: 10, bumpF: 60, bumpG: 0.7, hsF: 18000, hsG: -0.6 },
        { name: 'Modern reissue', fc: 100, lf: 0.6, d: 0.48, ceil: 2.4, b: 0.06, hp: 10, bumpF: 70, bumpG: 0.5, hsF: 19000, hsG: -0.4 },
        { name: 'Transformerless', fc: 100, lf: 0, d: 0.001, ceil: 3, b: 0, hp: 5, bumpF: 60, bumpG: 0, hsF: 20000, hsG: 0 },
      ],
      amp: [
        { name: 'Vintage NOS transistors', b: 0.14, ceil: 1.0, floor: -92 },
        { name: 'Current transistors', b: 0.10, ceil: 1.1, floor: -96 },
        { name: 'Modern low-noise', b: 0.05, ceil: 1.3, floor: -104 },
      ],
      outX: [
        { name: 'Vintage output', fc: 120, lf: 0.5, d: 0.53, ceil: 2.7, b: 0.04, hsF: 16000, hsG: -0.5 },
        { name: 'Modern reissue', fc: 120, lf: 0.4, d: 0.44, ceil: 3.3, b: 0.03, hsF: 18000, hsG: -0.3 },
        { name: 'Transformerless', fc: 120, lf: 0, d: 0.001, ceil: 4, b: 0, hsF: 20000, hsG: 0 },
      ],
    },
    fet76: {
      // index 0 = "revision standard": resolved from the selected revision (STD76)
      fet: [{ name: 'Model standard' },
        { name: 'Early FET (original)', thd: 1.0, bias: 1.0 }, { name: 'Later FET', thd: 0.7, bias: 0.6 },
        { name: 'NOS unselected (rough)', thd: 1.8, bias: 1.6 }, { name: 'Selected low-distortion', thd: 0.4, bias: 0.4 }],
      inX: [{ name: 'Model standard' },
        { name: 'Nickel core (vintage)', fc: 80, lf: 1.0, d: 0.62, ceil: 1.95, b: 0.08, hp: 15, lsF: 45, lsG: 0.6, pkF: 3000, pkG: 0, hsF: 11000, hsG: -1.2 },
        { name: 'Steel core', fc: 90, lf: 0.7, d: 0.48, ceil: 2.4, b: 0.05, hp: 12, lsF: 45, lsG: 0, pkF: 3000, pkG: 0.3, hsF: 15000, hsG: -0.6 },
        { name: 'Transformerless', fc: 90, lf: 0, d: 0.001, ceil: 3, b: 0, hp: 6, lsF: 45, lsG: 0, pkF: 3000, pkG: 0, hsF: 20000, hsG: 0 }],
      out: [{ name: 'Model standard' },
        { name: 'Class-A + transformer', ceil: 1.0, b: 1.0, floor: -88, x: { fc: 120, lf: 0.5, d: 0.44, ceil: 3.0, b: 0.04 }, pkF: 5000, pkG: 0, hsF: 16000, hsG: -0.3, lsG: 0 },
        { name: 'Class-A transformerless', ceil: 1.15, b: 0.8, floor: -92, x: null, pkF: 5000, pkG: 0.6, hsF: 16000, hsG: 0, lsG: 0 },
        { name: 'Hot-rodded (high gain)', ceil: 0.8, b: 1.6, floor: -84, x: { fc: 120, lf: 0.65, d: 0.57, ceil: 2.7, b: 0.06 }, pkF: 5000, pkG: 0, hsF: 14000, hsG: -0.8, lsG: 0.4 }],
    },
    opto3a: {
      cell: [
        { name: 'Opto cell (new)', atk: 1.0, rel: 1.0, slow: 1.0 },
        { name: 'Opto cell (aged)', atk: 1.3, rel: 1.4, slow: 1.8 },
        { name: 'Fast cell (modified)', atk: 0.6, rel: 0.6, slow: 0.6 },
      ],
      amp: [
        { name: 'Discrete Class-A (original)', ceil: 1.8, b: 0.05, pkG: 0.8, floor: -94 },
        { name: 'Later IC amp', ceil: 2.4, b: 0.02, pkG: 0.3, floor: -100 },
      ],
      outX: [
        { name: 'Original transformer', fc: 110, lf: 0.9, d: 1.1, ceil: 3.0, b: 0.04, hp: 18, lsG: -0.4, hsF: 14000, hsG: -0.8 },
        { name: 'Current transformer', fc: 110, lf: 0.7, d: 0.85, ceil: 3.6, b: 0.03, hp: 15, lsG: -0.2, hsF: 16000, hsG: -0.5 },
        { name: 'Transformerless', fc: 110, lf: 0, d: 0.001, ceil: 4, b: 0, hp: 5, lsG: 0, hsF: 20000, hsG: 0 },
      ],
    },
  };
  // components each FET-comp model ships with: [fet, inX, out]
  const STD76 = [[1, 1, 1], [1, 2, 1], [2, 2, 2]];

  // Continuous ("無段階") ranges — beyond the hardware, used when cont = 1
  const CONT = {
    hpf: [10, 600], low: [20, 500], mid: [150, 10000], high: [3000, 20000], lpf: [2000, 22000], sc: [500, 10000],
    hpfOff: 16,     // hpfHz below this = OFF
    lpfOff: 21000,  // lpfHz at/above this = OFF
  };

  const DEFAULTS = {
    pre73: { variant: 0, mic: 0, gain: 0, hpf: 0, lowFreq: 1, lowGain: 0, midFreq: 2, midGain: 0,
             hiQ: 0, highFreq: 1, highGain: 0, lpf: 0, eqIn: 1, phase: 0, output: 0,
             analog: 1, cont: 0, hpfHz: 80, lowHz: 60, midHz: 1600, highHz: 12000, lpfHz: 22000,
             inX: 0, amp: 0, outX: 0, bias: 0, headroom: 24, iron: 100, age: 0, noise: 0 },
    fet76: { variant: 1, input: 6, output: 0, attack: 3, release: 5, ratio: 0, analog: 1,
             fet: 0, inX: 0, out: 0, bias: 0, headroom: 24, detector: 50, iron: 100, age: 0, noise: 0 },
    opto3a: { peak: 40, gain: 50, limit: 0, hf: 0, analog: 1, cont: 0, scHz: 2500,
              cell: 0, amp: 0, outX: 0, memory: 50, headroom: 24, iron: 100, age: 0, noise: 0 },
  };

  // ---------------------------------------------------------------- biquad
  // RBJ cookbook. Coefficients normalised (a0 = 1).
  function bqIdentity() { return { b0: 1, b1: 0, b2: 0, a1: 0, a2: 0 }; }
  function bqPeak(fs, f, q, gDb) {
    const A = Math.pow(10, gDb / 40), w = 2 * PI * f / fs, al = Math.sin(w) / (2 * q), c = Math.cos(w);
    const a0 = 1 + al / A;
    return { b0: (1 + al * A) / a0, b1: -2 * c / a0, b2: (1 - al * A) / a0, a1: -2 * c / a0, a2: (1 - al / A) / a0 };
  }
  function bqLowShelf(fs, f, q, gDb) {
    const A = Math.pow(10, gDb / 40), w = 2 * PI * f / fs, al = Math.sin(w) / (2 * q), c = Math.cos(w), s = 2 * Math.sqrt(A) * al;
    const a0 = (A + 1) + (A - 1) * c + s;
    return { b0: A * ((A + 1) - (A - 1) * c + s) / a0, b1: 2 * A * ((A - 1) - (A + 1) * c) / a0,
             b2: A * ((A + 1) - (A - 1) * c - s) / a0, a1: -2 * ((A - 1) + (A + 1) * c) / a0, a2: ((A + 1) + (A - 1) * c - s) / a0 };
  }
  function bqHighShelf(fs, f, q, gDb) {
    const A = Math.pow(10, gDb / 40), w = 2 * PI * f / fs, al = Math.sin(w) / (2 * q), c = Math.cos(w), s = 2 * Math.sqrt(A) * al;
    const a0 = (A + 1) - (A - 1) * c + s;
    return { b0: A * ((A + 1) + (A - 1) * c + s) / a0, b1: -2 * A * ((A - 1) + (A + 1) * c) / a0,
             b2: A * ((A + 1) + (A - 1) * c - s) / a0, a1: 2 * ((A - 1) - (A + 1) * c) / a0, a2: ((A + 1) - (A - 1) * c - s) / a0 };
  }
  function bqHP(fs, f, q) {
    const w = 2 * PI * f / fs, al = Math.sin(w) / (2 * q), c = Math.cos(w), a0 = 1 + al;
    return { b0: (1 + c) / 2 / a0, b1: -(1 + c) / a0, b2: (1 + c) / 2 / a0, a1: -2 * c / a0, a2: (1 - al) / a0 };
  }
  function bqLP(fs, f, q) {
    const w = 2 * PI * f / fs, al = Math.sin(w) / (2 * q), c = Math.cos(w), a0 = 1 + al;
    return { b0: (1 - c) / 2 / a0, b1: (1 - c) / a0, b2: (1 - c) / 2 / a0, a1: -2 * c / a0, a2: (1 - al) / a0 };
  }
  // first-order sections expressed as biquads (b2 = a2 = 0), bilinear transform
  function fo_HP(fs, f) {
    const k = Math.tan(PI * f / fs), a0 = k + 1;
    return { b0: 1 / a0, b1: -1 / a0, b2: 0, a1: (k - 1) / a0, a2: 0 };
  }
  function fo_LP(fs, f) {
    const k = Math.tan(PI * f / fs), a0 = k + 1;
    return { b0: k / a0, b1: k / a0, b2: 0, a1: (k - 1) / a0, a2: 0 };
  }
  // |H(e^jw)| in dB
  function bqMagDb(c, fs, f) {
    const w = 2 * PI * f / fs;
    const cr = Math.cos(w), ci = -Math.sin(w), c2r = Math.cos(2 * w), c2i = -Math.sin(2 * w);
    const nr = c.b0 + c.b1 * cr + c.b2 * c2r, ni = c.b1 * ci + c.b2 * c2i;
    const dr = 1 + c.a1 * cr + c.a2 * c2r, di = c.a1 * ci + c.a2 * c2i;
    return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
  }
  class Biquad {
    constructor() { this.c = bqIdentity(); this.z1 = 0; this.z2 = 0; }
    set(c) { this.c = c; }
    reset() { this.z1 = this.z2 = 0; }
    process(x) { // transposed direct form II
      const c = this.c, y = c.b0 * x + this.z1;
      this.z1 = c.b1 * x - c.a1 * y + this.z2;
      this.z2 = c.b2 * x - c.a2 * y;
      return y;
    }
  }

  // ---------------------------------------------------------------- Pre/EQ coefficient design
  // Returns the list of filter "bands" with coefficients; used by both audio and the graph.
  // Actual band frequencies in Hz (0 = off), stepped or continuous
  function freqs73(p) {
    const v = SPEC.pre73.variants[p.variant] || 'classic';
    if (p.cont) return {
      hpf: p.hpfHz < CONT.hpfOff ? 0 : p.hpfHz, low: p.lowHz, mid: p.midHz, high: p.highHz,
      lpf: v === 'plus' && p.lpfHz < CONT.lpfOff ? p.lpfHz : 0 };
    const hl = SPEC.pre73.high[v];
    return { hpf: SPEC.pre73.hpf[v][p.hpf | 0] || 0, low: SPEC.pre73.low[p.lowFreq | 0], mid: SPEC.pre73.mid[p.midFreq | 0],
             high: hl[Math.min(p.highFreq | 0, hl.length - 1)] || hl[0], lpf: SPEC.pre73.lpf[v][p.lpf | 0] || 0 };
  }
  // skew: component-tolerance factor applied to every band frequency (AGE, right channel)
  function design73(p, fs, skew = 1) {
    const v = SPEC.pre73.variants[p.variant] || 'classic';
    const bands = [], F0 = freqs73(p), F = {};
    for (const k in F0) F[k] = F0[k] * skew;
    const hpfF = F.hpf;
    if (hpfF > 0) bands.push({ id: 'hpf', f: hpfF, secs: [fo_HP(fs, hpfF), bqHP(fs, hpfF, 1.0)] });   // 18 dB/oct
    else bands.push({ id: 'hpf', f: 0, secs: [] });
    // Low shelf: slightly resonant (Q 0.9) → the gentle bump/dip of the inductor shelf
    const lf = F.low;
    bands.push({ id: 'low', f: lf, secs: Math.abs(p.lowGain) > 0.01 ? [bqLowShelf(fs, lf, 0.9, p.lowGain)] : [] });
    // Mid peak: broad, gain-proportional Q (inductor style). Classic+ Hi-Q → narrow.
    const mf = F.mid;
    let mg = p.midGain;
    if (v === 'plus' && !p.hiQ) mg = clamp(mg, -12, 12);
    const q = (v === 'plus' && p.hiQ) ? 2.0 + 0.05 * Math.abs(mg) : 0.55 + 0.035 * Math.abs(mg);
    bands.push({ id: 'mid', f: mf, secs: Math.abs(mg) > 0.01 ? [bqPeak(fs, mf, q, mg)] : [] });
    // High shelf (broad)
    const hf = F.high;
    bands.push({ id: 'high', f: hf, secs: Math.abs(p.highGain) > 0.01 ? [bqHighShelf(fs, Math.min(hf, fs * 0.45), 0.55, p.highGain)] : [] });
    const lpfF = F.lpf;
    if (lpfF > 0) bands.push({ id: 'lpf', f: lpfF, secs: [fo_LP(fs, Math.min(lpfF, fs * 0.45)), bqLP(fs, Math.min(lpfF, fs * 0.45), 1.0)] });
    else bands.push({ id: 'lpf', f: 0, secs: [] });
    return bands;
  }
  // Opto side-chain HF emphasis shelf
  function scFreq3a(p) { return p.cont ? p.scHz : 2500; }
  function design3aSC(p, fs) { return bqHighShelf(fs, Math.min(scFreq3a(p), fs * 0.45), 0.6, (p.hf / 100) * 10); }

  // ---------------------------------------------------------------- analog coloration (small-signal)
  // Frequency fingerprint of the selected transformers / amp stages when ANALOG is on (shown on the map).
  function resolve76(p) {
    const std = STD76[p.variant | 0] || STD76[1], C = CIRCUIT.fet76;
    return { fet: C.fet[(p.fet | 0) || std[0]], inX: C.inX[(p.inX | 0) || std[1]], out: C.out[(p.out | 0) || std[2]] };
  }
  function designAnalog(type, p, fs) {
    if (!p.analog) return [];
    const hs = (f, q, g) => bqHighShelf(fs, Math.min(f, fs * 0.45), q, g);
    const age = (p.age || 0) / 100, s = [];
    if (type === 'pre73') {
      const C = CIRCUIT.pre73, X = C.inX[p.inX | 0] || C.inX[0], O = C.outX[p.outX | 0] || C.outX[0];
      s.push(fo_HP(fs, X.hp));
      if (X.bumpG) s.push(bqPeak(fs, X.bumpF, 0.8, X.bumpG));
      if (X.hsG) s.push(hs(X.hsF, 0.5, X.hsG));
      if (O.hsG) s.push(hs(O.hsF, 0.5, O.hsG));
    } else if (type === 'fet76') {
      const r = resolve76(p), X = r.inX, O = r.out;
      s.push(fo_HP(fs, X.hp));
      if (X.lsG) s.push(bqLowShelf(fs, X.lsF, 0.7, X.lsG));
      if (X.pkG) s.push(bqPeak(fs, X.pkF, 0.6, X.pkG));
      if (X.hsG) s.push(hs(X.hsF, 0.5, X.hsG));
      if (O.lsG) s.push(bqLowShelf(fs, 100, 0.7, O.lsG));
      if (O.pkG) s.push(bqPeak(fs, O.pkF, 0.6, O.pkG));
      if (O.hsG) s.push(hs(O.hsF, 0.5, O.hsG));
    } else if (type === 'opto3a') {
      const C = CIRCUIT.opto3a, A = C.amp[p.amp | 0] || C.amp[0], O = C.outX[p.outX | 0] || C.outX[0];
      s.push(fo_HP(fs, O.hp));
      if (O.lsG) s.push(bqLowShelf(fs, 80, 0.7, O.lsG));
      if (A.pkG) s.push(bqPeak(fs, 2200, 0.7, A.pkG));
      if (O.hsG) s.push(hs(O.hsF, 0.5, O.hsG));
    }
    if (age > 0) s.push(hs(9000, 0.6, -1.5 * age));          // aged caps / tired iron: duller top
    return s;
  }
  function responseAnalog(type, p, fs, freqs) { const secs = designAnalog(type, p, fs); return freqs.map((f) => secs.reduce((s, c) => s + bqMagDb(c, fs, f), 0)); }

  // response curves for the UI: { freqs, bands: {id: dB[]}, total: dB[] }
  function response73(p, fs, freqs) {
    const bands = design73(p, fs), out = { bands: {}, total: new Array(freqs.length).fill(0) };
    for (const b of bands) {
      const arr = freqs.map((f) => b.secs.reduce((s, c) => s + bqMagDb(c, fs, f), 0));
      out.bands[b.id] = arr;
      if (p.eqIn || b.id === 'hpf' || b.id === 'lpf') arr.forEach((d, i) => (out.total[i] += d));
    }
    return out;
  }
  function response3aSC(p, fs, freqs) { const c = design3aSC(p, fs); return freqs.map((f) => bqMagDb(c, fs, f)); }

  // ---------------------------------------------------------------- small helpers
  class DCBlock { constructor(fs) { this.R = 1 - 2 * PI * 5 / fs; this.x1 = 0; this.y1 = 0; }
    process(x) { const y = x - this.x1 + this.R * this.y1; this.x1 = x; this.y1 = y; return y; } }
  // Soft saturator with an even-harmonic bias. Transparent at low level, rounding near ceiling.
  function sat(x, ceil, bias) {
    const u = x / ceil;
    return ceil * (Math.tanh(u + bias) - Math.tanh(bias)) / (1 - Math.tanh(bias) * Math.tanh(bias));
  }
  const coef = (ms, fs) => Math.exp(-1 / (Math.max(ms, 0.001) * 0.001 * fs));
  // per-channel cascades of biquads (keeps filter state when coefficients change)
  function setSecs(chs, coefs) {
    for (const arr of chs) { while (arr.length < coefs.length) arr.push(new Biquad()); arr.length = coefs.length; coefs.forEach((c, i) => arr[i].set(c)); }
  }
  function runSecs(arr, x) { for (let k = 0; k < arr.length; k++) x = arr[k].process(x); return x; }

  // Transformer core: low frequencies drive the core harder (lf), so saturation is LF-weighted.
  // Unity gain at small signal; distortion grows with level, IRON (d) and lower ceiling.
  class Xfmr {
    constructor() { this.l = 0; this.on = false; this.a = 0; this.lf = 0; this.d = 1; this.ceil = 1; this.b = 0; }
    set(spec, fs, iron, ceilScale) {
      this.on = !!spec && spec.d > 0.01 && iron > 0;
      if (!this.on) return;
      this.a = 1 - Math.exp(-2 * PI * spec.fc / fs); this.lf = spec.lf;
      this.d = Math.max(spec.d * Math.pow(iron, 2), 1e-3);   // IRON: 0..200 %, progressive
      this.ceil = spec.ceil * ceilScale; this.b = spec.b;
    }
    process(x) {
      if (!this.on) return x;
      this.l += this.a * (x - this.l);
      const u = x + this.lf * this.l;
      return x + (sat(u * this.d, this.ceil, this.b) / this.d - u);
    }
  }
  // Hiss (soft white noise) + 50 Hz mains hum with odd harmonics. Deterministic (same in C++).
  class Noise {
    constructor(fs, seed) { this.s = seed >>> 0; this.n = 0; this.ph = 0; this.a = 1 - Math.exp(-2 * PI * 6000 / fs); this.dph = 2 * PI * 50 / fs; this.hiss = 0; this.hum = 0; }
    set(levelDb, on) {
      if (!on) { this.hiss = this.hum = 0; return; }
      const norm = 0.57735 * Math.sqrt(this.a / (2 - this.a));
      this.hiss = dbToLin(levelDb) / norm; this.hum = dbToLin(levelDb - 10);
    }
    next() {
      if (this.hiss === 0) return 0;
      this.s = (Math.imul(this.s, 1664525) + 1013904223) >>> 0;
      const w = (this.s >>> 8) / 16777216 * 2 - 1;
      this.n += this.a * (w - this.n);
      this.ph += this.dph; if (this.ph > 2 * PI) this.ph -= 2 * PI;
      return this.n * this.hiss + this.hum * (Math.sin(this.ph) + 0.5 * Math.sin(3 * this.ph) + 0.25 * Math.sin(5 * this.ph));
    }
  }
  const noiseDb = (floor, noise, age) => floor + (noise - 50) * 0.4 + 6 * age;
  const num = (v, d) => (v === undefined || v === null ? d : +v);

  // ================================================================ Pre / EQ
  class Pre73 {
    constructor(fs) {
      this.fs = fs; this.p = Object.assign({}, DEFAULTS.pre73); this.out = 0;
      this.ch = [0, 1].map((c) => ({ secs: [], dc: new DCBlock(fs), inX: new Xfmr(), outX: new Xfmr(), noise: new Noise(fs, 12345 + 777 * c) }));
      this.update();
    }
    setParams(o) { Object.assign(this.p, o); this.update(); }
    update() {
      const p = this.p, fs = this.fs, C = CIRCUIT.pre73;
      const X = C.inX[p.inX | 0] || C.inX[0], A = C.amp[p.amp | 0] || C.amp[0], O = C.outX[p.outX | 0] || C.outX[0];
      const age = num(p.age, 0) / 100, iron = num(p.iron, 100) / 100, V = Math.pow(num(p.headroom, 24) / 24, 1.5), ceilAge = 1 - 0.25 * age;
      this.analog = !!p.analog;
      const ana = designAnalog('pre73', p, fs);
      this.ch.forEach((ch, c) => {
        const bands = design73(p, fs, c ? 1 + 0.03 * age : 1);   // AGE: right channel drifts (component tolerance)
        let coefs = ana.slice();
        for (const b of bands) if (p.eqIn || b.id === 'hpf' || b.id === 'lpf') coefs = coefs.concat(b.secs);
        while (ch.secs.length < coefs.length) ch.secs.push(new Biquad());
        ch.secs.length = coefs.length; coefs.forEach((k, i) => ch.secs[i].set(k));
        ch.inX.set(X, fs, iron, ceilAge); ch.outX.set(O, fs, iron, ceilAge);
        ch.noise.set(noiseDb(A.floor, num(p.noise, 0), age), this.analog && num(p.noise, 0) > 0);
      });
      this.ampCeil = 1.6 * A.ceil * V * ceilAge; this.outCeil = 2.2 * A.ceil * V * ceilAge;
      this.ampBias = A.b + num(p.bias, 0) / 100 * 0.25;
      const g = p.mic ? p.gain - 50 : p.gain;           // mic: assume mic-level source (−50 dB re line)
      this.inG = dbToLin(g) * (p.phase ? -1 : 1);
      this.outG = dbToLin(p.output);
    }
    process(L, R, n) {
      let peak = 0;
      for (let c = 0; c < 2; c++) {
        const buf = c ? R : L, ch = this.ch[c], secs = ch.secs, ns = secs.length;
        for (let i = 0; i < n; i++) {
          let x = buf[i] * this.inG;
          if (this.analog) { x = ch.inX.process(x); x = sat(x, this.ampCeil, this.ampBias); }    // input transformer → Class-A amp
          for (let k = 0; k < ns; k++) x = secs[k].process(x);
          if (this.analog) { x = sat(x, this.outCeil, this.ampBias * 0.5); x = ch.outX.process(x); x += ch.noise.next(); } // output amp → transformer
          x = ch.dc.process(x) * this.outG;
          buf[i] = x; const a = Math.abs(x); if (a > peak) peak = a;
        }
      }
      this.out = peak;
    }
    meter() { return { out: this.out }; }
  }

  // ================================================================ FET compressor
  const FET_VAR = [ // Early (Blue), Low-noise (Black), Late (Silver): detector character
    { atkScale: 1.25, relScale: 1.1, knee: 8, thd: 0.10, bias: 0.18, thr: -17 },
    { atkScale: 1.0,  relScale: 1.0, knee: 6, thd: 0.06, bias: 0.10, thr: -18 },
    { atkScale: 0.9,  relScale: 0.9, knee: 4, thd: 0.035, bias: 0.06, thr: -19 },
  ];
  class Fet76 {
    constructor(fs) {
      this.fs = fs; this.p = Object.assign({}, DEFAULTS.fet76); this.env = 0; this.grDb = 0; this.slow = 0; this.grMeter = 0; this.out = 0;
      this.dc = [new DCBlock(fs), new DCBlock(fs)]; this.col = [[], []];
      this.inX = [new Xfmr(), new Xfmr()]; this.outX = [new Xfmr(), new Xfmr()]; this.noise = [new Noise(fs, 23456), new Noise(fs, 24233)];
      this.update();
    }
    setParams(o) { Object.assign(this.p, o); this.update(); }
    update() {
      const p = this.p, v = FET_VAR[p.variant | 0] || FET_VAR[1], fs = this.fs;
      const ta = 0.8 * Math.pow(0.02 / 0.8, (p.attack - 1) / 6) * v.atkScale;      // 800 µs → 20 µs (ms)
      const tr = 1100 * Math.pow(50 / 1100, (p.release - 1) / 6) * v.relScale;    // 1100 ms → 50 ms
      const all = (p.ratio | 0) === 4;
      this.ratio = [4, 8, 12, 20, 20][p.ratio | 0];
      this.knee = all ? 14 : v.knee * (p.ratio === 0 ? 1.4 : 1);
      this.thr = v.thr + (all ? -2 : 0);
      this.aA = coef(all ? ta * 0.6 : ta, fs);
      this.trMs = all ? tr * 1.3 : tr;
      this.all = all; this.v = v;
      this.inG = dbToLin(p.input); this.outG = dbToLin(p.output);
      this.slowC = coef(400, fs);
      // circuit
      const r = resolve76(p), age = num(p.age, 0) / 100, iron = num(p.iron, 100) / 100, V = Math.pow(num(p.headroom, 24) / 24, 1.5), ceilAge = 1 - 0.25 * age;
      this.fetThd = r.fet.thd; this.fetBias = r.fet.bias * (1 + num(p.bias, 0) / 100) * r.out.b;
      this.outCeil = 1.4 * r.out.ceil * V * ceilAge;
      this.pd = num(p.detector, 50) / 50;
      this.analog = !!p.analog;
      for (let c = 0; c < 2; c++) {
        this.inX[c].set(r.inX, fs, iron, ceilAge); this.outX[c].set(r.out.x, fs, iron, ceilAge);
        this.noise[c].set(noiseDb(r.out.floor, num(p.noise, 0), age), this.analog && num(p.noise, 0) > 0);
      }
      setSecs(this.col, designAnalog('fet76', p, fs));
    }
    gain(xDb) { // static curve (dB of reduction, positive)
      const T = this.thr, W = this.knee, R = this.ratio, o = xDb - T;
      let gr;
      if (2 * o < -W) gr = 0;
      else if (2 * Math.abs(o) <= W) gr = (1 - 1 / R) * (o + W / 2) * (o + W / 2) / (2 * W);
      else gr = (1 - 1 / R) * o;
      if (this.all && o > 0) gr += 0.35 * o;              // "all buttons": over-compression (negative slope)
      return gr;
    }
    process(L, R, n) {
      const v = this.v; let peak = 0, grMax = 0;
      for (let i = 0; i < n; i++) {
        let l = L[i] * this.inG, r = R[i] * this.inG;
        if (this.analog) { l = this.inX[0].process(l); r = this.inX[1].process(r); }   // input transformer
        const lvl = linToDb(Math.max(Math.abs(l), Math.abs(r)));
        const target = this.gain(lvl);
        // program-dependent release (DETECTOR): longer after sustained heavy reduction
        this.slow = this.slowC * this.slow + (1 - this.slowC) * this.grDb;
        if (target > this.grDb) this.grDb = this.aA * this.grDb + (1 - this.aA) * target;
        else { const ar = coef(this.trMs * (1 + this.pd * this.slow / 12), this.fs); this.grDb = ar * this.grDb + (1 - ar) * target; }
        const g = dbToLin(-this.grDb);
        let yl = l * g, yr = r * g;
        if (this.analog) {
          // FET gain cell + Class-A line amp: harmonics grow with gain reduction
          const drive = v.thd * this.fetThd * (1 + this.grDb / 6) * (this.all ? 2.2 : 1);
          const b = v.bias * this.fetBias * (1 + this.grDb / 10);
          yl = sat(yl * (1 + drive), this.outCeil, b) / (1 + drive);
          yr = sat(yr * (1 + drive), this.outCeil, b) / (1 + drive);
          yl = runSecs(this.col[0], this.outX[0].process(yl)) + this.noise[0].next();
          yr = runSecs(this.col[1], this.outX[1].process(yr)) + this.noise[1].next();
        }
        yl = this.dc[0].process(yl) * this.outG; yr = this.dc[1].process(yr) * this.outG;
        L[i] = yl; R[i] = yr;
        const a = Math.max(Math.abs(yl), Math.abs(yr)); if (a > peak) peak = a;
        if (this.grDb > grMax) grMax = this.grDb;
      }
      this.grMeter = grMax; this.out = peak;
    }
    meter() { return { gr: this.grMeter, out: this.out }; }
  }

  // ================================================================ Opto leveler
  // Opto cell: attack ≤1.5 ms (faster on big transients), release 60 ms for 50 %,
  // remainder 0.5–5 s depending on how long/hard it has been compressing (MEMORY, cell type).
  class Opto3A {
    constructor(fs) {
      this.fs = fs; this.p = Object.assign({}, DEFAULTS.opto3a); this.fast = 0; this.slowS = 0; this.mem = 0; this.grMeter = 0; this.out = 0;
      this.sc = [new Biquad(), new Biquad()]; this.dc = [new DCBlock(fs), new DCBlock(fs)]; this.col = [[], []];
      this.outX = [new Xfmr(), new Xfmr()]; this.noise = [new Noise(fs, 34567), new Noise(fs, 35344)];
      this.update();
    }
    setParams(o) { Object.assign(this.p, o); this.update(); }
    update() {
      const p = this.p, fs = this.fs, C = CIRCUIT.opto3a;
      const c = design3aSC(p, fs); this.sc.forEach((b) => b.set(c));
      this.scG = dbToLin(p.peak * 0.4);                     // Peak Reduction: 0..40 dB of side-chain drive
      this.outG = dbToLin((p.gain - 50) * 0.5);             // Gain: −25..+25 dB
      this.ratio = p.limit ? 12 : 3; this.knee = p.limit ? 6 : 14; this.thr = -12;
      const cell = C.cell[p.cell | 0] || C.cell[0], A = C.amp[p.amp | 0] || C.amp[0], O = C.outX[p.outX | 0] || C.outX[0];
      this.cell = cell;
      this.relFast = coef(60 / Math.log(6) * cell.rel, fs);  // fast stage (60 %) → total 50 % in ≈60 ms
      this.memA = coef(1500, fs); this.memR = coef(6000, fs);
      this.memScale = num(p.memory, 50) / 50;
      const age = num(p.age, 0) / 100, iron = num(p.iron, 100) / 100, V = Math.pow(num(p.headroom, 24) / 24, 1.5), ceilAge = 1 - 0.25 * age;
      this.ampCeil = A.ceil * V * ceilAge; this.ampBias = A.b;
      this.analog = !!p.analog;
      for (let k = 0; k < 2; k++) {
        this.outX[k].set(O, fs, iron, ceilAge);
        this.noise[k].set(noiseDb(A.floor, num(p.noise, 0), age), this.analog && num(p.noise, 0) > 0);
      }
      setSecs(this.col, designAnalog('opto3a', p, fs));
    }
    gain(xDb) {
      const T = this.thr, W = this.knee, R = this.ratio, o = xDb - T;
      if (2 * o < -W) return 0;
      if (2 * Math.abs(o) <= W) return (1 - 1 / R) * (o + W / 2) * (o + W / 2) / (2 * W);
      return (1 - 1 / R) * o;
    }
    process(L, R, n) {
      let peak = 0, grMax = 0; const fs = this.fs, cell = this.cell;
      for (let i = 0; i < n; i++) {
        const sl = this.sc[0].process(L[i] * this.scG), sr = this.sc[1].process(R[i] * this.scG);
        const lvl = linToDb(Math.max(Math.abs(sl), Math.abs(sr)));
        const t = this.gain(lvl);
        // level-dependent attack: ~3 ms for small overs, ~0.4 ms for big ones (scaled by the cell)
        const atk = coef(clamp(1.5 * 6 / (6 + Math.max(t - this.fast, 0)) * 2, 0.4, 3) * cell.atk, fs);
        const memTarget = clamp(this.slowS / 12, 0, 1);
        this.mem = memTarget > this.mem ? this.memA * this.mem + (1 - this.memA) * memTarget : this.memR * this.mem + (1 - this.memR) * memTarget;
        const relSlow = coef((170 + 1500 * this.mem * this.memScale) * cell.slow, fs);   // full release ≈0.5–5 s
        this.fast = t > this.fast ? atk * this.fast + (1 - atk) * t : this.relFast * this.fast + (1 - this.relFast) * t;
        this.slowS = t > this.slowS ? atk * this.slowS + (1 - atk) * t : relSlow * this.slowS + (1 - relSlow) * t;
        const gr = 0.6 * this.fast + 0.4 * this.slowS;
        const g = dbToLin(-gr) * this.outG;
        let yl = L[i] * g, yr = R[i] * g;
        if (this.analog) {
          yl = sat(yl, this.ampCeil, this.ampBias); yr = sat(yr, this.ampCeil, this.ampBias);   // solid-state Class-A output amp
          yl = runSecs(this.col[0], this.outX[0].process(yl)) + this.noise[0].next();
          yr = runSecs(this.col[1], this.outX[1].process(yr)) + this.noise[1].next();
        }
        yl = this.dc[0].process(yl); yr = this.dc[1].process(yr);
        L[i] = yl; R[i] = yr;
        const a = Math.max(Math.abs(yl), Math.abs(yr)); if (a > peak) peak = a;
        if (gr > grMax) grMax = gr;
      }
      this.grMeter = grMax; this.out = peak;
    }
    meter() { return { gr: this.grMeter, out: this.out }; }
  }

  // ================================================================ Rack: any number of modules, any order
  // slots: [{ uid, type: 'pre73'|'fet76'|'opto3a', on, p }]
  const MAKERS = { pre73: Pre73, fet76: Fet76, opto3a: Opto3A };
  class Rack {
    constructor(fs) { this.fs = fs; this.slots = []; this.byUid = new Map(); this.inPk = 0; this.outPk = 0; }
    setSlots(list) {
      const next = new Map();
      this.slots = list.map((s) => {
        let m = this.byUid.get(s.uid);
        if (!m || m.type !== s.type) m = { type: s.type, mod: new MAKERS[s.type](this.fs) };
        if (s.p) m.mod.setParams(s.p);
        next.set(s.uid, m);
        return { uid: s.uid, type: s.type, on: s.on !== false, m };
      });
      this.byUid = next;
    }
    setParams(uid, p) { const m = this.byUid.get(uid); if (m) m.mod.setParams(p); }
    setOn(uid, v) { const s = this.slots.find((x) => x.uid === uid); if (s) s.on = !!v; }
    process(L, R, n) {
      let a = 0; for (let i = 0; i < n; i++) a = Math.max(a, Math.abs(L[i]), Math.abs(R[i])); this.inPk = a;
      for (const s of this.slots) if (s.on) s.m.mod.process(L, R, n);
      a = 0; for (let i = 0; i < n; i++) a = Math.max(a, Math.abs(L[i]), Math.abs(R[i])); this.outPk = a;
    }
    meters() { const gr = {}; for (const s of this.slots) gr[s.uid] = s.m.mod.grMeter || 0; return { in: this.inPk, out: this.outPk, gr }; }
  }

  // ---------------------------------------------------------------- harmonic analysis (for the CIRCUIT panel)
  // Runs a sine through a fresh copy of the module (noise off) and measures H2..H7 relative to the fundamental.
  function harmonics(type, p, fs, f0, levelDb) {
    const m = new MAKERS[type](fs); m.setParams(Object.assign({}, p, { noise: 0 }));
    const n = Math.round(fs * 0.3), N = Math.round(fs * 0.1), A = dbToLin(levelDb);
    const L = new Float32Array(n), R = new Float32Array(n);
    for (let i = 0; i < n; i++) L[i] = R[i] = A * Math.sin(2 * PI * f0 * i / fs);
    for (let i = 0; i < n; i += 256) { const k = Math.min(256, n - i); m.process(L.subarray(i, i + k), R.subarray(i, i + k), k); }
    const mag = (f) => { let re = 0, im = 0; for (let i = 0; i < N; i++) { const w = 0.5 - 0.5 * Math.cos(2 * PI * i / (N - 1)), x = L[n - N + i] * w, ph = 2 * PI * f * i / fs; re += x * Math.cos(ph); im += x * Math.sin(ph); } return Math.hypot(re, im); };
    const h1 = mag(f0) || 1e-12, h = []; let sum = 0;
    for (let k = 2; k <= 7; k++) { const v = f0 * k < fs * 0.45 ? mag(f0 * k) : 0; sum += v * v; h.push(20 * Math.log10(Math.max(v / h1, 1e-7))); }
    return { h, thd: Math.sqrt(sum) / h1 * 100, gr: m.grMeter || 0 };
  }

  return { CIRCUIT, STD76, resolve76, harmonics, SPEC, DEFAULTS, CONT, Rack, Pre73, Fet76, Opto3A, response73, response3aSC, responseAnalog, design73, designAnalog, freqs73, scFreq3a, dbToLin, linToDb, bqMagDb };
})();
if (typeof module !== 'undefined') module.exports = VR;
