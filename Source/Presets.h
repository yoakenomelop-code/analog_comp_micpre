#pragma once
// Factory presets — the same Resources/presets.json the web version uses, compiled in as binary data.
#include <juce_core/juce_core.h>
#include "BinaryData.h"
#include "Params.h"

namespace vr
{
inline const char* typeKey (int t) { return t == kT73 ? "pre73" : t == kT76 ? "fet76" : t == kT3A ? "opto3a" : ""; }
inline int typeFromKey (const juce::String& k) { return k == "pre73" ? kT73 : k == "fet76" ? kT76 : k == "opto3a" ? kT3A : kEmpty; }

class PresetBank
{
public:
    PresetBank() { root = juce::JSON::parse (juce::String::fromUTF8 (VRData::presets_json, VRData::presets_jsonSize)); }

    int numModule (int type) const { auto* a = moduleArray (type); return a ? a->size() : 0; }
    juce::String moduleName (int type, int i) const { auto* a = moduleArray (type); return a && juce::isPositiveAndBelow (i, a->size()) ? (*a)[i]["name"].toString() : juce::String(); }
    juce::var moduleValues (int type, int i) const { auto* a = moduleArray (type); return a && juce::isPositiveAndBelow (i, a->size()) ? (*a)[i]["p"] : juce::var(); }
    juce::var moduleValuesByName (int type, const juce::String& name) const
    {
        if (auto* a = moduleArray (type)) for (auto& e : *a) if (e["name"].toString() == name) return e["p"];
        return {};
    }
    int numRack() const { auto* a = root["rack"].getArray(); return a ? a->size() : 0; }
    juce::String rackName (int i) const { auto* a = root["rack"].getArray(); return a && juce::isPositiveAndBelow (i, a->size()) ? (*a)[i]["name"].toString() : juce::String(); }
    juce::var rackSlots (int i) const { auto* a = root["rack"].getArray(); return a && juce::isPositiveAndBelow (i, a->size()) ? (*a)[i]["slots"] : juce::var(); }

    // JSON keys (web names) → per-slot parameter suffix and plain value
    static std::vector<std::pair<const char*, float>> toParams (int type, const juce::var& p)
    {
        std::vector<std::pair<const char*, float>> out;
        auto* obj = p.getDynamicObject();
        if (obj == nullptr) return out;
        struct K { const char* key; const char* suffix; };
        static const K k73[] = { { "variant", id::p73Variant }, { "mic", id::p73Mic }, { "phase", id::p73Phase }, { "hpf", id::p73Hpf },
            { "lowFreq", id::p73LowF }, { "lowGain", id::p73LowG }, { "midFreq", id::p73MidF }, { "midGain", id::p73MidG }, { "hiQ", id::p73HiQ },
            { "highFreq", id::p73HighF }, { "highGain", id::p73HighG }, { "lpf", id::p73Lpf }, { "eqIn", id::p73EqIn }, { "output", id::p73Out },
            { "analog", id::p73Analog }, { "cont", id::p73Cont }, { "hpfHz", id::p73HpfHz }, { "lowHz", id::p73LowHz }, { "midHz", id::p73MidHz },
            { "highHz", id::p73HighHz }, { "lpfHz", id::p73LpfHz }, { "inX", id::c73InX }, { "amp", id::c73Amp }, { "outX", id::c73OutX },
            { "bias", id::c73Bias }, { "headroom", id::c73Headroom }, { "iron", id::c73Iron }, { "age", id::c73Age }, { "noise", id::c73Noise } };
        static const K k76[] = { { "variant", id::f76Variant }, { "input", id::f76In }, { "output", id::f76Out }, { "attack", id::f76Atk },
            { "release", id::f76Rel }, { "ratio", id::f76Ratio }, { "analog", id::f76Analog }, { "fet", id::c76Fet }, { "inX", id::c76InX },
            { "out", id::c76Out }, { "bias", id::c76Bias }, { "headroom", id::c76Headroom }, { "detector", id::c76Detector }, { "iron", id::c76Iron },
            { "age", id::c76Age }, { "noise", id::c76Noise } };
        static const K k3a[] = { { "peak", id::o3Peak }, { "gain", id::o3Gain }, { "limit", id::o3Limit }, { "hf", id::o3Hf }, { "analog", id::o3Analog },
            { "cont", id::o3Cont }, { "scHz", id::o3ScHz }, { "cell", id::c3aCell }, { "amp", id::c3aAmp }, { "outX", id::c3aOutX },
            { "memory", id::c3aMemory }, { "headroom", id::c3aHeadroom }, { "iron", id::c3aIron }, { "age", id::c3aAge }, { "noise", id::c3aNoise } };
        auto run = [&] (const K* ks, size_t n)
        {
            for (size_t i = 0; i < n; ++i)
                if (obj->hasProperty (ks[i].key)) out.push_back ({ ks[i].suffix, (float) (double) obj->getProperty (ks[i].key) });
        };
        if (type == kT73)
        {
            run (k73, std::size (k73));
            if (obj->hasProperty ("gain"))   // web stores front-panel dB; the plugin stores the 5 dB step index
            {
                const bool mic = obj->hasProperty ("mic") && (bool) obj->getProperty ("mic");
                const int g = (int) obj->getProperty ("gain");
                out.push_back ({ id::p73Gain, (float) (mic ? (g - 20) / 5 : (g + 10) / 5) });
            }
        }
        else if (type == kT76) run (k76, std::size (k76));
        else if (type == kT3A) run (k3a, std::size (k3a));
        return out;
    }

private:
    const juce::Array<juce::var>* moduleArray (int type) const { return root["module"][typeKey (type)].getArray(); }
    juce::var root;
};
} // namespace vr
