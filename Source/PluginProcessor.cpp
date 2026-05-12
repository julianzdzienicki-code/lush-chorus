/*
  ==============================================================================

    Lush Chorus - Walrus Julia-inspired chorus/vibrato plugin
    PluginProcessor.cpp

    NOTE: DSP is not yet implemented — this file currently provides the
    parameter tree required by the editor, plus a simple peak-meter tap so
    the output meter / clip LED respond to the audio that passes through.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
LushChorusAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Rate: 0.1 – 10 Hz, log/skewed so the low end is usable
    {
        juce::NormalisableRange<float> range (0.1f, 10.0f);
        range.setSkewForCentre (1.0f);
        params.push_back (std::make_unique<APF> (
            juce::ParameterID { kRateID, 1 },
            "Rate", range, 0.8f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    }

    // Depth: 0 – 10 ms
    params.push_back (std::make_unique<APF> (
        juce::ParameterID { kDepthID, 1 },
        "Depth",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f),
        3.5f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Lag (center delay): 5 – 25 ms
    params.push_back (std::make_unique<APF> (
        juce::ParameterID { kLagID, 1 },
        "Lag",
        juce::NormalisableRange<float> (5.0f, 25.0f, 0.01f),
        12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Waveform: Sine / Triangle
    params.push_back (std::make_unique<APC> (
        juce::ParameterID { kWaveformID, 1 },
        "Waveform",
        juce::StringArray { "Sine", "Triangle" },
        0));

    // Mix (D-C-V): 0 = Dry-only wet path, 0.5 = Chorus, 1 = Vibrato
    params.push_back (std::make_unique<APF> (
        juce::ParameterID { kMixID, 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.5f));

    // Dry/Wet: classic wet/dry blend applied after the chorus engine.
    // 0 = fully dry, 1 = fully wet. Shown on the UI as the "MIX" knob.
    params.push_back (std::make_unique<APF> (
        juce::ParameterID { kDryWetID, 1 },
        "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        1.0f));

    // Output gain: -12 to +12 dB
    params.push_back (std::make_unique<APF> (
        juce::ParameterID { kOutputID, 1 },
        "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
LushChorusAudioProcessor::LushChorusAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
}

LushChorusAudioProcessor::~LushChorusAudioProcessor()
{
}

//==============================================================================
const juce::String LushChorusAudioProcessor::getName() const           { return JucePlugin_Name; }
bool  LushChorusAudioProcessor::acceptsMidi() const                    { return false; }
bool  LushChorusAudioProcessor::producesMidi() const                   { return false; }
bool  LushChorusAudioProcessor::isMidiEffect() const                   { return false; }
double LushChorusAudioProcessor::getTailLengthSeconds() const          { return 0.0; }

int  LushChorusAudioProcessor::getNumPrograms()                        { return 1; }
int  LushChorusAudioProcessor::getCurrentProgram()                     { return 0; }
void LushChorusAudioProcessor::setCurrentProgram (int)                 {}
const juce::String LushChorusAudioProcessor::getProgramName (int)      { return {}; }
void LushChorusAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void LushChorusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    engine.reset();

    outputGainSmooth.reset (sampleRate, 0.02);
    outputGainSmooth.setCurrentAndTargetValue (1.0f);

    dryWetSmooth.reset (sampleRate, 0.02);
    dryWetSmooth.setCurrentAndTargetValue (
        apvts.getRawParameterValue (kDryWetID)->load());

    // Pre-allocate a dry-cache buffer that matches host layout.
    dryBuffer.setSize (juce::jmax (2, getTotalNumInputChannels()),
                       samplesPerBlock, false, false, true);
    dryBuffer.clear();

    meterPeakL.store (0.0f);
    meterPeakR.store (0.0f);
    clipLatched.store (false);
}

void LushChorusAudioProcessor::releaseResources()
{
    engine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LushChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void LushChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // ----- Read parameters for this block -----
    const float rateHz   = apvts.getRawParameterValue (kRateID)   ->load();
    const float depthMs  = apvts.getRawParameterValue (kDepthID)  ->load();
    const float lagMs    = apvts.getRawParameterValue (kLagID)    ->load();
    const int   waveform = (int) apvts.getRawParameterValue (kWaveformID)->load();
    const float mix      = apvts.getRawParameterValue (kMixID)    ->load();
    const float drywet   = apvts.getRawParameterValue (kDryWetID) ->load();
    const float outDb    = apvts.getRawParameterValue (kOutputID) ->load();

    engine.setParameters (rateHz, depthMs, lagMs, waveform, mix);
    outputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (outDb));
    dryWetSmooth.setTargetValue (drywet);

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (2, buffer.getNumChannels());

    // ----- Cache the dry signal before the engine overwrites the buffer -----
    if (dryBuffer.getNumSamples() < numSamples
     || dryBuffer.getNumChannels() < numCh)
        dryBuffer.setSize (juce::jmax (numCh, dryBuffer.getNumChannels()),
                           numSamples, false, false, true);

    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ----- Run the chorus DSP in place on the buffer (now the wet signal) -----
    engine.process (buffer);

    // ----- Blend dry/wet, apply output gain, update meter + clip latch -----
    float peaks[2] = { 0.0f, 0.0f };

    for (int n = 0; n < numSamples; ++n)
    {
        const float w = dryWetSmooth.getNextValue();
        const float d = 1.0f - w;
        const float g = outputGainSmooth.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);

            const float blended = dry[n] * d + wet[n] * w;
            const float out     = blended * g;

            wet[n] = out;
            peaks[ch] = juce::jmax (peaks[ch], std::abs (out));
        }
    }

    // Peak meter with gentle ballistics for the UI side.
    constexpr float releaseCoeff = 0.85f;
    const float prevL = meterPeakL.load();
    const float prevR = meterPeakR.load();

    const float newL = juce::jmax (peaks[0], prevL * releaseCoeff);
    const float newR = juce::jmax (numCh > 1 ? peaks[1] : peaks[0], prevR * releaseCoeff);

    meterPeakL.store (newL);
    meterPeakR.store (newR);

    if (newL >= 1.0f || newR >= 1.0f)
        clipLatched.store (true);
}

//==============================================================================
bool LushChorusAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* LushChorusAudioProcessor::createEditor()
{
    return new LushChorusAudioProcessorEditor (*this);
}

//==============================================================================
void LushChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LushChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LushChorusAudioProcessor();
}
