#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"

class VintageRackProcessor : public juce::AudioProcessor
{
public:
    VintageRackProcessor();
    ~VintageRackProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // factory presets: rack chains (rack build) or module presets (single builds)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // ---- metering, read by the editor (lock-free)
    std::atomic<float> inPeak { 0 }, outPeak { 0 };
    std::array<std::atomic<float>, (size_t) vr::numSlots()> gr {};

    // ---- output spectrum FIFO (mono sum, at host rate)
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::AbstractFifo specFifo { fftSize * 4 };
    std::vector<float> specBuffer = std::vector<float> ((size_t) fftSize * 4);
    double getHostRate() const { return hostRate; }

    // ---- rack editing (message thread). Slots are kept packed: used slots first, in signal order.
    int usedSlots() const;
    bool addModule (int type);
    void removeSlot (int slot);
    void duplicateSlot (int slot);
    void moveSlot (int from, int to);
    void setAllAnalog (bool on);
    void applyModulePreset (int slot, int index);
    void applyRackPreset (int index);
    const vr::PresetBank presets;
    int currentProgram = 0;

private:
    std::vector<float> readSlot (int slot) const;
    void writeSlot (int slot, const std::vector<float>& values);
    std::vector<float> defaultSlot (int type) const;
    void setParam (const juce::String& pid, float normalised);

    vr::ParamReader reader { state };
    std::array<vr::Pre73, (size_t) vr::numSlots()> pre73;
    std::array<vr::Fet76, (size_t) vr::numSlots()> fet76;
    std::array<vr::Opto3A, (size_t) vr::numSlots()> opto3a;
    std::array<int, (size_t) vr::numSlots()> lastType {};
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> monoScratch;
    double hostRate = 48000, osRate = 96000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageRackProcessor)
};
