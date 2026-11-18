#include "PluginEditor.h"

using namespace vr;
using namespace vr::ui;

namespace
{
juce::String J (const char* s) { return juce::String::fromUTF8 (s); }
juce::Font font (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font mono (float size) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)); }
juce::String hz (double f)
{
    if (f >= 1000) { const double k = f / 1000.0; return juce::String (k, k >= 10 ? (std::abs (k - std::round (k)) < 0.05 ? 0 : 1) : (std::abs (k * 10 - std::round (k * 10)) < 0.05 ? 1 : 2)) + " kHz"; }
    return juce::String ((int) std::lround (f)) + " Hz";
}
float textW (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }
juce::String sdb (double d) { return (d > 0 ? "+" : "") + juce::String (d, 1) + " dB"; }
constexpr int kW = 1100, kHeader = 48, kMap = 210, kHint = 26, kGap = 8, kMargin = 12;
}

juce::Colour vr::ui::hlColour (int h)
{
    switch (h)
    {
        case hlHPF: return col::hpf; case hlLow: return col::low; case hlMid: return col::mid; case hlHigh: return col::high;
        case hlLPF: return col::lpf; case hlGain: return col::gain; case hlSC: return col::sc; case hlDyn: return col::dyn;
        case hlAnalog: return col::analog;
        default: return col::dyn;
    }
}

// ============================================================================ Look
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::outlineColourId, col::line);
    setColour (juce::ComboBox::textColourId, col::ink);
    setColour (juce::ComboBox::arrowColourId, col::inkDim);
    setColour (juce::PopupMenu::backgroundColourId, col::panel2);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff1a1408));
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::textColourOffId, col::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1a1408));
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2);
    const float d = std::min (b.getWidth(), b.getHeight()), r = d * 0.5f - 2, cx = b.getCentreX(), cy = b.getCentreY();
    // body
    g.setColour (juce::Colour (0xff2d323b)); g.fillEllipse (cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f);
    g.setColour (juce::Colour (0xff111317)); g.drawEllipse (cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f, 2.f);
    if ((bool) s.getProperties()["stepless"])   // stepless knobs get a dotted amber rim
    {
        juce::Path rim; rim.addEllipse (cx - r * 0.72f, cy - r * 0.72f, r * 1.44f, r * 1.44f);
        juce::Path d; const float pat[] = { 2.f, 3.f }; juce::PathStrokeType (1.6f).createDashedStroke (d, rim, pat, 2);
        g.setColour (col::accent); g.fillPath (d);
    }
    // track
    juce::Path tr; tr.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (juce::Colour (0xff3a404b)); g.strokePath (tr, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // value arc (bipolar knobs fill from the centre)
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float av = a0 + pos * (a1 - a0), from = bip ? (a0 + a1) * 0.5f : a0;
    if (std::abs (av - from) > 0.01f && s.isEnabled())
    {
        juce::Path va; va.addCentredArc (cx, cy, r, r, 0, std::min (from, av), std::max (from, av), true);
        g.setColour (c); g.strokePath (va, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // pointer
    juce::Path p; p.addRoundedRectangle (-1.3f, -r * 0.72f, 2.6f, r * 0.55f, 1.3f);
    g.setColour (juce::Colour (0xfff2f3f5)); g.fillPath (p, juce::AffineTransform::rotation (av).translated (cx, cy));
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool round = b.getProperties()["round"];
    const auto onCol = (bool) b.getProperties()["analog"] ? col::analog : col::accent;
    const float rad = round ? r.getHeight() * 0.5f : 4.f;
    g.setColour (on ? onCol : col::panel2); g.fillRoundedRectangle (r, rad);
    g.setColour (on ? onCol : (over ? col::inkDim : col::line)); g.drawRoundedRectangle (r, rad, 1.f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? juce::Colour (0xff1a1408) : (b.isEnabled() ? col::ink : col::inkDim.withAlpha (0.4f)));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (std::min (13.f, h * 0.55f), true); }

// ============================================================================ Knob
Knob::Knob (const juce::String& cap, int sl, int hl, HoverSink& s) : slot (sl), band (hl), caption (cap), sink (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, hl == hlNone ? juce::Colour (0xffc9ced8) : hlColour (hl));
    slider.setMouseDragSensitivity (180);
    slider.setVelocityModeParameters (1.0, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.addMouseListener (this, false);
    slider.onValueChange = [this]
    {
        repaint();
        if (slider.isMouseOverOrDragging()) sink.hover (slot, band, describe ? describe() : caption + ": " + slider.getTextFromValue (slider.getValue()));
    };
    addAndMakeVisible (slider);
}
void Knob::attach (juce::AudioProcessorValueTreeState& st, const juce::String& pid)
{
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, pid, slider);
    if (auto* p = st.getParameter (pid)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
void Knob::resized() { slider.setBounds (getLocalBounds().withHeight (54).withSizeKeepingCentre (54, 54).withY (0)); slider.getProperties().set ("stepless", stepless); }
void Knob::paint (juce::Graphics& g)
{
    const float a = isEnabled() ? 1.f : 0.3f;
    g.setColour (col::inkDim.withAlpha (a)); g.setFont (font (12.f, true));
    g.drawText (caption.toUpperCase(), 0, 54, getWidth(), 15, juce::Justification::centred);
    g.setColour (col::ink.withAlpha (a)); g.setFont (mono (11.5f));
    const auto v = slider.getValue();
    g.drawText (format ? format (v) : slider.getTextFromValue (v), 0, 69, getWidth(), 14, juce::Justification::centred);
}
void Knob::mouseEnter (const juce::MouseEvent&) { sink.hover (slot, band, describe ? describe() : caption + ": " + slider.getTextFromValue (slider.getValue())); }
void Knob::mouseExit (const juce::MouseEvent&) { if (! slider.isMouseButtonDown()) sink.hover (-1, hlNone, {}); }

// ============================================================================ FreqMap
FreqMap::FreqMap (VintageRackProcessor& p, HoverSink& s) : proc (p), sink (s), reader (p.state)
{
    for (int i = 0; i < NF; ++i) freqs[(size_t) i] = 20.0 * std::pow (1000.0, i / (double) (NF - 1));
    slots.resize ((size_t) numSlots());
    spec.fill (-100.f);
    tick();
}
juce::Rectangle<float> FreqMap::plot() const { return getLocalBounds().toFloat().withTrimmedLeft (46).withTrimmedRight (20).withTrimmedTop (34).withTrimmedBottom (22); }
float FreqMap::xOf (double f) const { auto r = plot(); return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); }
float FreqMap::yOf (double db) const { auto r = plot(); const double R = range; return r.getY() + (float) (1 - (juce::jlimit (-R, R, db) + R) / (2 * R)) * r.getHeight(); }

void FreqMap::compute (int s, SlotCurves& c)
{
    const double fs = 48000;
    const int t = reader.type (s);
    const bool on = reader.on (s);
    Pre73Params p; Fet76Params f; Opto3AParams o;
    if (t == kT73) p = reader.pre73 (s); else if (t == kT76) f = reader.fet76 (s); else if (t == kT3A) o = reader.opto3a (s);
    if (t == c.type && on == c.on && p == c.p && f == c.f && o == c.o) return;
    c.type = t; c.on = on; c.p = p; c.f = f; c.o = o;
    auto fill = [&] (Curve& dst, const std::vector<Coefs>& secs) { for (int i = 0; i < NF; ++i) { double v = 0; for (auto& k : secs) v += magDb (k, fs, freqs[(size_t) i]); dst[(size_t) i] = v; } };
    for (auto& b : c.band) b.fill (0);
    c.eqTotal.fill (0); c.analog.fill (0); c.analogIfOn.fill (0); c.sc.fill (0);
    if (t == kT73)
    {
        const auto bands = design73 (p, fs);
        for (int b = 0; b < kNumBands; ++b)
        {
            fill (c.band[(size_t) b], bands[(size_t) b].secs);
            if (p.eqIn || b == kHPF || b == kLPF) for (int i = 0; i < NF; ++i) c.eqTotal[(size_t) i] += c.band[(size_t) b][(size_t) i];
        }
        fill (c.analog, designAnalog (p, fs));
        auto q = p; q.analog = true; fill (c.analogIfOn, designAnalog (q, fs));
    }
    else if (t == kT76)
    {
        fill (c.analog, designAnalog (f, fs));
        auto q = f; q.analog = true; fill (c.analogIfOn, designAnalog (q, fs));
    }
    else if (t == kT3A)
    {
        fill (c.analog, designAnalog (o, fs));
        auto q = o; q.analog = true; fill (c.analogIfOn, designAnalog (q, fs));
        fill (c.sc, { design3aSC (o, fs) });
    }
}

void FreqMap::tick()
{
    for (int s = 0; s < numSlots(); ++s) compute (s, slots[(size_t) s]);
    const int ready = proc.specFifo.getNumReady();
    if (ready > 0)
    {
        const auto scope = proc.specFifo.read (ready);
        auto take = [&] (int start, int size) { for (int i = 0; i < size; ++i) { ring[(size_t) ringPos] = proc.specBuffer[(size_t) (start + i)]; ringPos = (ringPos + 1) % VintageRackProcessor::fftSize; } };
        take (scope.startIndex1, scope.blockSize1); take (scope.startIndex2, scope.blockSize2);
        const int N = VintageRackProcessor::fftSize;
        for (int i = 0; i < N; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % N)];
        window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        const double sr = proc.getHostRate();
        for (int i = 0; i < NF; ++i)
        {
            const int bin = juce::jlimit (1, N / 2 - 1, (int) std::lround (freqs[(size_t) i] / sr * N));
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) bin] / (N * 0.25f), -120.f);
            spec[(size_t) i] = spec[(size_t) i] * 0.7f + db * 0.3f;
        }
        hasSpec = true;
    }
    repaint();
}

void FreqMap::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (bounds, 6.f);
    g.setColour (col::line); g.drawRoundedRectangle (bounds.reduced (0.5f), 6.f, 1.f);
    g.setColour (col::ink); g.setFont (font (15.f, true)); g.drawText ("FREQUENCY MAP", 12, 8, 160, 18, juce::Justification::left);
    struct Leg { juce::String t; juce::Colour c; int style; };
    std::vector<Leg> legs;
    if (hasType (kT73)) for (auto& l : { Leg { "HPF", col::hpf, 0 }, Leg { "LOW", col::low, 0 }, Leg { "MID", col::mid, 0 }, Leg { "HIGH", col::high, 0 }, Leg { "LPF", col::lpf, 0 } }) legs.push_back (l);
    legs.push_back ({ J ("ANALOG 特性"), col::analog, 1 });
    if (hasType (kT3A)) legs.push_back ({ J ("Opto 検出感度"), col::sc, 1 });
    legs.push_back ({ J ("全体の特性"), juce::Colours::white, 0 });
    legs.push_back ({ J ("出力スペクトル"), juce::Colour (0xff5b6474), 0 });
    float lx = 170; g.setFont (font (12.f));
    for (auto& l : legs)
    {
        g.setColour (l.c);
        if (l.style) { const float dl[] = { 4, 3 }; g.drawDashedLine ({ lx, 17.5f, lx + 12, 17.5f }, dl, 2, 2.f); }
        else g.fillRoundedRectangle (lx, 12.5f, 10, 10, 2);
        g.setColour (col::inkDim); const int w = (int) textW (font (12.f), l.t);
        g.drawText (l.t, (int) lx + 15, 9, w + 4, 16, juce::Justification::left); lx += (float) w + 30;
    }

    auto r = plot();
    if (hasSpec)
    {
        juce::Path sp; sp.startNewSubPath (r.getX(), r.getBottom());
        for (int i = 0; i < NF; ++i) { const float v = juce::jlimit (-100.f, -10.f, spec[(size_t) i]); sp.lineTo (xOf (freqs[(size_t) i]), r.getBottom() - (v + 100) / 90 * r.getHeight()); }
        sp.lineTo (r.getRight(), r.getBottom()); sp.closeSubPath();
        g.setColour (juce::Colour (0xff5b6474).withAlpha (0.22f)); g.fillPath (sp);
    }
    g.setFont (mono (10.5f));
    for (double f : { 20., 50., 100., 200., 500., 1000., 2000., 5000., 10000., 20000. })
    {
        g.setColour (juce::Colour (0xff2c313a)); g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        g.setColour (juce::Colour (0xff7d8492));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xOf (f) - 20, (int) r.getBottom() + 3, 40, 14, juce::Justification::centred);
    }
    const double step = range >= 20 ? 6 : range >= 6 ? 2 : 0.5;
    for (double d = std::ceil ((-range + 1e-6) / step) * step; d < range; d += step)
    {
        g.setColour (std::abs (d) < 1e-6 ? juce::Colour (0xff4a515e) : juce::Colour (0xff2c313a)); g.drawHorizontalLine ((int) yOf (d), r.getX(), r.getRight());
        g.setColour (juce::Colour (0xff7d8492)); g.drawText ((d > 0 ? "+" : "") + juce::String (d, step < 1 ? 1 : 0), (int) r.getX() - 40, (int) yOf (d) - 7, 34, 14, juce::Justification::right);
    }
    auto pathOf = [&] (const Curve& c) { juce::Path p; for (int i = 0; i < NF; ++i) { const juce::Point<float> pt (xOf (freqs[(size_t) i]), yOf (c[(size_t) i])); i ? p.lineTo (pt) : p.startNewSubPath (pt); } return p; };
    auto fillOf = [&] (const Curve& c) { juce::Path p; p.startNewSubPath (xOf (freqs[0]), yOf (0)); for (int i = 0; i < NF; ++i) p.lineTo (xOf (freqs[(size_t) i]), yOf (c[(size_t) i])); p.lineTo (xOf (freqs[NF - 1]), yOf (0)); p.closeSubPath(); return p; };
    auto dashed = [&] (const juce::Path& p, float w, std::initializer_list<float> pat) { juce::Path d; std::vector<float> v (pat); juce::PathStrokeType (w).createDashedStroke (d, p, v.data(), (int) v.size()); return d; };

    static const juce::Colour bc[kNumBands] = { col::hpf, col::low, col::mid, col::high, col::lpf };
    Curve total {}, analogSum {}; bool anyAnalog = false;
    int n73 = 0; for (auto& c : slots) if (c.type == kT73) ++n73;
    struct Mark { int band; double f; juce::String name; int slot; };
    std::vector<Mark> marks;
    for (int s = 0; s < numSlots(); ++s)
    {
        auto& c = slots[(size_t) s];
        if (c.type == kEmpty) continue;
        if (c.on) for (int i = 0; i < NF; ++i) { total[(size_t) i] += c.analog[(size_t) i] + c.eqTotal[(size_t) i]; analogSum[(size_t) i] += c.analog[(size_t) i]; }
        if (c.on && (c.type == kT73 ? c.p.analog : c.type == kT76 ? c.f.analog : c.o.analog)) anyAnalog = true;
        if (c.type == kT73)
        {
            for (int b = 0; b < kNumBands; ++b)
            {
                auto& arr = c.band[(size_t) b];
                if (std::all_of (arr.begin(), arr.end(), [] (double v) { return std::abs (v) < 0.05; })) continue;
                const bool eqBand = b == kLow || b == kMid || b == kHigh, active = c.on && (c.p.eqIn || ! eqBand);
                const bool lit = hlSlot == s && hl == b, dim = hl != hlNone && ! lit;
                g.setColour (bc[b].withAlpha (active ? (lit ? 0.45f : dim ? 0.06f : 0.14f) : 0.04f)); g.fillPath (fillOf (arr));
                g.setColour (bc[b].withAlpha (active ? (lit ? 1.f : dim ? 0.3f : 0.7f) : 0.2f)); g.strokePath (pathOf (arr), juce::PathStrokeType (lit ? 2.5f : 1.5f));
            }
            const juce::String suffix = n73 > 1 ? juce::String::fromUTF8 ("·") + juce::String (s + 1) : juce::String();
            const double fr[] = { c.p.hpfHz(), c.p.lowHz(), c.p.midHz(), c.p.highHz(), c.p.lpfHz() };
            static const char* nm[] = { "HPF", "LOW", "MID", "HIGH", "LPF" };
            if (c.on) for (int b = 0; b < kNumBands; ++b) if (fr[b] > 0) marks.push_back ({ b, fr[b], juce::String (nm[b]) + suffix, s });
        }
        if (c.type == kT3A && (c.o.hf > 0 || (hlSlot == s && hl == hlSC)))
        {
            const bool lit = hlSlot == s && hl == hlSC;
            g.setColour (col::sc.withAlpha (c.on ? (hl != hlNone && ! lit ? 0.35f : 1.f) : 0.3f));
            g.fillPath (dashed (pathOf (c.sc), lit ? 2.5f : 1.5f, { 6, 4 }));
            if (lit) { g.setFont (font (12.f, true)); g.drawText (juce::String (s + 1) + J (" 番 Opto の検出回路の感度"), (int) xOf (3000), (int) yOf (c.sc[NF - 1]) - 22, 220, 16, juce::Justification::left); }
        }
    }
    // analog: the highlighted module alone (filled), otherwise the sum of all modules
    const bool anaLit = hl == hlAnalog && hlSlot >= 0 && slots[(size_t) hlSlot].type != kEmpty;
    if (anaLit)
    {
        auto& c = slots[(size_t) hlSlot];
        g.setColour (col::analog.withAlpha (0.3f)); g.fillPath (fillOf (c.analogIfOn));
        g.setColour (col::analog); g.fillPath (dashed (pathOf (c.analogIfOn), 2.5f, { 8, 3, 2, 3 }));
        const bool isOn = c.type == kT73 ? c.p.analog : c.type == kT76 ? c.f.analog : c.o.analog;
        g.setFont (font (12.f, true));
        g.drawText (juce::String (hlSlot + 1) + J (" 番 ") + typeTitle (c.type) + J (" のアナログ特性") + (isOn ? juce::String() : J ("（現在オフ：オンにした場合）")),
                    (int) r.getX() + 8, (int) r.getY() + 3, 520, 16, juce::Justification::left);
    }
    else if (anyAnalog)
    {
        g.setColour (col::analog.withAlpha (hl != hlNone ? 0.4f : 0.95f)); g.fillPath (dashed (pathOf (analogSum), 1.8f, { 8, 3, 2, 3 }));
    }
    g.setColour (juce::Colours::white.withAlpha (anaLit ? 0.35f : 1.f)); g.strokePath (pathOf (total), juce::PathStrokeType (2.5f));
    auto totalAt = [&] (double f) { int i = 0; while (i < NF - 1 && freqs[(size_t) i] < f) ++i; return total[(size_t) i]; };
    for (auto& m : marks)
    {
        const float x = xOf (m.f), y = yOf (totalAt (m.f)); const bool lit = hlSlot == m.slot && hl == m.band; const float rr = lit ? 7.f : 5.f;
        const float a = (hl != hlNone && ! lit && hl != hlAnalog) ? 0.35f : 1.f;
        g.setColour (bc[m.band].withAlpha (a)); g.fillEllipse (x - rr, y - rr, rr * 2, rr * 2);
        g.setColour (col::bg); g.drawEllipse (x - rr, y - rr, rr * 2, rr * 2, 2.f);
        if (lit || hl == hlNone)
        {
            g.setColour (bc[m.band]); g.setFont (lit ? font (12.f, true) : mono (10.5f));
            const float ty = y > r.getY() + r.getHeight() * 0.6f ? y - 22 : y + 8;
            g.drawText (m.name + " " + (m.f >= 1000 ? juce::String (m.f / 1000.0, 1) + "k" : juce::String ((int) std::lround (m.f))), (int) x - 55, (int) ty, 110, 14, juce::Justification::centred);
        }
    }
    if (anyAnalog && range >= 20 && ! anaLit)
    {
        g.setColour (col::inkDim); g.setFont (font (11.5f));
        g.drawText (J ("ANALOG の帯域変化は ±1 dB 前後です。縦軸 ±2 dB で拡大して見られます"), (int) r.getX() + 8, (int) r.getY() + 3, 520, 16, juce::Justification::left);
    }
}

void FreqMap::mouseMove (const juce::MouseEvent& e)
{
    auto r = plot(); const double f = 20.0 * std::pow (1000.0, (e.position.x - r.getX()) / r.getWidth());
    if (f < 20 || f > 20000) return;
    int bs = -1, bb = hlNone; double bd = 1e9;
    for (int s = 0; s < numSlots(); ++s)
    {
        auto& c = slots[(size_t) s];
        if (c.type != kT73 || ! c.on) continue;
        const double fr[] = { c.p.hpfHz(), c.p.lowHz(), c.p.midHz(), c.p.highHz(), c.p.lpfHz() };
        for (int b = 0; b < kNumBands; ++b) if (fr[b] > 0) { const double d = std::abs (std::log (f / fr[b])); if (d < bd) { bd = d; bs = s; bb = b; } }
    }
    if (bs < 0) return;
    static const char* names[] = { "HPF つまみ", "LOW の FREQ / GAIN", "MID の FREQ / GAIN", "HIGH の FREQ / GAIN", "LPF つまみ（Classic+）" };
    sink.hover (bs, bb, (f >= 1000 ? juce::String (f / 1000, 2) + " kHz" : juce::String ((int) f) + " Hz") + J (" 付近を動かすのは → ") + juce::String (bs + 1) + J (" 番の Pre / EQ の ") + J (names[bb]));
}
void FreqMap::mouseExit (const juce::MouseEvent&) { sink.hover (-1, hlNone, {}); }

// ============================================================================ DynDisplay
void DynDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::panel2); g.fillRoundedRectangle (b, 6.f);
    auto c = b.reduced (6).removeFromLeft (b.getHeight() - 12);
    const double lo = -48, hi = 0;
    auto X = [&] (double d) { return c.getX() + (float) ((d - lo) / (hi - lo)) * c.getWidth(); };
    auto Y = [&] (double d) { return c.getBottom() - (float) ((d - lo) / (hi - lo)) * c.getHeight(); };
    g.setColour (col::line);
    for (double d = lo; d <= hi; d += 12) { g.drawVerticalLine ((int) X (d), c.getY(), c.getBottom()); g.drawHorizontalLine ((int) Y (d), c.getX(), c.getRight()); }
    { juce::Path dl; dl.startNewSubPath (X (lo), Y (lo)); dl.lineTo (X (hi), Y (hi)); juce::Path d; const float pat[] = { 4, 4 }; juce::PathStrokeType (1.f).createDashedStroke (d, dl, pat, 2); g.fillPath (d); }
    juce::Path curve;
    vr::Fet76 f; vr::Opto3A o;
    if (type == kT76) { f.prepare (48000); f.setParams (reader.fet76 (slot)); } else { o.prepare (48000); o.setParams (reader.opto3a (slot)); }
    for (int i = 0; i <= 96; ++i)
    {
        const double xin = lo + i / 96.0 * (hi - lo);
        const double out = type == kT76 ? xin - f.gain (xin + f.inputDb()) : xin - o.gain (xin + o.scDriveDb());
        const juce::Point<float> pt (X (xin), Y (juce::jlimit (lo, hi, out)));
        i ? curve.lineTo (pt) : curve.startNewSubPath (pt);
    }
    g.setColour (col::dyn); g.strokePath (curve, juce::PathStrokeType (2.f));
    auto t = b.reduced (6).withTrimmedLeft (c.getWidth() + 12);
    g.setColour (col::inkDim); g.setFont (mono (11.f)); g.drawText ("GAIN REDUCTION", t.removeFromTop (16), juce::Justification::left);
    g.setColour (col::ink); g.setFont (mono (24.f)); g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (gr, 1), t.removeFromTop (32), juce::Justification::left);
    auto bar = t.removeFromTop (10);
    g.setColour (juce::Colour (0xff111317)); g.fillRoundedRectangle (bar, 2);
    const float w = juce::jlimit (0.f, 1.f, gr / 20.f) * bar.getWidth();
    g.setColour (col::accent); g.fillRoundedRectangle (bar.withLeft (bar.getRight() - w), 2);
    g.setColour (col::inkDim); g.setFont (mono (10.f)); g.drawText ("dB  (0 .. 20)", t.removeFromTop (16), juce::Justification::left);
}

// ============================================================================ circuit descriptions (hint line)
namespace
{
const char* compDesc (int type, int which, int idx)
{
    static const char* d73[3][4] = {
        { "ヴィンテージのニッケルコア。低域が太く、押し込むと豊かに歪む", "中期の製品。やや控えめ", "現行の再生産品。整っていてクリーン寄り", "トランスなし。帯域のクセも歪みもほぼなし" },
        { "ヴィンテージ品。偶数倍音が多く、ノイズもやや多い", "現行品。標準的な倍音", "ローノイズで倍音少なめ、余裕のある回路", "" },
        { "ヴィンテージ出力トランス。高域が少し丸い", "現行の出力トランス", "トランスなし", "" } };
    static const char* d76[3][5] = {
        { "選んだモデルの標準部品", "初期型の FET。標準的な歪み", "後期型の FET。歪みが少なめ", "選別なしの古い FET。歪みが多く荒い", "選別品。クリーン" },
        { "選んだモデルの標準部品", "初期型のニッケルコア。低域が太く、高域が早めに丸まる", "後期のスチールコア。標準的", "トランスなし", "" },
        { "選んだモデルの標準部品", "オリジナル構成（Class-A＋出力トランス）", "明るく速い（後期型系）", "改造した高ゲイン仕様。歪み・ノイズ多め", "" } };
    static const char* d3a[3][3] = {
        { "標準の光学セル", "古いセル。反応が遅く、リリースが長くなる", "改造の高速セル。反応が速く、リリースも短い" },
        { "オリジナルのディスクリート回路。中域が前に出る", "後期の IC 回路。クリーン", "" },
        { "オリジナルの出力トランス", "現行の出力トランス", "トランスなし" } };
    which = juce::jlimit (0, 2, which);
    if (type == kT73) return d73[which][juce::jlimit (0, 3, idx)];
    if (type == kT76) return d76[which][juce::jlimit (0, 4, idx)];
    return d3a[which][juce::jlimit (0, 2, idx)];
}
const char* knobDesc (const juce::String& key)
{
    if (key == "bias") return "BIAS：トランジスタのバイアス。プラスで偶数倍音（太さ・甘さ）が増え、マイナスで減ります";
    if (key == "headroom") return "HEADROOM：電源電圧（18〜30 V）。下げるほど早く飽和して歪みやすくなります";
    if (key == "iron") return "IRON：トランス鉄心の飽和量（0〜200 %）。低域ほど強く歪みます。0 % でトランスの歪みなし";
    if (key == "age") return "AGE：経年劣化・部品の個体差。高域がくすみ、歪みとノイズが増え、左右の EQ 周波数が少しずれます";
    if (key == "noise") return "NOISE：ヒス（サー）と電源ハム（50 Hz）の量。0 でなし";
    if (key == "detector") return "DETECTOR：検出回路のプログラム依存度。上げるほど、長く圧縮したあとのリリースが遅くなります";
    return "MEMORY：光学セルの記憶効果。上げるほど、長く圧縮したあとのリリースが遅くなります";
}
}

// ============================================================================ HarmDisplay
void HarmDisplay::refresh()
{
    bool changed = ! valid;
    if (type == kT73) { auto p = reader.pre73 (slot); changed |= p != p73; p73 = p; }
    else if (type == kT76) { auto p = reader.fet76 (slot); changed |= p != p76; p76 = p; }
    else { auto p = reader.opto3a (slot); changed |= p != p3a; p3a = p; }
    if (! changed) return;
    valid = true;
    for (int k = 0; k < 2; ++k)
    {
        const double f = k ? 1000.0 : 100.0;
        auto& h = k ? h1k : h100;
        if (type == kT73) h = measureHarmonics<Pre73> (p73, 48000, f, -12);
        else if (type == kT76) h = measureHarmonics<Fet76> (p76, 48000, f, -12);
        else h = measureHarmonics<Opto3A> (p3a, 48000, f, -12);
    }
    repaint();
}
void HarmDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::panel2); g.fillRoundedRectangle (b, 6.f);
    g.setFont (mono (10.5f)); g.setColour (col::inkDim);
    g.drawText (J ("倍音（入力 −12 dBFS）"), 8, 4, 200, 14, juce::Justification::left);
    g.setColour (col::low); g.drawText (J ("偶数"), getWidth() - 74, 4, 30, 14, juce::Justification::right);
    g.setColour (col::lpf); g.drawText (J ("奇数"), getWidth() - 40, 4, 30, 14, juce::Justification::right);
    const float top = 40, bottom = (float) getHeight() - 18, lo = -80;
    for (int k = 0; k < 2; ++k)
    {
        const auto& h = k ? h1k : h100;
        const float x0 = 10 + (float) k * (getWidth() - 20) * 0.5f, bw = 13, gap = 6;
        g.setColour (col::ink); g.setFont (mono (10.5f));
        g.drawText (juce::String (k ? "1 kHz" : "100 Hz") + "  THD " + juce::String (h.thd, h.thd < 10 ? 2 : 1) + " %", (int) x0, 20, 150, 14, juce::Justification::left);
        for (int i = 0; i < 6; ++i)
        {
            const float x = x0 + (float) i * (bw + gap), t = juce::jlimit (0.f, 1.f, ((float) h.h[(size_t) i] - lo) / -lo), y = bottom - t * (bottom - top);
            g.setColour (juce::Colour (0xff2c313a)); g.fillRect (x, top, bw, bottom - top);
            g.setColour (i % 2 == 0 ? col::low : col::lpf); g.fillRect (x, y, bw, bottom - y);
            g.setColour (col::inkDim); g.setFont (mono (9.f));
            g.drawText ("H" + juce::String (i + 2), (int) x - 4, (int) bottom + 2, (int) bw + 8, 12, juce::Justification::centred);
        }
    }
}

// ============================================================================ ModulePanel
ModulePanel::ModulePanel (::VintageRackEditor& e, VintageRackProcessor& p, HoverSink& s, int sl, int ty, const juce::String& sub)
    : slot (sl), type (ty), editor (e), proc (p), sink (s), subtitle (sub)
{
    power.getProperties().set ("round", true);
    toggle (power, id::on);
    circuitBtn.setClickingTogglesState (true); circuitBtn.getProperties().set ("analog", true);
    circuitBtn.setToggleState (editor.isCircuitOpen (slot), juce::dontSendNotification);
    circuitBtn.onClick = [this] { editor.setCircuitOpen (slot, circuitBtn.getToggleState()); };
    circuitBtn.onStateChange = [this] { if (circuitBtn.isOver()) sink.hover (slot, hlAnalog, J ("回路：部品の種類や BIAS・HEADROOM・IRON などの挙動を編集します。倍音のグラフで汚れ具合が見えます")); };
    addAndMakeVisible (circuitBtn);
    buildCircuit();
    if (isRack())
    {
        presetBox.setTextWhenNothingSelected (J ("プリセット…"));
        for (int i = 0; i < p.presets.numModule (ty); ++i) presetBox.addItem (p.presets.moduleName (ty, i), i + 1);
        presetBox.onChange = [this]
        {
            const int idx = presetBox.getSelectedItemIndex(); if (idx < 0) return;
            const auto name = presetBox.getText();
            presetBox.setSelectedId (0, juce::dontSendNotification);
            juce::MessageManager::callAsync ([&pr = proc, &sk = sink, s = slot, idx, name]
            {
                pr.applyModulePreset (s, idx);
                sk.hover (-1, hlNone, juce::String (s + 1) + J (" 番にプリセット「") + name + J ("」を読み込みました"));
            });
        };
        addAndMakeVisible (presetBox);
    }
    if (isRack())
    {
        for (auto* b : { &up, &down, &dupBtn, &delBtn }) addAndMakeVisible (*b);
        up.onClick = [this] { proc.moveSlot (slot, slot - 1); };
        down.onClick = [this] { proc.moveSlot (slot, slot + 1); };
        dupBtn.onClick = [this] { proc.duplicateSlot (slot); };
        delBtn.onClick = [this] { juce::MessageManager::callAsync ([&pr = proc, s = slot] { pr.removeSlot (s); }); };
    }
}
juce::TextButton& ModulePanel::toggle (juce::TextButton& b, const char* suffix)
{
    b.setClickingTogglesState (true); addAndMakeVisible (b);
    btnAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, pid (suffix), b));
    return b;
}
bool ModulePanel::circuitOpen() const { return editor.isCircuitOpen (slot); }

void ModulePanel::buildCircuit()
{
    using namespace id;
    struct KDef { const char* suffix; const char* key; const char* caption; };
    std::vector<KDef> kd;
    if (type == kT73)
    {
        cSuffix = { c73InX, c73Amp, c73OutX }; cLabel = { J ("入力トランス"), J ("増幅回路"), J ("出力トランス") };
        for (auto& x : circuit::in73) cBox[0].addItem (x.name, cBox[0].getNumItems() + 1);
        for (auto& x : circuit::amp73) cBox[1].addItem (x.name, cBox[1].getNumItems() + 1);
        for (auto& x : circuit::out73) cBox[2].addItem (x.name, cBox[2].getNumItems() + 1);
        kd = { { c73Bias, "bias", "Bias" }, { c73Headroom, "headroom", "Headroom" }, { c73Iron, "iron", "Iron" }, { c73Age, "age", "Age" }, { c73Noise, "noise", "Noise" } };
    }
    else if (type == kT76)
    {
        cSuffix = { c76Fet, c76InX, c76Out }; cLabel = { "FET", J ("入力トランス"), J ("出力段") };
        for (auto& x : circuit::fet76) cBox[0].addItem (x.name, cBox[0].getNumItems() + 1);
        for (auto& x : circuit::in76) cBox[1].addItem (x.name, cBox[1].getNumItems() + 1);
        for (auto& x : circuit::out76) cBox[2].addItem (x.name, cBox[2].getNumItems() + 1);
        kd = { { c76Bias, "bias", "Bias" }, { c76Headroom, "headroom", "Headroom" }, { c76Detector, "detector", "Detector" }, { c76Iron, "iron", "Iron" }, { c76Age, "age", "Age" }, { c76Noise, "noise", "Noise" } };
    }
    else
    {
        cSuffix = { c3aCell, c3aAmp, c3aOutX }; cLabel = { J ("光学セル"), J ("出力アンプ"), J ("出力トランス") };
        for (auto& x : circuit::cell3a) cBox[0].addItem (x.name, cBox[0].getNumItems() + 1);
        for (auto& x : circuit::amp3a) cBox[1].addItem (x.name, cBox[1].getNumItems() + 1);
        for (auto& x : circuit::out3a) cBox[2].addItem (x.name, cBox[2].getNumItems() + 1);
        kd = { { c3aMemory, "memory", "Memory" }, { c3aHeadroom, "headroom", "Headroom" }, { c3aIron, "iron", "Iron" }, { c3aAge, "age", "Age" }, { c3aNoise, "noise", "Noise" } };
    }
    for (size_t k = 0; k < 3; ++k)
    {
        addChildComponent (cBox[k]);
        cAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, pid (cSuffix[k]), cBox[k]));
        cBox[k].addMouseListener (this, false);
    }
    for (auto& d : kd)
    {
        auto k = std::make_unique<Knob> (d.caption, slot, hlAnalog, sink);
        k->attach (proc.state, pid (d.suffix));
        const juce::String key (d.key);
        k->format = [key] (double v)
        {
            if (key == "bias") return (v > 0 ? "+" : "") + juce::String ((int) v);
            if (key == "headroom") return juce::String (v, 1) + " V";
            if (key == "iron") return juce::String ((int) v) + " %";
            if (key == "age") return v > 0 ? juce::String ((int) v) + " %" : juce::String ("NEW");
            if (key == "noise") return v > 0 ? juce::String ((int) v) : juce::String ("OFF");
            return juce::String ((int) v);
        };
        auto* kp = k.get();
        k->describe = [key, kp] { return J (knobDesc (key)) + J ("（現在 ") + kp->format (kp->slider.getValue()) + J ("）"); };
        addChildComponent (*k);
        cKnobs.push_back (std::move (k));
    }
    harm = std::make_unique<HarmDisplay> (proc, slot, type);
    addChildComponent (*harm);
    addChildComponent (circuitReset);
    circuitReset.onClick = [this]
    {
        for (auto& b : cBox) juce::ignoreUnused (b);
        for (size_t k = 0; k < 3; ++k) if (auto* prm = proc.state.getParameter (pid (cSuffix[k]))) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
        for (auto& k : cKnobs) k->slider.setValue (k->slider.getDoubleClickReturnValue(), juce::sendNotificationSync);
        sink.hover (slot, hlAnalog, J ("回路を初期値に戻しました"));
    };
}
void ModulePanel::layoutCircuit (juce::Rectangle<int> a)
{
    circuitArea = a;
    const bool open = circuitOpen();
    for (auto& b : cBox) b.setVisible (open);
    for (auto& k : cKnobs) k->setVisible (open);
    harm->setVisible (open); circuitReset.setVisible (open);
    if (! open) return;
    auto r = a.reduced (0, 4);
    auto head = r.removeFromTop (26);
    circuitReset.setBounds (head.withTrimmedLeft (300).removeFromLeft (110).reduced (0, 1));
    r.removeFromTop (6);
    harm->setBounds (r.removeFromRight (270).withHeight (124));
    auto comps = r.removeFromLeft (300);
    for (size_t k = 0; k < 3; ++k) cBox[k].setBounds (comps.getX() + 88, comps.getY() + 4 + (int) k * 36, 205, 28);
    r.removeFromLeft (6);
    for (auto& k : cKnobs) k->setBounds (r.removeFromLeft (66).withHeight (86).translated (0, 8));
}
void ModulePanel::tickCircuit()
{
    circuitBtn.setToggleState (circuitOpen(), juce::dontSendNotification);
    if (! circuitOpen()) return;
    ParamReader rd (proc.state);
    if (type == kT76)   // show which part "Model standard" currently means
    {
        const auto fp = rd.fet76 (slot);
        auto stdName = [] (const char* n) { return J ("モデル標準（") + n + J ("）"); };
        Fet76Params q = fp; q.fet = 0; q.inX = 0; q.out = 0;
        cBox[0].changeItemText (1, stdName (circuit::fet76[q.fetIdx()].name));
        cBox[1].changeItemText (1, stdName (circuit::in76[q.inIdx()].name));
        cBox[2].changeItemText (1, stdName (circuit::out76[q.outIdx()].name));
        for (auto& b : cBox) if (b.getSelectedId() == 1) b.setText (b.getItemText (0), juce::dontSendNotification);
    }
    const bool analog = rd.b (slot, type == kT73 ? id::p73Analog : type == kT76 ? id::f76Analog : id::o3Analog);
    for (auto& b : cBox) b.setAlpha (analog ? 1.f : 0.4f);
    for (auto& k : cKnobs) k->setAlpha (analog ? 1.f : 0.4f);
    harm->refresh();
}
void ModulePanel::mouseEnter (const juce::MouseEvent& e)
{
    for (size_t k = 0; k < 3; ++k)
        if (e.eventComponent == &cBox[k])
        {
            const int idx = juce::jmax (0, cBox[k].getSelectedItemIndex());
            sink.hover (slot, hlAnalog, cLabel[k] + J ("：") + cBox[k].getText() + J (" — ") + J (compDesc (type, (int) k, idx)));
        }
}

void ModulePanel::addHeaderToggles (const char* analogSuffix, const char* contSuffix)
{
    analogBtn.getProperties().set ("analog", true);
    toggle (analogBtn, analogSuffix);
    auto desc = [this, analogSuffix]
    {
        ParamReader rd (proc.state);
        const bool on = rd.b (slot, analogSuffix);
        juce::String d;
        if (type == kT73) d = J ("入出力トランスと Class-A 回路：60 Hz 付近がわずかに膨らみ（+0.7 dB）、18 kHz 以上がなだらかに下がり、レベルを上げるほど倍音が増えます");
        else if (type == kT76)
        {
            const int v = rd.i (slot, id::f76Variant);
            d = v == 0 ? J ("Early：低域が太く（45 Hz +0.6 dB）高域が早めに丸まる（11 kHz〜 −1.2 dB）。倍音多め")
              : v == 2 ? J ("Late：5 kHz 付近が明るく（+0.6 dB）、倍音少なめでクリーン寄り")
                       : J ("Low-noise：3 kHz にわずかな張り、15 kHz〜 −0.6 dB。標準的な倍音");
        }
        else d = J ("ソリッドステート回路：2.2 kHz 付近が前に出て（+0.8 dB）、80 Hz 以下と 14 kHz 以上がわずかに控えめ。軽い倍音");
        return (on ? J ("ANALOG オン：") : J ("ANALOG オフ（クリーン）。オンにすると → ")) + d;
    };
    analogBtn.onStateChange = [this, desc] { if (analogBtn.isOver()) sink.hover (slot, hlAnalog, desc()); else if (editor.highlightBand() == hlAnalog) sink.hover (-1, hlNone, {}); };
    if (contSuffix != nullptr)
    {
        hasCont = true;
        toggle (contBtn, contSuffix);
        contBtn.onStateChange = [this] { if (contBtn.isOver()) sink.hover (-1, hlNone, J ("無段階：周波数つまみを連続可変にします（点線の縁のつまみ）。実機にはない拡張機能です")); };
    }
}
juce::Rectangle<int> ModulePanel::placeGroup (juce::Rectangle<int>& row, int width, const juce::String& name, int hl)
{
    auto r = row.removeFromLeft (width); row.removeFromLeft (6);
    groups.push_back ({ r, name, hl });
    return r.reduced (4, 6);
}
void ModulePanel::resized()
{
    groups.clear();
    auto r = getLocalBounds();
    auto rail = r.removeFromLeft (railW);
    up.setBounds (rail.getCentreX() - 14, 62, 28, 22);
    down.setBounds (rail.getCentreX() - 14, 90, 28, 22);
    r.reduce (14, 8);
    auto head = r.removeFromTop (32);
    power.setBounds (head.removeFromLeft (30).withSizeKeepingCentre (30, 30));
    if (isRack()) { delBtn.setBounds (head.removeFromRight (52).withSizeKeepingCentre (52, 24)); head.removeFromRight (6); dupBtn.setBounds (head.removeFromRight (52).withSizeKeepingCentre (52, 24)); head.removeFromRight (10); }
    if (hasCont) { contBtn.setBounds (head.removeFromRight (64).withSizeKeepingCentre (64, 26)); head.removeFromRight (6); }
    analogBtn.setBounds (head.removeFromRight (78).withSizeKeepingCentre (78, 26)); head.removeFromRight (6);
    circuitBtn.setBounds (head.removeFromRight (56).withSizeKeepingCentre (56, 26)); head.removeFromRight (10);
    if (variant.getNumItems() > 0) { variant.setBounds (head.removeFromRight (170).withSizeKeepingCentre (170, 28)); head.removeFromRight (6); }
    if (isRack()) presetBox.setBounds (head.removeFromRight (180).withSizeKeepingCentre (180, 28));
    subtitleRight = head.getRight();
    r.removeFromTop (8);
    layoutControls (r.removeFromTop (100));
    r.removeFromTop (8);
    layoutCircuit (r);
}
void ModulePanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (b, 6.f);
    g.setColour (dragging ? col::accent : col::line); g.drawRoundedRectangle (b.reduced (0.5f), 6.f, dragging ? 2.f : 1.f);
    auto rail = b.removeFromLeft ((float) railW);
    g.setColour (col::panel2); g.fillRoundedRectangle (rail, 6.f); g.fillRect (rail.withTrimmedLeft (6));
    g.setColour (col::accent); g.setFont (font (20.f, true));
    g.drawText (juce::String (slot + 1), rail.removeFromTop (34).toNearestInt(), juce::Justification::centred);
    if (isRack())
    {
        g.setColour (col::inkDim);
        for (int yy = 0; yy < 3; ++yy) for (int xx = 0; xx < 2; ++xx) g.fillEllipse ((float) railW * 0.5f - 5 + (float) xx * 7, 38.f + (float) yy * 6, 3.2f, 3.2f);
    }
    const bool on = ParamReader (proc.state).b (slot, id::on);
    g.setColour (col::ink.withAlpha (on ? 1.f : 0.45f)); g.setFont (font (19.f, true));
    g.drawText (typeTitle (type), railW + 54, 8, 260, 32, juce::Justification::left);
    g.setColour (col::inkDim); g.setFont (font (12.f));
    {
        const int sx = railW + 54 + (int) textW (font (19.f, true), typeTitle (type)) + 14;
        if (subtitleRight - sx > 60) g.drawFittedText (subtitle, sx, 8, subtitleRight - sx - 8, 32, juce::Justification::centredLeft, 1, 1.0f);
    }
    if (circuitOpen() && ! circuitArea.isEmpty())
    {
        const float y = (float) circuitArea.getY();
        const float dash[] = { 4.f, 4.f };
        g.setColour (col::line); g.drawDashedLine ({ (float) circuitArea.getX(), y, (float) circuitArea.getRight(), y }, dash, 2, 1.f);
        g.setColour (col::analog); g.setFont (font (13.f, true));
        g.drawText ("CIRCUIT", circuitArea.getX(), circuitArea.getY() + 6, 80, 18, juce::Justification::left);
        g.setColour (col::inkDim); g.setFont (font (11.5f));
        g.drawText (J ("部品を替えると倍音と帯域のクセ（点線）が変わります"), circuitArea.getX() + 70, circuitArea.getY() + 6, 230, 18, juce::Justification::left);
        g.setFont (font (12.f));
        for (size_t k = 0; k < 3; ++k)
            if (cBox[k].isVisible()) g.drawText (cLabel[k], circuitArea.getX(), cBox[k].getY(), 86, cBox[k].getHeight(), juce::Justification::centredLeft);
    }
    const int hlS = editor.highlightSlot(), hlB = editor.highlightBand();
    for (auto& gr : groups)
    {
        auto gb = gr.r.toFloat();
        if (gr.hl != hlNone)
        {
            const auto c = hlColour (gr.hl); const bool lit = hlS == slot && hlB == gr.hl;
            g.setColour (c.withAlpha (lit ? 0.16f : 0.07f)); g.fillRoundedRectangle (gb, 6.f);
            g.setColour (c.withAlpha (lit ? 1.f : 0.35f)); g.drawRoundedRectangle (gb.reduced (0.5f), 6.f, 1.f);
        }
        if (gr.name.isNotEmpty())
        {
            const int w = (int) textW (font (11.f, true), gr.name) + 8;
            g.setFont (font (11.f, true));
            g.setColour (col::panel); g.fillRect (gr.r.getX() + 8, gr.r.getY() - 7, w, 13);
            g.setColour (gr.hl != hlNone ? hlColour (gr.hl) : col::inkDim);
            g.drawText (gr.name, gr.r.getX() + 8, gr.r.getY() - 7, w, 13, juce::Justification::centred);
        }
    }
}
void ModulePanel::mouseDown (const juce::MouseEvent& e) { if (e.eventComponent != this) return; dragging = isRack() && e.x < railW && e.y > 30 && e.y < 60; repaint(); }
void ModulePanel::mouseDrag (const juce::MouseEvent& e) { if (dragging) editor.dragPanel (*this, e.getEventRelativeTo (getParentComponent()).y, false); }
void ModulePanel::mouseUp (const juce::MouseEvent& e) { if (dragging) { dragging = false; editor.dragPanel (*this, e.getEventRelativeTo (getParentComponent()).y, true); repaint(); } }

// ============================================================================ Pre73Panel
Pre73Panel::Pre73Panel (::VintageRackEditor& e, VintageRackProcessor& p, HoverSink& s, int sl)
    : ModulePanel (e, p, s, sl, kT73, J ("マイクプリ＋3バンド・インダクター EQ")),
      gain ("Gain", sl, hlGain, s), hpf ("HPF", sl, hlHPF, s), lowF ("Freq", sl, hlLow, s), lowG ("Gain", sl, hlLow, s), midF ("Freq", sl, hlMid, s), midG ("Gain", sl, hlMid, s),
      highF ("Freq", sl, hlHigh, s), highG ("Gain", sl, hlHigh, s), lpf ("LPF", sl, hlLPF, s), out ("Output", sl, hlNone, s),
      hpfC ("HPF", sl, hlHPF, s), lowC ("Freq", sl, hlLow, s), midC ("Freq", sl, hlMid, s), highC ("Freq", sl, hlHigh, s), lpfC ("LPF", sl, hlLPF, s)
{
    using namespace id;
    variant.addItemList ({ "Classic", "Classic+ (Hi-Q / LPF)" }, 1); addAndMakeVisible (variant);
    variantAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.state, pid (p73Variant), variant);
    addHeaderToggles (p73Analog, p73Cont);
    auto P = [&p, sl] { return ParamReader (p.state).pre73 (sl); };

    // Gain: its range depends on MIC/LINE, so it is synced by hand instead of an attachment
    const juce::String gainId = pid (p73Gain);
    gain.slider.setRange (0, 6, 1);
    gain.slider.onDragStart = [&p, gainId] { p.state.getParameter (gainId)->beginChangeGesture(); };
    gain.slider.onDragEnd = [&p, gainId] { p.state.getParameter (gainId)->endChangeGesture(); };
    auto base = gain.slider.onValueChange;
    gain.slider.onValueChange = [this, &p, gainId, base]
    {
        auto* prm = p.state.getParameter (gainId);
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) gain.slider.getValue()));
        if (base) base();
    };
    gain.format = [P] (double) { const auto q = P(); return (q.gainDb > 0 ? "+" : "") + juce::String (q.gainDb) + " dB"; };
    gain.describe = [P] { const auto q = P(); return J ("GAIN：プリアンプの増幅量（5 dB ステップ）。") + (q.analog ? J ("上げるほどトランスが飽和して倍音が増えます。") : J ("ANALOG オフのため倍音は加わりません。")) + J ("現在 ") + (q.mic ? "MIC " : "LINE ") + juce::String (q.gainDb) + " dB"; };
    gain.slider.setDoubleClickReturnValue (true, 2);

    hpf.attach (p.state, pid (p73Hpf));
    hpf.describe = [P] { const auto q = P(); return q.hpfHz() > 0 ? J ("HPF：") + hz (q.hpfHz()) + J (" より下を 18 dB/oct でカット") : J ("HPF：オフ（50 / 80 / 160 / 300 Hz から選択）"); };
    lowF.attach (p.state, pid (p73LowF));
    lowF.describe = [P] { return J ("LOW FREQ：低域シェルフを 35 / 60 / 110 / 220 Hz から選択。現在 ") + hz (P().lowHz()); };
    lowG.attach (p.state, pid (p73LowG)); lowG.format = sdb;
    lowG.describe = [P] { const auto q = P(); return J ("LOW：") + hz (q.lowHz()) + J (" 以下を ") + sdb (q.lowGain) + J ("（シェルフ）"); };
    midF.attach (p.state, pid (p73MidF));
    midF.describe = [P] { return J ("MID FREQ：中域ピークの中心を 360 Hz〜7.2 kHz の 6 点から選択。現在 ") + hz (P().midHz()); };
    midG.attach (p.state, pid (p73MidG)); midG.format = sdb;
    midG.describe = [P] { const auto q = P(); return J ("MID：") + hz (q.midHz()) + J (" を中心に ") + sdb (q.midGain) + J ("（ベル）") + (q.isPlus() && ! q.hiQ ? J ("・Classic+ の通常 Q では ±12 dB まで") : juce::String()); };
    highF.attach (p.state, pid (p73HighF));
    highF.format = [P] (double) { return hz (P().highHz()); };
    highF.describe = [] { return J ("HIGH FREQ：Classic+ は高域シェルフを 10 / 12 / 16 kHz から選択（Classic は 12 kHz 固定）"); };
    highG.attach (p.state, pid (p73HighG)); highG.format = sdb;
    highG.describe = [P] { const auto q = P(); return J ("HIGH：") + hz (q.highHz()) + J (" 以上を ") + sdb (q.highGain) + J ("（シェルフ）"); };
    lpf.attach (p.state, pid (p73Lpf));
    lpf.describe = [P] { const auto q = P(); return q.lpfHz() > 0 ? J ("LPF（Classic+）：") + hz (q.lpfHz()) + J (" より上を 18 dB/oct でカット") : J ("LPF（Classic+）：オフ"); };
    out.attach (p.state, pid (p73Out)); out.format = sdb;
    out.describe = [P] { return J ("OUTPUT：出力レベル ") + sdb (P().output); };

    // stepless (無段階) frequency knobs
    for (auto* k : { &hpfC, &lowC, &midC, &highC, &lpfC }) k->stepless = true;
    hpfC.attach (p.state, pid (p73HpfHz));  hpfC.format = [] (double v) { return v < spec::hpfOffBelow ? juce::String ("OFF") : hz (v); };
    hpfC.describe = [P] { const auto q = P(); return q.hpfHz() > 0 ? J ("HPF（無段階）：") + hz (q.hpfHz()) + J (" より下を 18 dB/oct でカット。左端で OFF") : J ("HPF（無段階）：OFF"); };
    lowC.attach (p.state, pid (p73LowHz));  lowC.format = [] (double v) { return hz (v); };
    lowC.describe = [P] { return J ("LOW FREQ（無段階 20〜500 Hz）：現在 ") + hz (P().lowHz()); };
    midC.attach (p.state, pid (p73MidHz));  midC.format = [] (double v) { return hz (v); };
    midC.describe = [P] { return J ("MID FREQ（無段階 150 Hz〜10 kHz）：現在 ") + hz (P().midHz()); };
    highC.attach (p.state, pid (p73HighHz)); highC.format = [] (double v) { return hz (v); };
    highC.describe = [P] { return J ("HIGH FREQ（無段階 3〜20 kHz）：現在 ") + hz (P().highHz()); };
    lpfC.attach (p.state, pid (p73LpfHz));  lpfC.format = [] (double v) { return v >= spec::lpfOffAbove ? juce::String ("OFF") : hz (v); };
    lpfC.describe = [P] { const auto q = P(); return q.lpfHz() > 0 ? J ("LPF（無段階・Classic+）：") + hz (q.lpfHz()) + J (" より上をカット。右端で OFF") : J ("LPF（無段階・Classic+）：OFF"); };

    for (auto* k : { &gain, &hpf, &lowF, &lowG, &midF, &midG, &highF, &highG, &lpf, &out, &hpfC, &lowC, &midC, &highC, &lpfC }) addAndMakeVisible (*k);
    toggle (mic, p73Mic); toggle (phase, p73Phase); toggle (hiQ, p73HiQ); toggle (eqIn, p73EqIn);
    mic.onClick = [&p, gainId, micId = pid (p73Mic)]
    {
        const bool m = p.state.getRawParameterValue (micId)->load() > 0.5f;   // jump to unity gain for that input
        auto* prm = p.state.getParameter (gainId);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (m ? 6.f : 2.f)); prm->endChangeGesture();
    };
    contBtn.onClick = [this, &p, sl]
    {
        // entering stepless mode: start from the current stepped frequencies
        ParamReader rd (p.state);
        if (! rd.b (sl, p73Cont)) return;
        auto q = rd.pre73 (sl); q.cont = false;
        auto setHz = [&] (const char* suf, double v) { auto* prm = p.state.getParameter (pid (suf)); prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); prm->endChangeGesture(); };
        setHz (p73HpfHz, q.hpfHz() > 0 ? q.hpfHz() : spec::contHpf[0]); setHz (p73LowHz, q.lowHz()); setHz (p73MidHz, q.midHz());
        setHz (p73HighHz, q.highHz()); setHz (p73LpfHz, q.lpfHz() > 0 ? q.lpfHz() : spec::contLpf[1]);
    };
    ParamReader rd (p.state);
    lastMic = ! rd.b (sl, p73Mic); lastCont = ! rd.b (sl, p73Cont);
    tick();
}
void Pre73Panel::tick()
{
    using namespace id;
    ParamReader rd (proc.state);
    const bool m = rd.b (slot, p73Mic);
    const int step = rd.i (slot, p73Gain);
    if (m != lastMic) { lastMic = m; gain.slider.setRange (0, m ? 12 : 6, 1); gain.slider.setDoubleClickReturnValue (true, m ? 6 : 2); }
    if ((int) gain.slider.getValue() != step) gain.slider.setValue (std::min (step, m ? 12 : 6), juce::dontSendNotification);
    const bool c = rd.b (slot, p73Cont);
    const int v = rd.i (slot, p73Variant);
    if (c != lastCont || v != lastVariant)
    {
        lastCont = c; lastVariant = v; const bool is84 = v == 1;
        for (auto* k : { &hpf, &lowF, &midF, &highF, &lpf }) k->setVisible (! c);
        for (auto* k : { &hpfC, &lowC, &midC, &highC, &lpfC }) k->setVisible (c);
        hiQ.setEnabled (is84); lpf.setEnabled (is84); lpfC.setEnabled (is84); highF.setEnabled (is84);
        resized(); repaint();
    }
    gain.repaint(); highF.repaint();
}
void Pre73Panel::layoutControls (juce::Rectangle<int> row)
{
    const int kw = 66, kh = 86;
    auto put = [&] (juce::Rectangle<int>& g, std::initializer_list<juce::Component*> cs, int w) { auto r = g.removeFromLeft (w).withHeight (kh); for (auto* c : cs) c->setBounds (r); };
    auto btns = [&] (juce::Rectangle<int>& g, std::initializer_list<juce::Button*> bs) { auto cl = g.removeFromLeft (58); int y = cl.getY() + 6; for (auto* b : bs) { b->setBounds (cl.getX() + 2, y, 54, 26); y += 32; } };
    row = row.withHeight (kh + 12);
    auto g = placeGroup (row, kw + 70, "PRE", hlGain);     put (g, { &gain }, kw); btns (g, { &mic, &phase });
    g = placeGroup (row, kw + 8, "HPF", hlHPF);           put (g, { &hpf, &hpfC }, kw);
    g = placeGroup (row, kw * 2 + 8, "LOW", hlLow);       put (g, { &lowF, &lowC }, kw); put (g, { &lowG }, kw);
    g = placeGroup (row, kw * 2 + 70, "MID", hlMid);      put (g, { &midF, &midC }, kw); put (g, { &midG }, kw); btns (g, { &hiQ });
    g = placeGroup (row, kw * 2 + 8, "HIGH", hlHigh);     put (g, { &highF, &highC }, kw); put (g, { &highG }, kw);
    g = placeGroup (row, kw + 8, "LPF", hlLPF);           put (g, { &lpf, &lpfC }, kw);
    g = placeGroup (row, kw + 70, "", hlNone);            btns (g, { &eqIn }); put (g, { &out }, kw);
}

// ============================================================================ Fet76Panel
Fet76Panel::Fet76Panel (::VintageRackEditor& e, VintageRackProcessor& p, HoverSink& s, int sl)
    : ModulePanel (e, p, s, sl, kT76, J ("FET ピークリミッター")),
      input ("Input", sl, hlDyn, s), output ("Output", sl, hlDyn, s), attack ("Attack", sl, hlDyn, s), release ("Release", sl, hlDyn, s), dyn (p, sl, kT76)
{
    using namespace id;
    variant.addItemList ({ "Early (Blue)", "Low-noise (Black)", "Late (Silver)" }, 1); addAndMakeVisible (variant);
    variantAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.state, pid (f76Variant), variant);
    addHeaderToggles (f76Analog, nullptr);
    input.attach (p.state, pid (f76In)); input.format = sdb;
    input.describe = [] { return J ("INPUT：入力を上げるほど深くかかります（スレッショルド固定の FET コンプ方式）。全帯域に作用"); };
    output.attach (p.state, pid (f76Out)); output.format = sdb;
    output.describe = [] { return J ("OUTPUT：出力レベル。ANALOG オンでは上げるほど歪みも増えます"); };
    auto atkUs = [] (double v) { return (int) std::lround (800 * std::pow (20.0 / 800, (v - 1) / 6)); };
    auto relMs = [] (double v) { return (int) std::lround (1100 * std::pow (50.0 / 1100, (v - 1) / 6)); };
    attack.attach (p.state, pid (f76Atk)); attack.format = [atkUs] (double v) { return juce::String (atkUs (v)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb5s")); };
    attack.describe = [this, atkUs] { return J ("ATTACK：7 が最速（20 µs）、1 が最遅（800 µs）。現在 ") + juce::String (atkUs (attack.slider.getValue())) + J (" µs"); };
    release.attach (p.state, pid (f76Rel)); release.format = [relMs] (double v) { return juce::String (relMs (v)) + " ms"; };
    release.describe = [this, relMs] { return J ("RELEASE：7 が最速（50 ms）、1 が最遅（1.1 s）。現在 ") + juce::String (relMs (release.slider.getValue())) + " ms"; };
    for (auto* k : { &input, &output, &attack, &release }) addAndMakeVisible (*k);
    static const char* names[] = { "4:1", "8:1", "12:1", "20:1", "ALL" };
    for (int i = 0; i < 5; ++i)
    {
        ratio[i].setButtonText (names[i]); addAndMakeVisible (ratio[i]);
        ratio[i].onClick = [this, i] { auto* prm = proc.state.getParameter (pid (id::f76Ratio)); prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture(); tick(); };
    }
    ratio[4].setTooltip (J ("全ボタン押し：過圧縮でアグレッシブな歪み"));
    addAndMakeVisible (dyn);
    tick();
}
void Fet76Panel::tick()
{
    const int r = ParamReader (proc.state).i (slot, id::f76Ratio);
    for (int i = 0; i < 5; ++i) ratio[i].setToggleState (i == r, juce::dontSendNotification);
    dyn.repaint();
}
void Fet76Panel::layoutControls (juce::Rectangle<int> row)
{
    const int kw = 70, kh = 86;
    row = row.withHeight (kh + 12);
    auto g = placeGroup (row, kw * 2 + 8, "", hlNone); input.setBounds (g.removeFromLeft (kw).withHeight (kh)); output.setBounds (g.removeFromLeft (kw).withHeight (kh));
    g = placeGroup (row, kw * 2 + 8, "", hlNone); attack.setBounds (g.removeFromLeft (kw).withHeight (kh)); release.setBounds (g.removeFromLeft (kw).withHeight (kh));
    g = placeGroup (row, 186, "RATIO", hlNone);
    for (int i = 0; i < 5; ++i) ratio[i].setBounds (g.getX() + (i % 3) * 60, g.getY() + 8 + (i / 3) * 34, 56, 28);
    dyn.setBounds (row.removeFromRight (260).withHeight (kh + 12));
}

// ============================================================================ Opto3APanel
Opto3APanel::Opto3APanel (::VintageRackEditor& e, VintageRackProcessor& p, HoverSink& s, int sl)
    : ModulePanel (e, p, s, sl, kT3A, J ("光学式レベラー（ソリッドステート）")),
      peak ("Peak Red.", sl, hlDyn, s), gain ("Gain", sl, hlDyn, s), hf ("HF Emph", sl, hlSC, s), scFreq ("Freq", sl, hlSC, s), dyn (p, sl, kT3A)
{
    using namespace id;
    addHeaderToggles (o3Analog, o3Cont);
    peak.attach (p.state, pid (o3Peak)); peak.format = [] (double v) { return juce::String ((int) v); };
    peak.describe = [] { return J ("PEAK REDUCTION：かかり具合（スレッショルド）。上げるほど深く圧縮"); };
    gain.attach (p.state, pid (o3Gain)); gain.format = [] (double v) { return sdb ((v - 50) * 0.5); };
    gain.describe = [this] { return J ("GAIN：メイクアップゲイン ") + sdb ((gain.slider.getValue() - 50) * 0.5); };
    hf.attach (p.state, pid (o3Hf)); hf.format = [] (double v) { return v > 0 ? "+" + juce::String (v / 10, 1) + " dB" : juce::String ("FLAT"); };
    hf.describe = [this, &p, sl] { return J ("HF EMPH：検出回路だけに ") + hz (ParamReader (p.state).opto3a (sl).scFreq()) + J (" 以上のシェルフ（+") + juce::String (hf.slider.getValue() / 10, 1) + J (" dB）を足し、高域で強めに反応させます。音そのものの EQ は変わりません（点線）"); };
    scFreq.stepless = true;
    scFreq.attach (p.state, pid (o3ScHz)); scFreq.format = [] (double v) { return hz (v); };
    scFreq.describe = [this] { return J ("EMPH FREQ（無段階）：検出回路のシェルフ周波数 ") + hz (scFreq.slider.getValue()) + J ("。実機は固定"); };
    for (auto* k : { &peak, &gain, &hf, &scFreq }) addAndMakeVisible (*k);
    for (int i = 0; i < 2; ++i)
    {
        auto& b = i ? limit : comp; addAndMakeVisible (b);
        b.onClick = [this, i] { auto* prm = proc.state.getParameter (pid (id::o3Limit)); prm->beginChangeGesture(); prm->setValueNotifyingHost ((float) i); prm->endChangeGesture(); tick(); };
    }
    addAndMakeVisible (dyn);
    lastCont = ! ParamReader (p.state).b (sl, o3Cont);
    tick();
}
void Opto3APanel::tick()
{
    ParamReader rd (proc.state);
    const bool l = rd.b (slot, id::o3Limit);
    comp.setToggleState (! l, juce::dontSendNotification); limit.setToggleState (l, juce::dontSendNotification);
    const bool c = rd.b (slot, id::o3Cont);
    if (c != lastCont) { lastCont = c; scFreq.setVisible (c); resized(); repaint(); }
    dyn.repaint();
}
void Opto3APanel::layoutControls (juce::Rectangle<int> row)
{
    const int kw = 70, kh = 86;
    row = row.withHeight (kh + 12);
    auto g = placeGroup (row, kw * 2 + 8, "", hlNone); peak.setBounds (g.removeFromLeft (kw).withHeight (kh)); gain.setBounds (g.removeFromLeft (kw).withHeight (kh));
    g = placeGroup (row, 76, "", hlNone); comp.setBounds (g.getX() + 4, g.getY() + 8, 62, 28); limit.setBounds (g.getX() + 4, g.getY() + 42, 62, 28);
    g = placeGroup (row, lastCont ? kw * 2 + 8 : kw + 8, "SIDECHAIN", hlSC);
    hf.setBounds (g.removeFromLeft (kw).withHeight (kh)); scFreq.setBounds (g.removeFromLeft (kw).withHeight (kh));
    dyn.setBounds (row.removeFromRight (260).withHeight (kh + 12));
}

// ============================================================================ Editor
namespace { constexpr int kToolbar = 34; }

VintageRackEditor::VintageRackEditor (VintageRackProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    if (hasType (kT73) || hasType (kT3A))
    {
        map = std::make_unique<FreqMap> (p, *this); addAndMakeVisible (*map);
        for (auto* b : { &r20, &r6, &r2 }) { addAndMakeVisible (*b); b->setClickingTogglesState (false); }
        r20.onClick = [this] { map->range = 20; timerCallback(); };
        r6.onClick = [this] { map->range = 6; timerCallback(); };
        r2.onClick = [this] { map->range = 2; timerCallback(); };
        addAndMakeVisible (allOn); addAndMakeVisible (allOff);
    }
    else { addAndMakeVisible (allOn); addAndMakeVisible (allOff); }
    allOn.onClick = [this] { proc.setAllAnalog (true); hover (-1, hlNone, J ("全モジュールの ANALOG をオンにしました")); };
    allOff.onClick = [this] { proc.setAllAnalog (false); hover (-1, hlNone, J ("全モジュールの ANALOG をオフにしました（完全にクリーンな処理）")); };
    if (isRack())
    {
        for (auto* b : { &add73, &add76, &add3A }) addAndMakeVisible (*b);
        add73.onClick = [this] { if (! proc.addModule (kT73)) hover (-1, hlNone, J ("スロットがいっぱいです（最大 8 台）")); };
        add76.onClick = [this] { if (! proc.addModule (kT76)) hover (-1, hlNone, J ("スロットがいっぱいです（最大 8 台）")); };
        add3A.onClick = [this] { if (! proc.addModule (kT3A)) hover (-1, hlNone, J ("スロットがいっぱいです（最大 8 台）")); };
    }
    // factory presets: whole chains in the rack, module presets in the single plugins
    headerPreset.setTextWhenNothingSelected (isRack() ? J ("ラックプリセット…") : J ("プリセット…"));
    for (int i = 0; i < proc.getNumPrograms(); ++i) headerPreset.addItem (proc.getProgramName (i), i + 1);
    const juce::Component::SafePointer<VintageRackEditor> safeThis (this);
    headerPreset.onChange = [this, safeThis]
    {
        const int idx = headerPreset.getSelectedItemIndex(); if (idx < 0) return;
        const auto name = headerPreset.getText();
        headerPreset.setSelectedId (0, juce::dontSendNotification);
        juce::MessageManager::callAsync ([safeThis, idx, name]
        {
            if (auto* self = safeThis.getComponent()) self->applyPreset (idx, name);
        });
    };
    addAndMakeVisible (headerPreset);
    viewport.setViewedComponent (&rackContent, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    rebuildPanels();
    hover (-1, hlNone, {});
    if (isRack()) { setResizable (true, true); setResizeLimits (kW, 640, kW, 1600); setSize (kW, 900); }
    else
    {
        int h = kMargin + kHeader + kGap + (map ? kMap + kGap : 0) + kHint + kGap + kToolbar + kGap + ModulePanel::heightFor (fixedType(), false) + kMargin;
        setSize (kW, h);
    }
    startTimerHz (30);
}
VintageRackEditor::~VintageRackEditor() { setLookAndFeel (nullptr); }

void VintageRackEditor::applyPreset (int idx, const juce::String& name)
{
    circuitOpen.fill (false);
    if (isRack()) proc.applyRackPreset (idx); else proc.applyModulePreset (0, idx);
    proc.currentProgram = idx;
    hover (-1, hlNone, J ("プリセット「") + name + J ("」を読み込みました"));
    layoutRack();
}

void VintageRackEditor::rebuildPanels()
{
    ParamReader rd (proc.state);
    signature.clear();
    for (int s = 0; s < numSlots(); ++s) signature.push_back (rd.type (s));
    panels.clear();
    for (int s = 0; s < numSlots(); ++s)
    {
        const int t = signature[(size_t) s];
        if (t == kT73) panels.push_back (std::make_unique<Pre73Panel> (*this, proc, *this, s));
        else if (t == kT76) panels.push_back (std::make_unique<Fet76Panel> (*this, proc, *this, s));
        else if (t == kT3A) panels.push_back (std::make_unique<Opto3APanel> (*this, proc, *this, s));
    }
    for (auto& pn : panels) rackContent.addAndMakeVisible (*pn);
    layoutRack();
}
void VintageRackEditor::layoutRack()
{
    const int w = viewport.getWidth() - (isRack() ? viewport.getScrollBarThickness() + 4 : 0);
    int y = 0;
    for (auto& pn : panels)
    {
        pn->setBounds (0, y, std::max (w, 200), ModulePanel::heightFor (pn->type, isCircuitOpen (pn->slot))); y += pn->getHeight() + kGap;
        pn->up.setEnabled (pn->slot > 0); pn->down.setEnabled (pn->slot < (int) panels.size() - 1);
    }
    rackContent.setSize (std::max (w, 200), std::max (y - kGap, 1));
    if (! isRack() && ! panels.empty() && viewport.getHeight() > 0 && viewport.getHeight() != rackContent.getHeight())
        setSize (getWidth(), getHeight() + rackContent.getHeight() - viewport.getHeight());   // single plugins grow with the CIRCUIT panel
    add73.setEnabled ((int) panels.size() < numSlots()); add76.setEnabled ((int) panels.size() < numSlots()); add3A.setEnabled ((int) panels.size() < numSlots());
}

void VintageRackEditor::hover (int slot, int hl, const juce::String& text)
{
    hlSlot = slot; hlBand = hl;
    hint = text.isNotEmpty() ? text
         : (map ? J ("つまみにカーソルを合わせると、そのつまみが動かす帯域がグラフ上で同じ色で光ります。ANALOG ボタンではアナログ特性のカーブが光ります。")
                : J ("つまみにカーソルを合わせると説明が表示されます。"));
    if (map) { map->hlSlot = slot; map->hl = hl; map->repaint(); }
    for (auto& pn : panels) pn->repaint();
    repaint (hintArea);
}

void VintageRackEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto head = getLocalBounds().reduced (kMargin).removeFromTop (kHeader).toFloat();
    g.setColour (col::panel); g.fillRoundedRectangle (head, 6.f);
    g.setColour (col::line); g.drawRoundedRectangle (head.reduced (0.5f), 6.f, 1.f);
    g.setColour (col::ink); g.setFont (font (22.f, true));
    g.drawText (JucePlugin_Name, (int) head.getX() + 14, (int) head.getY(), 300, (int) head.getHeight(), juce::Justification::left);
    g.setColour (col::inkDim); g.setFont (font (12.f));
    const juce::String sub = isRack() ? J ("ヴィンテージ・マイクプリ／コンプ ラック ― 上から順に信号が流れます") : J ("Vintage Rack シリーズ");
    const int sx = (int) head.getX() + 14 + (int) textW (font (22.f, true), JucePlugin_Name) + 14;
    g.drawFittedText (sub, sx, (int) head.getY(), headerPreset.getX() - sx - 10, (int) head.getHeight(), juce::Justification::centredLeft, 1, 1.0f);
    auto m = meterArea.toFloat();
    auto drawMeter = [&] (juce::Rectangle<float> r, const char* lab, float lvl)
    {
        g.setColour (col::inkDim); g.setFont (mono (11.f)); g.drawText (lab, r.removeFromLeft (30).toNearestInt(), juce::Justification::centredRight);
        r.removeFromLeft (6); r = r.withSizeKeepingCentre (r.getWidth(), 8);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 2);
        const float t = juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (lvl, -60.f) + 48) / 48);
        juce::ColourGradient cg (col::good, r.getX(), 0, col::bad, r.getRight(), 0, false); cg.addColour (0.7, col::good); cg.addColour (0.8, col::warn);
        g.setGradientFill (cg); g.fillRoundedRectangle (r.withWidth (r.getWidth() * t), 2);
    };
    drawMeter (m.removeFromLeft (m.getWidth() / 2).reduced (4, 0), "IN", inLvl);
    drawMeter (m.reduced (4, 0), "OUT", outLvl);
    g.setFont (font (13.f));
    auto h = hintArea;
    if (hlBand != hlNone) { g.setColour (hlColour (hlBand)); g.fillEllipse ((float) h.getX() + 4, (float) h.getCentreY() - 5, 10, 10); }
    g.setColour (col::ink); g.drawFittedText (hint, h.withTrimmedLeft (22), juce::Justification::centredLeft, 1, 0.9f);
    // toolbar labels
    g.setColour (col::inkDim); g.setFont (font (12.f));
    if (isRack()) g.drawText (J ("追加:"), toolbarArea.getX(), toolbarArea.getY(), 40, toolbarArea.getHeight(), juce::Justification::centredLeft);
    if (map) g.drawText (J ("縦軸"), r20.getX() - 34, toolbarArea.getY(), 30, toolbarArea.getHeight(), juce::Justification::centredRight);
    if (isRack()) g.drawText (juce::String ((int) panels.size()) + J (" 台（最大 8）"), allOn.getX() - 110, toolbarArea.getY(), 100, toolbarArea.getHeight(), juce::Justification::centredRight);
    if (isRack() && panels.empty())
    {
        g.drawText (J ("モジュールがありません。上の「＋」ボタンで追加してください。"), viewport.getBounds(), juce::Justification::centred);
    }
}

void VintageRackEditor::paintOverChildren (juce::Graphics& g)
{
    if (dropY >= 0) { g.setColour (col::accent); g.fillRect (viewport.getX(), dropY - 2, viewport.getWidth(), 3); }   // drop position while dragging a module
}

void VintageRackEditor::resized()
{
    auto r = getLocalBounds().reduced (kMargin);
    auto head = r.removeFromTop (kHeader);
    meterArea = head.removeFromRight (300).reduced (14, 0);
    headerPreset.setBounds (head.removeFromRight (250).withSizeKeepingCentre (250, 30));
    r.removeFromTop (kGap);
    if (map) { map->setBounds (r.removeFromTop (kMap)); r.removeFromTop (kGap); }
    hintArea = r.removeFromTop (kHint);
    r.removeFromTop (kGap);
    toolbarArea = r.removeFromTop (kToolbar);
    {
        auto t = toolbarArea;
        if (isRack())
        {
            t.removeFromLeft (44);
            add73.setBounds (t.removeFromLeft (140).reduced (0, 3)); t.removeFromLeft (6);
            add76.setBounds (t.removeFromLeft (110).reduced (0, 3)); t.removeFromLeft (6);
            add3A.setBounds (t.removeFromLeft (110).reduced (0, 3)); t.removeFromLeft (6);
        }
        if (map)
        {
            t.removeFromLeft (40);
            r20.setBounds (t.removeFromLeft (70).reduced (0, 5)); t.removeFromLeft (4);
            r6.setBounds (t.removeFromLeft (60).reduced (0, 5)); t.removeFromLeft (4);
            r2.setBounds (t.removeFromLeft (60).reduced (0, 5));
        }
        allOff.setBounds (t.removeFromRight (120).reduced (0, 3)); t.removeFromRight (6);
        allOn.setBounds (t.removeFromRight (120).reduced (0, 3));
    }
    r.removeFromTop (kGap);
    viewport.setBounds (r);
    layoutRack();
}

void VintageRackEditor::dragPanel (vr::ui::ModulePanel& pn, int yInRack, bool finished)
{
    const int yEd = yInRack + viewport.getY() - viewport.getViewPositionY();
    if (! finished) { dropY = juce::jlimit (viewport.getY(), viewport.getBottom(), yEd); repaint(); return; }
    dropY = -1;
    int target = (int) panels.size() - 1;
    for (auto& other : panels) if (yInRack < other->getBounds().getCentreY()) { target = other->slot - (other->slot > pn.slot ? 1 : 0); break; }
    proc.moveSlot (pn.slot, juce::jlimit (0, (int) panels.size() - 1, target));
    repaint();
}

void VintageRackEditor::timerCallback()
{
    inLvl = std::max (proc.inPeak.exchange (0.f), inLvl * 0.85f);
    outLvl = std::max (proc.outPeak.exchange (0.f), outLvl * 0.85f);
    repaint (meterArea);
    // rebuild the rack when modules were added / removed / moved (also when the host restores a session)
    ParamReader rd (proc.state);
    bool changed = false;
    for (int s = 0; s < numSlots(); ++s) if (rd.type (s) != signature[(size_t) s]) changed = true;
    if (changed) { hover (-1, hlNone, {}); rebuildPanels(); repaint(); }
    for (auto& pn : panels) { pn->tick(); pn->tickCircuit(); pn->setGR (proc.gr[(size_t) pn->slot].exchange (0.f)); }
    if (map)
    {
        for (auto* b : { &r20, &r6, &r2 }) b->setToggleState (false, juce::dontSendNotification);
        (map->range >= 20 ? r20 : map->range >= 6 ? r6 : r2).setToggleState (true, juce::dontSendNotification);
        map->tick();
    }
    if (dropY >= 0) repaint();
}
