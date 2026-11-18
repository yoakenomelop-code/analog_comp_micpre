#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

class VintageRackEditor;

namespace vr::ui
{
namespace col   // shared by knobs and curves (same values as the web version)
{
    const juce::Colour bg { 0xff15171b }, panel { 0xff1f2228 }, panel2 { 0xff272b33 }, line { 0xff353a44 },
        ink { 0xffe7e9ee }, inkDim { 0xff9097a5 }, accent { 0xffe8b04a },
        gain { 0xffe0574f }, hpf { 0xff7d8fb3 }, low { 0xffe8a33a }, mid { 0xff4fbf86 }, high { 0xffd46fd0 },
        lpf { 0xff4cb6d6 }, sc { 0xfff08a3c }, dyn { 0xffb9bfca }, analog { 0xffd8c58f },
        good { 0xff4fbf86 }, warn { 0xffe8b04a }, bad { 0xffe0574f };
}
enum HL { hlNone = -1, hlHPF = 0, hlLow, hlMid, hlHigh, hlLPF, hlGain, hlSC, hlDyn, hlAnalog };
juce::Colour hlColour (int h);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
};

// Any control can light up its band on the map and explain itself in the hint line
struct HoverSink { virtual ~HoverSink() = default; virtual void hover (int slot, int hl, const juce::String& text) = 0; };

// ---------------------------------------------------------------- knob with caption + value
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption, int slot, int highlight, HoverSink& sink);
    void attach (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    std::function<juce::String (double)> format;
    std::function<juce::String()> describe;
    juce::Slider slider;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    int slot, band;
    bool stepless = false;   // drawn with a dotted rim
private:
    juce::String caption;
    HoverSink& sink;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// ---------------------------------------------------------------- frequency map
class FreqMap : public juce::Component
{
public:
    FreqMap (VintageRackProcessor&, HoverSink&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void tick();
    int hlSlot = -1, hl = hlNone;
    float range = 20.f;
private:
    static constexpr int NF = 256;
    using Curve = std::array<double, NF>;
    struct SlotCurves
    {
        int type = -1; bool on = false;
        Pre73Params p; Fet76Params f; Opto3AParams o;
        std::array<Curve, kNumBands> band {}; Curve eqTotal {}, analog {}, analogIfOn {}, sc {};
    };
    void compute (int slot, SlotCurves&);
    VintageRackProcessor& proc; HoverSink& sink; ParamReader reader;
    std::array<double, NF> freqs {};
    std::vector<SlotCurves> slots;
    std::array<float, NF> spec {};
    juce::dsp::FFT fft { VintageRackProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) VintageRackProcessor::fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> ((size_t) VintageRackProcessor::fftSize), fftData = std::vector<float> ((size_t) VintageRackProcessor::fftSize * 2);
    int ringPos = 0; bool hasSpec = false;
    juce::Rectangle<float> plot() const;
    float xOf (double f) const; float yOf (double db) const;
};

// ---------------------------------------------------------------- dynamics display: transfer curve + GR
class DynDisplay : public juce::Component
{
public:
    DynDisplay (VintageRackProcessor& p, int slot, int type) : proc (p), slot (slot), type (type) {}
    void paint (juce::Graphics&) override;
    float gr = 0;
private:
    VintageRackProcessor& proc; int slot, type; ParamReader reader { proc.state };
};

// ---------------------------------------------------------------- harmonics display (CIRCUIT panel)
class HarmDisplay : public juce::Component
{
public:
    HarmDisplay (VintageRackProcessor& p, int slot, int type) : proc (p), slot (slot), type (type) {}
    void paint (juce::Graphics&) override;
    void refresh();                    // recomputes only when the module's parameters changed
private:
    VintageRackProcessor& proc; int slot, type; ParamReader reader { proc.state };
    Pre73Params p73; Fet76Params p76; Opto3AParams p3a; bool valid = false;
    Harmonics h100, h1k;
};

// ---------------------------------------------------------------- module panel base
class ModulePanel : public juce::Component
{
public:
    ModulePanel (::VintageRackEditor&, VintageRackProcessor&, HoverSink&, int slot, int type, const juce::String& subtitle);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    virtual void layoutControls (juce::Rectangle<int> area) = 0;
    virtual void tick() {}
    virtual void setGR (float) {}
    static constexpr int circuitH = 168;
    static int heightFor (int type, bool open) { return (type == kT73 ? 156 : 150) + (open ? circuitH : 0); }
    void mouseEnter (const juce::MouseEvent&) override;
    void tickCircuit();
    const int slot, type;
    static constexpr int railW = 44;
    juce::TextButton power { "ON" }, up { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, down { juce::CharPointer_UTF8 ("\xe2\x96\xbc") };
protected:
    struct Group { juce::Rectangle<int> r; juce::String name; int hl; };
    std::vector<Group> groups;
    juce::Rectangle<int> placeGroup (juce::Rectangle<int>& row, int width, const juce::String& name, int hl);
    juce::String pid (const char* suf) const { return id::of (slot, suf); }
    juce::TextButton& toggle (juce::TextButton& b, const char* suffix);
    void addHeaderToggles (const char* analogSuffix, const char* contSuffix);
    ::VintageRackEditor& editor; VintageRackProcessor& proc; HoverSink& sink;
    juce::String subtitle;
    juce::TextButton analogBtn { "ANALOG" }, contBtn { juce::String::fromUTF8 ("無段階") }, dupBtn { juce::String::fromUTF8 ("複製") }, delBtn { juce::String::fromUTF8 ("削除") };
    bool hasCont = false;
    juce::ComboBox variant, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> variantAtt;
    // CIRCUIT section
    void buildCircuit();
    void layoutCircuit (juce::Rectangle<int> area);
    bool circuitOpen() const;
    juce::TextButton circuitBtn { juce::String::fromUTF8 ("回路") }, circuitReset { juce::String::fromUTF8 ("回路を初期化") };
    std::array<juce::ComboBox, 3> cBox;
    std::array<juce::String, 3> cLabel;
    std::array<const char*, 3> cSuffix {};
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> cAtts;
    std::vector<std::unique_ptr<Knob>> cKnobs;
    std::unique_ptr<HarmDisplay> harm;
    juce::Rectangle<int> circuitArea;
    int subtitleRight = 0;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAtts;
    bool dragging = false;
};

class Pre73Panel : public ModulePanel
{
public:
    Pre73Panel (::VintageRackEditor&, VintageRackProcessor&, HoverSink&, int slot);
    void layoutControls (juce::Rectangle<int>) override;
    void tick() override;
private:
    Knob gain, hpf, lowF, lowG, midF, midG, highF, highG, lpf, out, hpfC, lowC, midC, highC, lpfC;
    juce::TextButton mic { "MIC" }, phase { juce::CharPointer_UTF8 ("\xc3\x98") }, hiQ { "HI-Q" }, eqIn { "EQ IN" };
    bool lastMic = false, lastCont = false; int lastVariant = -1;
};

class Fet76Panel : public ModulePanel
{
public:
    Fet76Panel (::VintageRackEditor&, VintageRackProcessor&, HoverSink&, int slot);
    void layoutControls (juce::Rectangle<int>) override;
    void tick() override;
    void setGR (float v) override { dyn.gr = std::max (v, dyn.gr * 0.8f); }
private:
    Knob input, output, attack, release;
    juce::TextButton ratio[5];
    DynDisplay dyn;
};

class Opto3APanel : public ModulePanel
{
public:
    Opto3APanel (::VintageRackEditor&, VintageRackProcessor&, HoverSink&, int slot);
    void layoutControls (juce::Rectangle<int>) override;
    void tick() override;
    void setGR (float v) override { dyn.gr = std::max (v, dyn.gr * 0.8f); }
private:
    Knob peak, gain, hf, scFreq;
    juce::TextButton comp { "COMP" }, limit { "LIMIT" };
    DynDisplay dyn;
    bool lastCont = false;
};
} // namespace vr::ui

// ---------------------------------------------------------------- the editor
class VintageRackEditor : public juce::AudioProcessorEditor, public vr::ui::HoverSink, private juce::Timer
{
public:
    explicit VintageRackEditor (VintageRackProcessor&);
    ~VintageRackEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void hover (int slot, int hl, const juce::String& text) override;
    void dragPanel (vr::ui::ModulePanel&, int yInRack, bool finished);
    int highlightSlot() const { return hlSlot; }
    bool isCircuitOpen (int slot) const { return juce::isPositiveAndBelow (slot, (int) circuitOpen.size()) && circuitOpen[(size_t) slot]; }
    void setCircuitOpen (int slot, bool open) { if (juce::isPositiveAndBelow (slot, (int) circuitOpen.size())) { circuitOpen[(size_t) slot] = open; layoutRack(); } }
    int highlightBand() const { return hlBand; }
    VintageRackProcessor& proc;
    void timerCallback() override;
private:
    void rebuildPanels();
    void applyPreset (int index, const juce::String& name);
    void layoutRack();
    vr::ui::Look look;
    std::unique_ptr<vr::ui::FreqMap> map;
    juce::Viewport viewport;
    juce::Component rackContent;
    std::vector<std::unique_ptr<vr::ui::ModulePanel>> panels;
    std::vector<int> signature;
    std::array<bool, (size_t) vr::numSlots()> circuitOpen {};
    juce::ComboBox headerPreset;
    juce::TextButton add73 { juce::String::fromUTF8 ("＋ Pre / EQ") }, add76 { juce::String::fromUTF8 ("＋ FET Comp") }, add3A { juce::String::fromUTF8 ("＋ Opto Leveler") },
        allOn { juce::String::fromUTF8 ("ANALOG 全オン") }, allOff { juce::String::fromUTF8 ("ANALOG 全オフ") }, r20 { juce::String::fromUTF8 ("±20 dB") }, r6 { juce::String::fromUTF8 ("±6 dB") }, r2 { juce::String::fromUTF8 ("±2 dB") };
    juce::String hint;
    int hlSlot = -1, hlBand = vr::ui::hlNone;
    float inLvl = 0, outLvl = 0;
    int dropY = -1;
    juce::Rectangle<int> hintArea, meterArea, toolbarArea;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageRackEditor)
};
