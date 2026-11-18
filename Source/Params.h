#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"

// Which build this is (set per CMake target):  7 = rack (any modules, any number), 1 = Pre/EQ only, 2 = FET only, 4 = Opto only
#ifndef VR_MODULES
 #define VR_MODULES 7
#endif

namespace vr
{
enum SlotType { kEmpty = 0, kT73 = 1, kT76 = 2, kT3A = 3 };
inline constexpr bool isRack() { return VR_MODULES == 7; }
inline constexpr int numSlots() { return isRack() ? 8 : 1; }
inline constexpr int fixedType() { return VR_MODULES == 1 ? kT73 : VR_MODULES == 2 ? kT76 : VR_MODULES == 4 ? kT3A : kEmpty; }
inline bool typeAllowed (int t) { return isRack() ? t != kEmpty : t == fixedType(); }
inline const char* typeTitle (int t) { static const char* n[] = { "EMPTY", "PRE / EQ", "FET COMP", "OPTO LEVELER" }; return n[juce::jlimit (0, 3, t)]; }

namespace id
{
    // per-slot parameter suffixes
    inline const char* type = "type"; inline const char* on = "on";
    // 73
    inline const char *p73Variant = "p73_variant", *p73Mic = "p73_mic", *p73Gain = "p73_gain", *p73Phase = "p73_phase",
        *p73Hpf = "p73_hpf", *p73LowF = "p73_lowfreq", *p73LowG = "p73_low", *p73MidF = "p73_midfreq", *p73MidG = "p73_mid",
        *p73HiQ = "p73_hiq", *p73HighF = "p73_highfreq", *p73HighG = "p73_high", *p73Lpf = "p73_lpf", *p73EqIn = "p73_eqin",
        *p73Out = "p73_output", *p73Analog = "p73_analog", *p73Cont = "p73_cont",
        *p73HpfHz = "p73_hpfhz", *p73LowHz = "p73_lowhz", *p73MidHz = "p73_midhz", *p73HighHz = "p73_highhz", *p73LpfHz = "p73_lpfhz";
    // 76
    inline const char *f76Variant = "f76_variant", *f76In = "f76_input", *f76Out = "f76_output", *f76Atk = "f76_attack",
        *f76Rel = "f76_release", *f76Ratio = "f76_ratio", *f76Analog = "f76_analog";
    // 3A
    inline const char *o3Peak = "o3_peak", *o3Gain = "o3_gain", *o3Limit = "o3_limit", *o3Hf = "o3_hf",
        *o3Analog = "o3_analog", *o3Cont = "o3_cont", *o3ScHz = "o3_schz";

    // circuit ("CIRCUIT" panel)
    inline const char *c73InX = "c73_inx", *c73Amp = "c73_amp", *c73OutX = "c73_outx", *c73Bias = "c73_bias", *c73Headroom = "c73_headroom",
        *c73Iron = "c73_iron", *c73Age = "c73_age", *c73Noise = "c73_noise";
    inline const char *c76Fet = "c76_fet", *c76InX = "c76_inx", *c76Out = "c76_out", *c76Bias = "c76_bias", *c76Headroom = "c76_headroom",
        *c76Detector = "c76_detector", *c76Iron = "c76_iron", *c76Age = "c76_age", *c76Noise = "c76_noise";
    inline const char *c3aCell = "c3a_cell", *c3aAmp = "c3a_amp", *c3aOutX = "c3a_outx", *c3aMemory = "c3a_memory", *c3aHeadroom = "c3a_headroom",
        *c3aIron = "c3a_iron", *c3aAge = "c3a_age", *c3aNoise = "c3a_noise";

    // full parameter ID for a slot: "s1_p73_mid"
    inline juce::String of (int slot, const char* suffix) { return "s" + juce::String (slot + 1) + "_" + suffix; }
}

inline bool hasType (int t) { return isRack() || fixedType() == t; }

// every per-slot suffix that exists in this build (used to copy / swap slots)
inline const std::vector<const char*>& slotSuffixes()
{
    static const std::vector<const char*> v = []
    {
        using namespace id;
        std::vector<const char*> s;
        if (isRack()) s.push_back (type);
        s.push_back (on);
        if (hasType (kT73)) for (auto* x : { p73Variant, p73Mic, p73Gain, p73Phase, p73Hpf, p73LowF, p73LowG, p73MidF, p73MidG, p73HiQ, p73HighF,
                                            p73HighG, p73Lpf, p73EqIn, p73Out, p73Analog, p73Cont, p73HpfHz, p73LowHz, p73MidHz, p73HighHz, p73LpfHz,
                                            c73InX, c73Amp, c73OutX, c73Bias, c73Headroom, c73Iron, c73Age, c73Noise }) s.push_back (x);
        if (hasType (kT76)) for (auto* x : { f76Variant, f76In, f76Out, f76Atk, f76Rel, f76Ratio, f76Analog,
                                            c76Fet, c76InX, c76Out, c76Bias, c76Headroom, c76Detector, c76Iron, c76Age, c76Noise }) s.push_back (x);
        if (hasType (kT3A)) for (auto* x : { o3Peak, o3Gain, o3Limit, o3Hf, o3Analog, o3Cont, o3ScHz,
                                            c3aCell, c3aAmp, c3aOutX, c3aMemory, c3aHeadroom, c3aIron, c3aAge, c3aNoise }) s.push_back (x);
        return s;
    }();
    return v;
}

inline juce::NormalisableRange<float> logRange (double lo, double hi)
{
    juce::NormalisableRange<float> r ((float) lo, (float) hi, 1.f);
    r.setSkewForCentre ((float) std::sqrt (lo * hi));
    return r;
}

// BIAS (optional) / HEADROOM / IRON / AGE / NOISE — shared by every module's circuit
inline void addCommonCircuit (juce::AudioProcessorValueTreeState::ParameterLayout& L, int s, const juce::String& pre,
                              const char* bias, const char* headroom, const char* iron, const char* age, const char* noise)
{
    using namespace juce;
    auto P = [s] (const char* suf) { return ParameterID { id::of (s, suf), 1 }; };
    if (bias != nullptr) L.add (std::make_unique<AudioParameterFloat> (P (bias), pre + "Bias", NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f));
    L.add (std::make_unique<AudioParameterFloat> (P (headroom), pre + "Headroom", NormalisableRange<float> (18.f, 30.f, 0.5f), 24.f, AudioParameterFloatAttributes().withLabel ("V")));
    L.add (std::make_unique<AudioParameterFloat> (P (iron), pre + "Iron", NormalisableRange<float> (0.f, 200.f, 1.f), 100.f, AudioParameterFloatAttributes().withLabel ("%")));
    L.add (std::make_unique<AudioParameterFloat> (P (age), pre + "Age", NormalisableRange<float> (0.f, 100.f, 1.f), 0.f));
    L.add (std::make_unique<AudioParameterFloat> (P (noise), pre + "Noise", NormalisableRange<float> (0.f, 100.f, 1.f), 0.f));
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    auto dB = AudioParameterFloatAttributes().withLabel ("dB");
    auto hzA = AudioParameterFloatAttributes().withLabel ("Hz");

    for (int s = 0; s < numSlots(); ++s)
    {
        auto P = [s] (const char* suf) { return ParameterID { id::of (s, suf), 1 }; };
        const String pre = numSlots() > 1 ? "S" + String (s + 1) + " " : String();
        if (isRack())
        {
            const int def = s == 0 ? kT73 : s == 1 ? kT76 : s == 2 ? kT3A : kEmpty;   // default rack: 73 > 76 > 3A
            L.add (std::make_unique<AudioParameterChoice> (P (id::type), pre + "Module", StringArray { "Empty", "Pre/EQ", "FET Comp", "Opto Leveler" }, def));
        }
        L.add (std::make_unique<AudioParameterBool> (P (id::on), pre + "On", true));

        if (hasType (kT73))
        {
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73Variant), pre + "PRE Model", StringArray { "Classic", "Classic+ (Hi-Q / LPF)" }, 0));
            L.add (std::make_unique<AudioParameterBool> (P (id::p73Mic), pre + "PRE Mic", false));
            L.add (std::make_unique<AudioParameterInt> (P (id::p73Gain), pre + "PRE Gain Step", 0, 12, 2));   // MIC 20..80 / LINE -10..+20 (5 dB)
            L.add (std::make_unique<AudioParameterBool> (P (id::p73Phase), pre + "PRE Phase", false));
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73Hpf), pre + "PRE HPF", StringArray { "OFF", "50 Hz", "80 Hz", "160 Hz", "300 Hz" }, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73LowF), pre + "PRE Low Freq", StringArray { "35 Hz", "60 Hz", "110 Hz", "220 Hz" }, 1));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73LowG), pre + "PRE Low", NormalisableRange<float> (-16.f, 16.f, 0.5f), 0.f, dB));
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73MidF), pre + "PRE Mid Freq", StringArray { "360 Hz", "700 Hz", "1.6 kHz", "3.2 kHz", "4.8 kHz", "7.2 kHz" }, 2));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73MidG), pre + "PRE Mid", NormalisableRange<float> (-18.f, 18.f, 0.5f), 0.f, dB));
            L.add (std::make_unique<AudioParameterBool> (P (id::p73HiQ), pre + "PRE Hi-Q (Classic+)", false));
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73HighF), pre + "PRE High Freq (Classic+)", StringArray { "10 kHz", "12 kHz", "16 kHz" }, 1));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73HighG), pre + "PRE High", NormalisableRange<float> (-16.f, 16.f, 0.5f), 0.f, dB));
            L.add (std::make_unique<AudioParameterChoice> (P (id::p73Lpf), pre + "PRE LPF (Classic+)", StringArray { "OFF", "18 kHz", "10 kHz", "8 kHz", "6 kHz" }, 0));
            L.add (std::make_unique<AudioParameterBool> (P (id::p73EqIn), pre + "PRE EQ In", true));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73Out), pre + "PRE Output", NormalisableRange<float> (-24.f, 12.f, 0.5f), 0.f, dB));
            L.add (std::make_unique<AudioParameterBool> (P (id::p73Analog), pre + "PRE Analog", true));
            L.add (std::make_unique<AudioParameterBool> (P (id::p73Cont), pre + "PRE Stepless", false));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73HpfHz), pre + "PRE HPF Hz (stepless)", logRange (spec::contHpf[0], spec::contHpf[1]), 80.f, hzA));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73LowHz), pre + "PRE Low Hz (stepless)", logRange (spec::contLow[0], spec::contLow[1]), 60.f, hzA));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73MidHz), pre + "PRE Mid Hz (stepless)", logRange (spec::contMid[0], spec::contMid[1]), 1600.f, hzA));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73HighHz), pre + "PRE High Hz (stepless)", logRange (spec::contHigh[0], spec::contHigh[1]), 12000.f, hzA));
            L.add (std::make_unique<AudioParameterFloat> (P (id::p73LpfHz), pre + "PRE LPF Hz (stepless)", logRange (spec::contLpf[0], spec::contLpf[1]), 22000.f, hzA));
            StringArray inX, amp, outX;
            for (auto& c : circuit::in73) inX.add (c.name);
            for (auto& c : circuit::amp73) amp.add (c.name);
            for (auto& c : circuit::out73) outX.add (c.name);
            L.add (std::make_unique<AudioParameterChoice> (P (id::c73InX), pre + "PRE Input Transformer", inX, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c73Amp), pre + "PRE Amp", amp, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c73OutX), pre + "PRE Output Transformer", outX, 0));
            addCommonCircuit (L, s, pre + "PRE ", id::c73Bias, id::c73Headroom, id::c73Iron, id::c73Age, id::c73Noise);
        }
        if (hasType (kT76))
        {
            L.add (std::make_unique<AudioParameterChoice> (P (id::f76Variant), pre + "FET Model", StringArray { "Early (Blue)", "Low-noise (Black)", "Late (Silver)" }, 1));
            L.add (std::make_unique<AudioParameterFloat> (P (id::f76In), pre + "FET Input", NormalisableRange<float> (-12.f, 48.f, 0.5f), 6.f, dB));
            L.add (std::make_unique<AudioParameterFloat> (P (id::f76Out), pre + "FET Output", NormalisableRange<float> (-24.f, 24.f, 0.5f), 0.f, dB));
            L.add (std::make_unique<AudioParameterFloat> (P (id::f76Atk), pre + "FET Attack", NormalisableRange<float> (1.f, 7.f, 0.1f), 3.f));
            L.add (std::make_unique<AudioParameterFloat> (P (id::f76Rel), pre + "FET Release", NormalisableRange<float> (1.f, 7.f, 0.1f), 5.f));
            L.add (std::make_unique<AudioParameterChoice> (P (id::f76Ratio), pre + "FET Ratio", StringArray { "4:1", "8:1", "12:1", "20:1", "ALL" }, 0));
            L.add (std::make_unique<AudioParameterBool> (P (id::f76Analog), pre + "FET Analog", true));
            StringArray fet, inX, out;
            for (auto& c : circuit::fet76) fet.add (c.name);
            for (auto& c : circuit::in76) inX.add (c.name);
            for (auto& c : circuit::out76) out.add (c.name);
            L.add (std::make_unique<AudioParameterChoice> (P (id::c76Fet), pre + "FET Device", fet, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c76InX), pre + "FET Input Transformer", inX, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c76Out), pre + "FET Output Stage", out, 0));
            L.add (std::make_unique<AudioParameterFloat> (P (id::c76Detector), pre + "FET Detector", NormalisableRange<float> (0.f, 100.f, 1.f), 50.f));
            addCommonCircuit (L, s, pre + "FET ", id::c76Bias, id::c76Headroom, id::c76Iron, id::c76Age, id::c76Noise);
        }
        if (hasType (kT3A))
        {
            L.add (std::make_unique<AudioParameterFloat> (P (id::o3Peak), pre + "OPTO Peak Reduction", NormalisableRange<float> (0.f, 100.f, 1.f), 40.f));
            L.add (std::make_unique<AudioParameterFloat> (P (id::o3Gain), pre + "OPTO Gain", NormalisableRange<float> (0.f, 100.f, 1.f), 50.f));
            L.add (std::make_unique<AudioParameterBool> (P (id::o3Limit), pre + "OPTO Limit", false));
            L.add (std::make_unique<AudioParameterFloat> (P (id::o3Hf), pre + "OPTO HF Emphasis", NormalisableRange<float> (0.f, 100.f, 1.f), 0.f));
            L.add (std::make_unique<AudioParameterBool> (P (id::o3Analog), pre + "OPTO Analog", true));
            L.add (std::make_unique<AudioParameterBool> (P (id::o3Cont), pre + "OPTO Stepless", false));
            L.add (std::make_unique<AudioParameterFloat> (P (id::o3ScHz), pre + "OPTO Emph Hz (stepless)", logRange (spec::contSc[0], spec::contSc[1]), 2500.f, hzA));
            StringArray cell, amp, outX;
            for (auto& c : circuit::cell3a) cell.add (c.name);
            for (auto& c : circuit::amp3a) amp.add (c.name);
            for (auto& c : circuit::out3a) outX.add (c.name);
            L.add (std::make_unique<AudioParameterChoice> (P (id::c3aCell), pre + "OPTO Opto Cell", cell, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c3aAmp), pre + "OPTO Amp", amp, 0));
            L.add (std::make_unique<AudioParameterChoice> (P (id::c3aOutX), pre + "OPTO Output Transformer", outX, 0));
            L.add (std::make_unique<AudioParameterFloat> (P (id::c3aMemory), pre + "OPTO Memory", NormalisableRange<float> (0.f, 100.f, 1.f), 50.f));
            addCommonCircuit (L, s, pre + "OPTO ", nullptr, id::c3aHeadroom, id::c3aIron, id::c3aAge, id::c3aNoise);
        }
    }
    return L;
}

// Read current parameter values into DSP structs (audio thread and editor alike)
struct ParamReader
{
    explicit ParamReader (juce::AudioProcessorValueTreeState& s) : st (s) {}
    float v (int slot, const char* suf) const { auto* p = st.getRawParameterValue (id::of (slot, suf)); return p ? p->load() : 0.f; }
    bool b (int slot, const char* suf) const { return v (slot, suf) > 0.5f; }
    int  i (int slot, const char* suf) const { return (int) std::lround (v (slot, suf)); }

    int type (int s) const { return isRack() ? i (s, id::type) : fixedType(); }
    bool on (int s) const { return type (s) != kEmpty && b (s, id::on); }

    Pre73Params pre73 (int s) const
    {
        using namespace id;
        Pre73Params p;
        p.variant = i (s, p73Variant); p.mic = b (s, p73Mic);
        p.gainDb = p.mic ? spec::micGainDb (i (s, p73Gain)) : spec::lineGainDb (i (s, p73Gain));
        p.phase = b (s, p73Phase); p.hpf = i (s, p73Hpf); p.lowFreq = i (s, p73LowF); p.lowGain = v (s, p73LowG);
        p.midFreq = i (s, p73MidF); p.midGain = v (s, p73MidG); p.hiQ = b (s, p73HiQ) && p.isPlus();
        p.highFreq = i (s, p73HighF); p.highGain = v (s, p73HighG); p.lpf = i (s, p73Lpf);
        p.eqIn = b (s, p73EqIn); p.output = v (s, p73Out);
        p.analog = b (s, p73Analog); p.cont = b (s, p73Cont);
        p.hpfC = v (s, p73HpfHz); p.lowC = v (s, p73LowHz); p.midC = v (s, p73MidHz); p.highC = v (s, p73HighHz); p.lpfC = v (s, p73LpfHz);
        p.inX = i (s, c73InX); p.amp = i (s, c73Amp); p.outX = i (s, c73OutX);
        p.bias = v (s, c73Bias); p.headroom = v (s, c73Headroom); p.iron = v (s, c73Iron); p.age = v (s, c73Age); p.noise = v (s, c73Noise);
        return p;
    }
    Fet76Params fet76 (int s) const
    {
        using namespace id;
        Fet76Params p; p.variant = i (s, f76Variant); p.input = v (s, f76In); p.output = v (s, f76Out);
        p.attack = v (s, f76Atk); p.release = v (s, f76Rel); p.ratio = i (s, f76Ratio); p.analog = b (s, f76Analog);
        p.fet = i (s, c76Fet); p.inX = i (s, c76InX); p.out = i (s, c76Out);
        p.bias = v (s, c76Bias); p.headroom = v (s, c76Headroom); p.detector = v (s, c76Detector); p.iron = v (s, c76Iron); p.age = v (s, c76Age); p.noise = v (s, c76Noise);
        return p;
    }
    Opto3AParams opto3a (int s) const
    {
        using namespace id;
        Opto3AParams p; p.peak = v (s, o3Peak); p.gain = v (s, o3Gain); p.limit = b (s, o3Limit); p.hf = v (s, o3Hf);
        p.analog = b (s, o3Analog); p.cont = b (s, o3Cont); p.scHz = v (s, o3ScHz);
        p.cell = i (s, c3aCell); p.amp = i (s, c3aAmp); p.outX = i (s, c3aOutX);
        p.memory = v (s, c3aMemory); p.headroom = v (s, c3aHeadroom); p.iron = v (s, c3aIron); p.age = v (s, c3aAge); p.noise = v (s, c3aNoise);
        return p;
    }
    juce::AudioProcessorValueTreeState& st;
};
} // namespace vr
