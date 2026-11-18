#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vr;

VintageRackProcessor::VintageRackProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "VintageRack", createLayout())
{
    lastType.fill (-1);
}

bool VintageRackProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void VintageRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    // 2x oversampling around the whole chain keeps saturation and the fast FET detector clean.
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) std::ceil (os->getLatencyInSamples()));
    osRate = sampleRate * 2.0;
    for (int s = 0; s < numSlots(); ++s) { pre73[(size_t) s].prepare (osRate); fet76[(size_t) s].prepare (osRate); opto3a[(size_t) s].prepare (osRate); }
    lastType.fill (-1);
    monoScratch.setSize (2, samplesPerBlock);
}

void VintageRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), numCh = buffer.getNumChannels();
    if (n == 0 || numCh == 0) return;

    inPeak.store (std::max (inPeak.load(), buffer.getMagnitude (0, n)));

    juce::AudioBuffer<float>* work = &buffer;
    if (numCh == 1)
    {
        if (monoScratch.getNumSamples() < n) monoScratch.setSize (2, n, false, false, true);
        monoScratch.copyFrom (0, 0, buffer, 0, 0, n);
        monoScratch.copyFrom (1, 0, buffer, 0, 0, n);
        work = &monoScratch;
    }
    juce::dsp::AudioBlock<float> block (work->getArrayOfWritePointers(), 2, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int un = (int) up.getNumSamples();
    float* chans[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };

    for (int s = 0; s < numSlots(); ++s)
    {
        const auto idx = (size_t) s;
        const int t = reader.type (s);
        if (t != lastType[idx])             // module type changed in this slot: start from a clean state
        {
            lastType[idx] = t;
            pre73[idx].prepare (osRate); fet76[idx].prepare (osRate); opto3a[idx].prepare (osRate);
        }
        if (! reader.on (s)) continue;
        float g = 0;
        if (t == kT73)      { pre73[idx].setParams (reader.pre73 (s)); pre73[idx].process (chans, 2, un); }
        else if (t == kT76) { fet76[idx].setParams (reader.fet76 (s)); fet76[idx].process (chans, 2, un); g = fet76[idx].grMeter; }
        else if (t == kT3A) { opto3a[idx].setParams (reader.opto3a (s)); opto3a[idx].process (chans, 2, un); g = opto3a[idx].grMeter; }
        gr[idx].store (std::max (gr[idx].load(), g));
    }
    os->processSamplesDown (block);
    if (numCh == 1) buffer.copyFrom (0, 0, monoScratch, 0, 0, n);
    for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, n);

    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));

    const auto scope = specFifo.write (n);
    auto feed = [&] (int start, int size, int offset)
    {
        for (int i = 0; i < size; ++i)
        {
            float v = buffer.getSample (0, offset + i);
            if (numCh > 1) v = 0.5f * (v + buffer.getSample (1, offset + i));
            specBuffer[(size_t) (start + i)] = v;
        }
    };
    if (scope.blockSize1 > 0) feed (scope.startIndex1, scope.blockSize1, 0);
    if (scope.blockSize2 > 0) feed (scope.startIndex2, scope.blockSize2, scope.blockSize1);
}

// ----------------------------------------------------------------------------- rack editing
void VintageRackProcessor::setParam (const juce::String& pid, float normalised)
{
    if (auto* p = state.getParameter (pid))
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (normalised); p->endChangeGesture();
    }
}
std::vector<float> VintageRackProcessor::readSlot (int slot) const
{
    std::vector<float> v;
    for (auto* suf : slotSuffixes()) v.push_back (state.getParameter (id::of (slot, suf))->getValue());
    return v;
}
void VintageRackProcessor::writeSlot (int slot, const std::vector<float>& values)
{
    const auto& sfx = slotSuffixes();
    for (size_t k = 0; k < sfx.size() && k < values.size(); ++k) setParam (id::of (slot, sfx[k]), values[k]);
}
std::vector<float> VintageRackProcessor::defaultSlot (int type) const
{
    std::vector<float> v;
    for (auto* suf : slotSuffixes())
    {
        auto* p = state.getParameter (id::of (0, suf));
        if (juce::String (suf) == id::type) v.push_back (p->convertTo0to1 ((float) type));
        else v.push_back (p->getDefaultValue());
    }
    return v;
}
int VintageRackProcessor::usedSlots() const
{
    if (! isRack()) return 1;
    int n = 0;
    for (int s = 0; s < numSlots(); ++s) if (reader.type (s) != kEmpty) n = s + 1;
    return n;
}
bool VintageRackProcessor::addModule (int type)
{
    if (! isRack()) return false;
    const int s = usedSlots();
    if (s >= numSlots()) return false;
    writeSlot (s, defaultSlot (type));
    return true;
}
void VintageRackProcessor::removeSlot (int slot)
{
    if (! isRack()) return;
    const int n = usedSlots();
    if (slot < 0 || slot >= n) return;
    for (int s = slot; s < n - 1; ++s) writeSlot (s, readSlot (s + 1));
    writeSlot (n - 1, defaultSlot (kEmpty));
}
void VintageRackProcessor::duplicateSlot (int slot)
{
    if (! isRack()) return;
    const int n = usedSlots();
    if (n >= numSlots() || slot < 0 || slot >= n) return;
    for (int s = n; s > slot + 1; --s) writeSlot (s, readSlot (s - 1));
    writeSlot (slot + 1, readSlot (slot));
}
void VintageRackProcessor::moveSlot (int from, int to)
{
    const int n = usedSlots();
    if (! isRack() || from == to || from < 0 || to < 0 || from >= n || to >= n) return;
    const auto moving = readSlot (from);
    if (from < to) for (int s = from; s < to; ++s) writeSlot (s, readSlot (s + 1));
    else           for (int s = from; s > to; --s) writeSlot (s, readSlot (s - 1));
    writeSlot (to, moving);
}
void VintageRackProcessor::setAllAnalog (bool on)
{
    for (int s = 0; s < numSlots(); ++s)
        for (auto* suf : { id::p73Analog, id::f76Analog, id::o3Analog })
            if (state.getParameter (id::of (s, suf)) != nullptr) setParam (id::of (s, suf), on ? 1.f : 0.f);
}

// ----------------------------------------------------------------------------- factory presets
void VintageRackProcessor::applyModulePreset (int slot, int index)
{
    ParamReader rd (state);
    const int type = rd.type (slot);
    if (type == kEmpty || ! juce::isPositiveAndBelow (index, presets.numModule (type))) return;
    auto values = defaultSlot (type);
    const auto& sfx = slotSuffixes();
    auto setPlain = [&] (const char* suffix, float plain)
    {
        for (size_t k = 0; k < sfx.size(); ++k)
            if (juce::String (sfx[k]) == suffix) values[k] = state.getParameter (id::of (slot, suffix))->convertTo0to1 (plain);
    };
    for (auto& kv : PresetBank::toParams (type, presets.moduleValues (type, index))) setPlain (kv.first, kv.second);
    writeSlot (slot, values);
}
void VintageRackProcessor::applyRackPreset (int index)
{
    const auto slots = presets.rackSlots (index);
    if (! isRack() || ! slots.isArray()) return;
    for (int s = 0; s < numSlots(); ++s)
    {
        if (s >= slots.size()) { writeSlot (s, defaultSlot (kEmpty)); continue; }
        const auto& sl = slots[s];
        const int type = typeFromKey (sl["type"].toString());
        auto values = defaultSlot (type);
        const auto& sfx = slotSuffixes();
        auto apply = [&] (const juce::var& p)
        {
            for (auto& kv : PresetBank::toParams (type, p))
                for (size_t k = 0; k < sfx.size(); ++k)
                    if (juce::String (sfx[k]) == kv.first) values[k] = state.getParameter (id::of (s, kv.first))->convertTo0to1 (kv.second);
        };
        // the slot's own values are applied on top of the module preset it names; "gain" needs MIC from either level
        juce::var merged (new juce::DynamicObject());
        auto mergeIn = [&] (const juce::var& p) { if (auto* o = p.getDynamicObject()) for (auto& prop : o->getProperties()) merged.getDynamicObject()->setProperty (prop.name, prop.value); };
        if (sl.hasProperty ("preset")) mergeIn (presets.moduleValuesByName (type, sl["preset"].toString()));
        mergeIn (sl["p"]);
        apply (merged);
        writeSlot (s, values);
    }
}
int VintageRackProcessor::getNumPrograms() { return std::max (1, isRack() ? presets.numRack() : presets.numModule (fixedType())); }
const juce::String VintageRackProcessor::getProgramName (int index) { return isRack() ? presets.rackName (index) : presets.moduleName (fixedType(), index); }
void VintageRackProcessor::setCurrentProgram (int index)
{
    if (index == currentProgram || ! juce::isPositiveAndBelow (index, getNumPrograms())) return;   // hosts often call this with the current index on load
    currentProgram = index;
    if (isRack()) applyRackPreset (index); else applyModulePreset (0, index);
}

void VintageRackProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, destData);
}
void VintageRackProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType())) state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VintageRackProcessor::createEditor() { return new VintageRackEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VintageRackProcessor(); }
