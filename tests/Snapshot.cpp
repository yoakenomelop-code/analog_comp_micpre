// Offscreen check: slot editing, audio through the plugin, then render the editor to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
using namespace vr;
static juce::String types (VintageRackProcessor& p) { ParamReader rd (p.state); juce::String s; for (int i = 0; i < numSlots(); ++i) s << rd.type (i) << " "; return s; }
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outPath = argc > 1 ? argv[1] : "snapshot.png";
    const int hlBand = argc > 2 ? juce::String (argv[2]).getIntValue() : vr::ui::hlMid;
    const int hlSlot = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
    VintageRackProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    auto set = [&] (int s, const char* suf, float v) { if (auto* p = proc.state.getParameter (id::of (s, suf))) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    if (isRack())
    {
        std::printf ("types initially:  %s\n", types (proc).toRawUTF8());
        proc.addModule (kT76);                 // Pre FET Opto FET
        set (1, id::f76In, 20);                // mark slot 2 (the first 76)
        proc.duplicateSlot (1);                // Pre FET FET Opto FET
        proc.moveSlot (4, 0);                  // FET Pre FET FET Opto
        proc.removeSlot (3);                   // FET Pre FET Opto
        std::printf ("after edits:      %s (expect 2 1 2 3 0 0 0 0), slot3 input %.1f (expect 20)\n", types (proc).toRawUTF8(), ParamReader (proc.state).fet76 (2).input);
        proc.moveSlot (0, 3);                  // Pre FET Opto FET
        proc.addModule (kT73);                 // Pre FET Opto FET Pre
        std::printf ("final:            %s\n", types (proc).toRawUTF8());
        set (0, id::p73Hpf, 2); set (0, id::p73LowG, 3); set (0, id::p73MidF, 3); set (0, id::p73MidG, 4); set (0, id::p73HighG, 3);
        set (4, id::p73Cont, 1); set (4, id::p73MidHz, 900); set (4, id::p73MidG, -5); set (4, id::p73Variant, 1); set (4, id::p73LpfHz, 9000);
        set (2, id::o3Hf, 60); set (2, id::o3Cont, 1); set (2, id::o3ScHz, 4000); set (3, id::f76Analog, 0); set (3, id::f76Variant, 0);
    }
    else if (fixedType() == kT73) { set (0, id::p73Hpf, 2); set (0, id::p73MidG, 4); }
    else if (fixedType() == kT3A) set (0, id::o3Hf, 60);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    juce::Random rnd (1);
    float maxOut = 0; bool nan = false;
    for (int b = 0; b < 300; ++b)
    {
        for (int i = 0; i < 512; ++i) { const double t = (b * 512 + i) / 48000.0; const float s = (float) (0.5 * std::sin (2 * juce::MathConstants<double>::pi * 110 * t) * ((b / 20) % 2 ? 1.0 : 0.1) + 0.05 * (rnd.nextFloat() - 0.5f)); buf.setSample (0, i, s); buf.setSample (1, i, s); }
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float v = buf.getSample (c, i); nan |= ! std::isfinite (v); maxOut = std::max (maxOut, std::abs (v)); }
    }
    std::printf ("latency %d samples, max out %.3f, nan %d, params %d\n", proc.getLatencySamples(), maxOut, (int) nan, proc.getParameters().size());
    juce::MemoryBlock mb; proc.getStateInformation (mb); proc.setStateInformation (mb.getData(), (int) mb.getSize());
    // presets
    if (isRack())
    {
        std::printf ("programs: %d, first '%s', last '%s'\n", proc.getNumPrograms(), proc.getProgramName (0).toRawUTF8(), proc.getProgramName (proc.getNumPrograms() - 1).toRawUTF8());
        proc.applyRackPreset (1);   // ロックボーカル
        ParamReader rd (proc.state);
        std::printf ("rack preset 1 types: %s  | Pre midGain %.1f hpf %d variant %d gainDb %d | FET variant %d ratio %d output %.1f | Opto peak %.0f gain %.0f\n",
            types (proc).toRawUTF8(), rd.pre73 (0).midGain, rd.pre73 (0).hpf, rd.pre73 (0).variant, rd.pre73 (0).gainDb, rd.fet76 (1).variant, rd.fet76 (1).ratio, rd.fet76 (1).output, rd.opto3a (2).peak, rd.opto3a (2).gain);
        proc.applyModulePreset (1, 7);   // FET: 初期型ブルー：荒め
        std::printf ("module preset 76/7: fet %d out %d iron %.0f bias %.0f\n", rd.fet76 (1).fet, rd.fet76 (1).out, rd.fet76 (1).iron, rd.fet76 (1).bias);
        set (0, id::c73Iron, 160); set (0, id::c73Noise, 30);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* rack = dynamic_cast<VintageRackEditor*> (ed.get());
    rack->setCircuitOpen (0, true); rack->setCircuitOpen (1, true);
    for (int i = 0; i < 3; ++i) rack->timerCallback();
    ed->setSize (1100, 1260); rack->timerCallback();
    rack->hover (hlSlot, hlBand, juce::String::fromUTF8 ("（スナップショット用のハイライト）"));
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (outPath)); f.deleteFile();
    juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("editor %dx%d -> %s\n", ed->getWidth(), ed->getHeight(), f.getFullPathName().toRawUTF8());
    return 0;
}
