/*
  ==============================================================================

    Lush Chorus - Walrus Julia-inspired chorus/vibrato plugin
    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// LushKnobLookAndFeel
//==============================================================================
LushKnobLookAndFeel::LushKnobLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId,    LushColours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, LushColours::panelLight);
    setColour (juce::Slider::textBoxTextColourId,         LushColours::textDim);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 LushColours::textDim);
}

void LushKnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& /*slider*/)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    const float angle = rotaryStartAngle
                      + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float outerR   = radius;
    const float bodyR    = radius * 0.78f;
    const float arcR     = radius * 0.92f;
    const float arcThick = 3.5f;

    // Outer ring (subtle)
    g.setColour (LushColours::panelLight);
    g.drawEllipse (centre.x - outerR, centre.y - outerR,
                   outerR * 2.0f, outerR * 2.0f, 1.5f);

    // Background arc
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR,
                             0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (LushColours::panel);
        g.strokePath (track, juce::PathStrokeType (arcThick,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Value arc
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR,
                             0.0f, rotaryStartAngle, angle, true);
        g.setColour (LushColours::accent);
        g.strokePath (value, juce::PathStrokeType (arcThick,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // Knob body (slightly raised disc)
    juce::ColourGradient body (LushColours::panelLight,
                               centre.x - bodyR * 0.4f, centre.y - bodyR * 0.7f,
                               LushColours::panel,
                               centre.x + bodyR * 0.4f, centre.y + bodyR * 0.7f,
                               false);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    g.setColour (LushColours::background.withAlpha (0.6f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR,
                   bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    // Indicator line
    {
        const float pointerLen   = bodyR * 0.72f;
        const float pointerThick = 2.5f;
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThick * 0.5f,
                                     -pointerLen,
                                     pointerThick,
                                     pointerLen * 0.85f,
                                     pointerThick * 0.5f);

        g.setColour (LushColours::accent);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle)
                                 .translated (centre.x, centre.y));
    }

    // Centre cap
    const float capR = bodyR * 0.18f;
    g.setColour (LushColours::accentDim);
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
}

juce::Label* LushKnobLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::textColourId, LushColours::textDim);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::Font (juce::FontOptions (12.0f)));
    return l;
}

juce::Font LushKnobLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

//==============================================================================
// LabelledKnob
//==============================================================================
LabelledKnob::LabelledKnob (const juce::String& name)
    : knobName (name)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setColour (juce::Slider::textBoxTextColourId, LushColours::textDim);
    addAndMakeVisible (slider);
}

void LabelledKnob::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (18);        // title label
    slider.setBounds (b);
}

void LabelledKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    auto titleArea = b.removeFromTop (18);

    g.setColour (LushColours::accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawFittedText (knobName, titleArea, juce::Justification::centred, 1);
}

//==============================================================================
// OutputMeter
//==============================================================================
OutputMeter::OutputMeter (LushChorusAudioProcessor& p) : processor (p)
{
    startTimerHz (30);
}

OutputMeter::~OutputMeter() { stopTimer(); }

void OutputMeter::resized()
{
    auto b = getLocalBounds();
    clipBounds  = b.removeFromTop (14).reduced (2);
    b.removeFromTop (4);
    meterBounds = b;
}

void OutputMeter::timerCallback()
{
    const float rawL = processor.meterPeakL.load();
    const float rawR = processor.meterPeakR.load();

    // UI-side smoothing for a silky meter.
    constexpr float attack  = 0.55f;
    constexpr float release = 0.12f;

    auto approach = [] (float current, float target, float a, float r)
    {
        const float coeff = (target > current) ? a : r;
        return current + coeff * (target - current);
    };

    displayL = approach (displayL, rawL, attack, release);
    displayR = approach (displayR, rawR, attack, release);

    repaint();
}

void OutputMeter::mouseDown (const juce::MouseEvent&)
{
    processor.resetClipIndicator();
    repaint();
}

static float linearToMeterY (float linear, float h)
{
    // Map -60 dB..0 dB to the meter height.
    const float db = juce::Decibels::gainToDecibels (juce::jmax (linear, 1.0e-5f));
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    return h * (1.0f - norm);
}

void OutputMeter::paint (juce::Graphics& g)
{
    // Clip LED
    {
        const bool clipping = processor.clipLatched.load();
        g.setColour (clipping ? LushColours::clipOn : LushColours::clipOff);
        g.fillRoundedRectangle (clipBounds.toFloat(), 3.0f);
        g.setColour (clipping ? LushColours::clipOn.brighter (0.3f)
                              : LushColours::panelLight);
        g.drawRoundedRectangle (clipBounds.toFloat(), 3.0f, 1.0f);

        g.setColour (clipping ? juce::Colours::white
                              : LushColours::textDim.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawFittedText ("CLIP", clipBounds, juce::Justification::centred, 1);
    }

    // Meter body
    auto mb = meterBounds.toFloat();
    g.setColour (LushColours::background.darker (0.3f));
    g.fillRoundedRectangle (mb, 3.0f);
    g.setColour (LushColours::panelLight);
    g.drawRoundedRectangle (mb, 3.0f, 1.0f);

    // Two channels side by side
    const float gap = 2.0f;
    const float chW = (mb.getWidth() - 3 * gap) * 0.5f;

    auto drawChannel = [&] (float x, float value)
    {
        const float innerX = x;
        const float innerY = mb.getY() + gap;
        const float innerH = mb.getHeight() - gap * 2.0f;

        const float topY = innerY + linearToMeterY (value, innerH);
        const float barH = (innerY + innerH) - topY;

        if (barH <= 0.0f) return;

        juce::ColourGradient grad (LushColours::meterLow,
                                   0.0f, innerY + innerH,
                                   LushColours::meterHigh,
                                   0.0f, innerY,
                                   false);
        grad.addColour (0.65, LushColours::meterMid);

        juce::Rectangle<float> bar (innerX, topY, chW, barH);
        g.setGradientFill (grad);
        g.fillRect (bar);
    };

    drawChannel (mb.getX() + gap,              displayL);
    drawChannel (mb.getX() + gap * 2 + chW,    displayR);
}

//==============================================================================
// WaveformSwitch
//==============================================================================
WaveformSwitch::WaveformSwitch()
{
    auto style = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,     LushColours::panel);
        b.setColour (juce::TextButton::buttonOnColourId,   LushColours::accent);
        b.setColour (juce::TextButton::textColourOffId,    LushColours::textDim);
        b.setColour (juce::TextButton::textColourOnId,     LushColours::background);
        b.setClickingTogglesState (true);
        b.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    };

    style (sineButton);
    style (triangleButton);

    sineButton.setRadioGroupId (42);
    triangleButton.setRadioGroupId (42);

    addAndMakeVisible (sineButton);
    addAndMakeVisible (triangleButton);
}

void WaveformSwitch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    auto titleArea = b.removeFromTop (18);
    g.setColour (LushColours::accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawFittedText ("WAVE", titleArea, juce::Justification::centred, 1);
}

void WaveformSwitch::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (18);
    b = b.reduced (4, 6);
    const int half = b.getWidth() / 2;
    sineButton.setBounds     (b.removeFromLeft (half));
    triangleButton.setBounds (b);
}

//==============================================================================
// LushChorusAudioProcessorEditor
//==============================================================================
LushChorusAudioProcessorEditor::LushChorusAudioProcessorEditor (LushChorusAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      meter (p)
{
    setLookAndFeel (&knobLnf);

    for (auto* k : { &rateKnob, &depthKnob, &lagKnob, &mixKnob, &outputKnob })
    {
        k->slider.setLookAndFeel (&knobLnf);
        addAndMakeVisible (*k);
    }

    // Unit suffixes
    rateKnob.slider.setTextValueSuffix   (" Hz");
    depthKnob.slider.setTextValueSuffix  (" ms");
    lagKnob.slider.setTextValueSuffix    (" ms");
    outputKnob.slider.setTextValueSuffix (" dB");

    // Rate uses a log-ish skew via the parameter range; make the slider reflect it.
    rateKnob.slider.setNumDecimalPlacesToDisplay (2);
    depthKnob.slider.setNumDecimalPlacesToDisplay (2);
    lagKnob.slider.setNumDecimalPlacesToDisplay   (2);
    mixKnob.slider.setNumDecimalPlacesToDisplay   (2);
    outputKnob.slider.setNumDecimalPlacesToDisplay (2);

    addAndMakeVisible (waveSwitch);
    addAndMakeVisible (meter);

    // Attachments (sliders)
    auto& vts = audioProcessor.apvts;
    rateAtt   = std::make_unique<SliderAttachment> (vts, LushChorusAudioProcessor::kRateID,   rateKnob.slider);
    depthAtt  = std::make_unique<SliderAttachment> (vts, LushChorusAudioProcessor::kDepthID,  depthKnob.slider);
    lagAtt    = std::make_unique<SliderAttachment> (vts, LushChorusAudioProcessor::kLagID,    lagKnob.slider);
    mixAtt    = std::make_unique<SliderAttachment> (vts, LushChorusAudioProcessor::kMixID,    mixKnob.slider);
    outputAtt = std::make_unique<SliderAttachment> (vts, LushChorusAudioProcessor::kOutputID, outputKnob.slider);

    // Waveform toggle: drive the ChoiceParameter manually so the two buttons
    // behave like a radio pair with no extra attachment needed.
    waveSwitch.sineButton.onClick     = [this] { updateWaveformFromButtons(); };
    waveSwitch.triangleButton.onClick = [this] { updateWaveformFromButtons(); };
    refreshWaveformButtons();

    startTimerHz (10); // keeps the waveform toggle in sync with host-side changes

    setSize (640, 380);
}

void LushChorusAudioProcessorEditor::timerCallback()
{
    refreshWaveformButtons();
}

LushChorusAudioProcessorEditor::~LushChorusAudioProcessorEditor()
{
    for (auto* k : { &rateKnob, &depthKnob, &lagKnob, &mixKnob, &outputKnob })
        k->slider.setLookAndFeel (nullptr);

    setLookAndFeel (nullptr);
}

void LushChorusAudioProcessorEditor::updateWaveformFromButtons()
{
    auto* p = audioProcessor.apvts.getParameter (LushChorusAudioProcessor::kWaveformID);
    if (p == nullptr) return;

    const int index = waveSwitch.triangleButton.getToggleState() ? 1 : 0;
    // ChoiceParameter values are normalised 0..1 over the list
    const float norm = (float) index / 1.0f; // 2 choices -> 0 or 1
    p->beginChangeGesture();
    p->setValueNotifyingHost (norm);
    p->endChangeGesture();
}

void LushChorusAudioProcessorEditor::refreshWaveformButtons()
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            audioProcessor.apvts.getParameter (LushChorusAudioProcessor::kWaveformID)))
    {
        const int idx = choice->getIndex();
        waveSwitch.sineButton.setToggleState     (idx == 0, juce::dontSendNotification);
        waveSwitch.triangleButton.setToggleState (idx == 1, juce::dontSendNotification);
    }
}

//==============================================================================
void LushChorusAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient for a hint of analog warmth.
    juce::ColourGradient bg (LushColours::background.brighter (0.05f),
                             0.0f, 0.0f,
                             LushColours::background.darker (0.15f),
                             0.0f, (float) getHeight(),
                             false);
    g.setGradientFill (bg);
    g.fillAll();

    // Top header strip
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (LushColours::panel.withAlpha (0.55f));
    g.fillRect (header);

    g.setColour (LushColours::accent);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawFittedText ("LUSH CHORUS",
                      header.reduced (20, 0),
                      juce::Justification::centredLeft, 1);

    g.setColour (LushColours::textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::italic)));
    g.drawFittedText ("analog-voiced modulation",
                      header.reduced (20, 0).translated (180, 6),
                      juce::Justification::centredLeft, 1);

    // Footer strip
    auto footer = getLocalBounds().removeFromBottom (22);
    g.setColour (LushColours::panel.withAlpha (0.4f));
    g.fillRect (footer);
    g.setColour (LushColours::textDim.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawFittedText (juce::String (JucePlugin_Manufacturer)
                          + "  -  v" + juce::String (JucePlugin_VersionString),
                      footer.reduced (10, 0),
                      juce::Justification::centredRight, 1);

    // Divider under header
    g.setColour (LushColours::accent.withAlpha (0.25f));
    g.drawHorizontalLine (56, 20.0f, (float) getWidth() - 20.0f);
}

void LushChorusAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (56);     // header
    b.removeFromBottom (22);  // footer
    b.reduce (16, 10);

    // Right sidebar: waveform switch on top, meter below.
    auto sidebar = b.removeFromRight (120);
    auto waveArea = sidebar.removeFromTop (72);
    sidebar.removeFromTop (10);
    auto meterArea = sidebar;

    waveSwitch.setBounds (waveArea);
    meter.setBounds      (meterArea);

    b.removeFromRight (16);  // gap

    // Five knobs in a row
    const int numKnobs = 5;
    const int knobW = b.getWidth() / numKnobs;

    auto place = [&] (LabelledKnob& k)
    {
        k.setBounds (b.removeFromLeft (knobW).reduced (4, 0));
    };

    place (rateKnob);
    place (depthKnob);
    place (lagKnob);
    place (mixKnob);
    place (outputKnob);
}
