/*
  ==============================================================================

    Lush Chorus - Walrus Julia-inspired chorus/vibrato plugin
    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Colour palette - inspired by the Walrus Julia's purple / cream aesthetic.
namespace LushColours
{
    const juce::Colour background   { 0xff2a1e34 }; // deep plum
    const juce::Colour panel        { 0xff3a2a45 };
    const juce::Colour panelLight   { 0xff4a3758 };
    const juce::Colour accent       { 0xffe8c685 }; // warm cream/amber
    const juce::Colour accentDim    { 0xff8a6b44 };
    const juce::Colour text         { 0xfff0e6d2 };
    const juce::Colour textDim      { 0xffb5a78c };
    const juce::Colour meterLow     { 0xff7bcf8a };
    const juce::Colour meterMid     { 0xffe8c685 };
    const juce::Colour meterHigh    { 0xffd96060 };
    const juce::Colour clipOff      { 0xff3a1f22 };
    const juce::Colour clipOn       { 0xffff4444 };
}

//==============================================================================
/** Rotary-knob LookAndFeel with a Julia-ish arc + indicator. */
class LushKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LushKnobLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font   getLabelFont (juce::Label&) override;
};

//==============================================================================
/** A labelled rotary-knob component: knob + name + value readout. */
class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (const juce::String& name);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;

private:
    juce::String knobName;
};

//==============================================================================
/** Vertical peak meter (stereo) + latching clip LED on top. */
class OutputMeter : public juce::Component,
                    private juce::Timer
{
public:
    explicit OutputMeter (LushChorusAudioProcessor& p);
    ~OutputMeter() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    LushChorusAudioProcessor& processor;
    float displayL = 0.0f, displayR = 0.0f;
    juce::Rectangle<int> clipBounds;
    juce::Rectangle<int> meterBounds;
};

//==============================================================================
/** Two-position toggle used for the waveform switch (Sine / Triangle). */
class WaveformSwitch : public juce::Component
{
public:
    WaveformSwitch();

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::TextButton sineButton    { "SINE" };
    juce::TextButton triangleButton{ "TRI"  };
};

//==============================================================================
class LushChorusAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    LushChorusAudioProcessorEditor (LushChorusAudioProcessor&);
    ~LushChorusAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LushChorusAudioProcessor& audioProcessor;

    LushKnobLookAndFeel knobLnf;

    LabelledKnob rateKnob   { "RATE"   };
    LabelledKnob depthKnob  { "DEPTH"  };
    LabelledKnob lagKnob    { "LAG"    };
    LabelledKnob mixKnob    { "D   C   V" };
    LabelledKnob dryWetKnob { "MIX"    };
    LabelledKnob outputKnob { "OUTPUT" };

    WaveformSwitch waveSwitch;
    OutputMeter    meter;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> rateAtt, depthAtt, lagAtt, mixAtt, dryWetAtt, outputAtt;

    // For the two-state waveform choice we listen to button clicks directly
    // and write into the APVTS ChoiceParameter.
    void updateWaveformFromButtons();
    void refreshWaveformButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LushChorusAudioProcessorEditor)
};
