/*
  ==============================================================================

    Lush Chorus - Walrus Julia-inspired chorus/vibrato plugin
    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "ChorusDSP.h"

//==============================================================================
class LushChorusAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    LushChorusAudioProcessor();
    ~LushChorusAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter tree (public so the editor can attach controls)
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter IDs
    static constexpr const char* kRateID      = "rate";
    static constexpr const char* kDepthID     = "depth";
    static constexpr const char* kLagID       = "lag";
    static constexpr const char* kWaveformID  = "waveform";
    static constexpr const char* kMixID       = "mix";      // D-C-V blend
    static constexpr const char* kDryWetID    = "drywet";   // classic wet/dry
    static constexpr const char* kOutputID    = "output";

    // Meter state exposed to the UI (peak in linear, clip latch flag)
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<bool>  clipLatched { false };

    // Called by the editor to reset the clip LED
    void resetClipIndicator() noexcept { clipLatched.store (false); }

private:
    //==============================================================================
    LushDSP::ChorusEngine engine;
    juce::SmoothedValue<float> outputGainSmooth;
    juce::SmoothedValue<float> dryWetSmooth;
    juce::AudioBuffer<float>   dryBuffer;   // cached dry copy for wet/dry mix

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LushChorusAudioProcessor)
};
