/*
  ==============================================================================

    Lush Chorus - DSP building blocks
    ChorusDSP.h

    Signal flow:
        input
          └─ split
               ├─ dry path ──────────────────────────────────┐
               └─ wet path:                                  │
                    pre-emphasis (+3 dB high-shelf @ 2k)     │
                    -> soft saturation (tanh)                │
                    -> modulated fractional delay (Lagrange) │
                    -> BBD-style lowpass                     │
                    -> de-emphasis (matching cut)            │
                    -> tone EQ (HP 100, peak 1.5k, LP 9k)    │
          └─ mix law (D-C-V)  (1-mix)*dry + mix*wet  + comp ─┘
          └─ (output gain + meter handled in the processor)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace LushDSP
{

//==============================================================================
/** Analog-voiced LFO: sine or slightly-asymmetric triangle, with gentle
    frequency drift and a one-pole smoother on the output to soften corners. */
class AnalogLFO
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // Smoother cutoff ~120 Hz: enough to round the triangle kink without
        // eating the LFO signal even at 10 Hz.
        const float fc = 120.0f;
        smoothCoeff = std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sr);
        reset();
    }

    void reset()
    {
        phase        = 0.0f;
        driftTimer   = 0.0f;
        driftTarget  = 0.0f;
        driftValue   = 0.0f;
        smoothedOut  = 0.0f;
    }

    void setRateHz   (float hz)  { baseRate = juce::jlimit (0.01f, 40.0f, hz); }
    void setWaveform (int w)     { waveform = (w == 1) ? 1 : 0; }
    void setPhase    (float p)   { phase = p - std::floor (p); }

    float process() noexcept
    {
        // Slow drift: re-roll a new target every ~50 ms, crawl toward it.
        driftTimer += 1.0f / (float) sr;
        if (driftTimer >= 0.05f)
        {
            driftTimer  = 0.0f;
            driftTarget = (rng.nextFloat() * 2.0f - 1.0f) * 0.018f; // +/- 1.8%
        }
        driftValue += 0.0006f * (driftTarget - driftValue);

        const float effectiveRate = baseRate * (1.0f + driftValue);
        phase += effectiveRate / (float) sr;
        if (phase >= 1.0f) phase -= 1.0f;

        float raw;
        if (waveform == 0)
        {
            raw = std::sin (phase * juce::MathConstants<float>::twoPi);
        }
        else
        {
            // 48 / 52 asymmetric triangle - removes the perfectly-symmetric
            // feel that makes digital triangles sound "mechanical".
            constexpr float rise = 0.48f;
            raw = (phase < rise)
                    ? (-1.0f + 2.0f *  (phase            / rise))
                    : ( 1.0f - 2.0f * ((phase - rise) / (1.0f - rise)));
        }

        smoothedOut = smoothedOut * smoothCoeff + raw * (1.0f - smoothCoeff);
        return smoothedOut;
    }

private:
    double sr = 44100.0;
    float phase         = 0.0f;
    float baseRate      = 1.0f;
    int   waveform      = 0;
    float driftTimer    = 0.0f;
    float driftTarget   = 0.0f;
    float driftValue    = 0.0f;
    float smoothedOut   = 0.0f;
    float smoothCoeff   = 0.0f;
    juce::Random rng { (juce::int64) juce::Time::getHighResolutionTicks() };
};

//==============================================================================
/** Full stereo chorus/vibrato engine. */
class ChorusEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;

        const int maxDelaySamples = (int) std::ceil (0.060 * sr) + 8;

        juce::dsp::ProcessSpec spec {};
        spec.sampleRate       = sr;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);
        spec.numChannels      = 1;

        for (auto* d : { &delayL, &delayR })
        {
            d->setMaximumDelayInSamples (maxDelaySamples);
            d->prepare (spec);
            d->reset();
        }

        lfoL.prepare (sr);
        lfoR.prepare (sr);
        lfoL.setPhase (0.0f);
        lfoR.setPhase (0.25f);      // 90 degrees offset for stereo width

        // Filter coefficients (fixed)
        const float preShelfGain = juce::Decibels::decibelsToGain ( 3.0f);
        const float deShelfGain  = juce::Decibels::decibelsToGain (-3.0f);
        const float midBoostGain = juce::Decibels::decibelsToGain ( 1.5f);

        auto preCoeffs  = juce::dsp::IIR::Coefficients<float>::makeHighShelf  (sr, 2000.0f, 0.707f, preShelfGain);
        auto deCoeffs   = juce::dsp::IIR::Coefficients<float>::makeHighShelf  (sr, 2000.0f, 0.707f, deShelfGain);
        auto bbdCoeffs  = juce::dsp::IIR::Coefficients<float>::makeLowPass    (sr, 7000.0f, 0.60f);
        auto toneHpC    = juce::dsp::IIR::Coefficients<float>::makeHighPass   (sr,  100.0f, 0.707f);
        auto toneMidC   = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0f, 0.9f,  midBoostGain);
        auto toneLpC    = juce::dsp::IIR::Coefficients<float>::makeLowPass    (sr, 9000.0f, 0.5f);

        auto applyAndReset = [] (juce::dsp::IIR::Filter<float>& f,
                                 juce::dsp::IIR::Coefficients<float>::Ptr c)
        {
            f.coefficients = c;
            f.reset();
        };

        applyAndReset (preEmphL, preCoeffs);    applyAndReset (preEmphR, preCoeffs);
        applyAndReset (deEmphL,  deCoeffs);     applyAndReset (deEmphR,  deCoeffs);
        applyAndReset (bbdLpL,   bbdCoeffs);    applyAndReset (bbdLpR,   bbdCoeffs);
        applyAndReset (toneHpL,  toneHpC);      applyAndReset (toneHpR,  toneHpC);
        applyAndReset (toneMidL, toneMidC);     applyAndReset (toneMidR, toneMidC);
        applyAndReset (toneLpL,  toneLpC);      applyAndReset (toneLpR,  toneLpC);

        depthSmooth.reset (sr, 0.05);
        lagSmooth  .reset (sr, 0.05);
        mixSmooth  .reset (sr, 0.02);
        depthSmooth.setCurrentAndTargetValue (3.5f);
        lagSmooth  .setCurrentAndTargetValue (12.0f);
        mixSmooth  .setCurrentAndTargetValue (0.5f);
    }

    void reset()
    {
        delayL.reset(); delayR.reset();
        lfoL.reset();   lfoR.reset();
        lfoR.setPhase (0.25f);

        for (auto* f : { &preEmphL, &preEmphR, &deEmphL, &deEmphR,
                         &bbdLpL,   &bbdLpR,   &toneHpL, &toneHpR,
                         &toneMidL, &toneMidR, &toneLpL, &toneLpR })
            f->reset();
    }

    void setParameters (float rateHz, float depthMs, float lagMs,
                        int waveform, float mix)
    {
        lfoL.setRateHz   (rateHz);
        lfoR.setRateHz   (rateHz);
        lfoL.setWaveform (waveform);
        lfoR.setWaveform (waveform);

        depthSmooth.setTargetValue (depthMs);
        lagSmooth  .setTargetValue (lagMs);
        mixSmooth  .setTargetValue (juce::jlimit (0.0f, 1.0f, mix));
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (2, buffer.getNumChannels());

        auto* chL = buffer.getWritePointer (0);
        auto* chR = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

        const float msToSamples = (float) sr * 0.001f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float lag   = lagSmooth  .getNextValue();
            const float depth = depthSmooth.getNextValue();
            const float mix   = mixSmooth  .getNextValue();

            const float modL = lfoL.process();
            const float modR = lfoR.process();

            chL[n] = processChannel (chL[n], modL, lag, depth, mix, msToSamples,
                                     delayL,   preEmphL, deEmphL, bbdLpL,
                                     toneHpL,  toneMidL, toneLpL);

            if (chR != nullptr)
                chR[n] = processChannel (chR[n], modR, lag, depth, mix, msToSamples,
                                         delayR,   preEmphR, deEmphR, bbdLpR,
                                         toneHpR,  toneMidR, toneLpR);
        }
    }

private:
    using LagrangeDelay = juce::dsp::DelayLine<float,
                            juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;

    static inline float processChannel (float in,
                                        float mod,
                                        float lagMs,
                                        float depthMs,
                                        float mix,
                                        float msToSamples,
                                        LagrangeDelay& delay,
                                        juce::dsp::IIR::Filter<float>& preE,
                                        juce::dsp::IIR::Filter<float>& deE,
                                        juce::dsp::IIR::Filter<float>& bbd,
                                        juce::dsp::IIR::Filter<float>& toneHp,
                                        juce::dsp::IIR::Filter<float>& toneMid,
                                        juce::dsp::IIR::Filter<float>& toneLp)
    {
        // Pre-emphasis + mild saturation on the wet signal only.
        const float preEmph = preE.processSample (in);

        constexpr float drive = 1.25f;
        const float sat = std::tanh (preEmph * drive);

        // Delay time in samples.  Clamp to stay positive even if the user
        // dials Lag lower than Depth.
        float delayMs = lagMs + depthMs * mod;
        delayMs = juce::jlimit (1.0f, 55.0f, delayMs);
        const float delaySamples = delayMs * msToSamples;

        delay.pushSample (0, sat);
        float wet = delay.popSample (0, delaySamples, true);

        // Analog-style post-delay shaping.
        wet = bbd.processSample (wet);
        wet = deE.processSample (wet);
        wet = toneHp.processSample (wet);
        wet = toneMid.processSample (wet);
        wet = toneLp.processSample (wet);

        // D-C-V mix law with a subtle +1 dB bump at the centre.
        const float compDb = 1.0f * std::sin (mix * juce::MathConstants<float>::pi);
        const float comp   = juce::Decibels::decibelsToGain (compDb);
        return (in * (1.0f - mix) + wet * mix) * comp;
    }

    double sr = 44100.0;

    LagrangeDelay delayL, delayR;
    AnalogLFO     lfoL,   lfoR;

    juce::dsp::IIR::Filter<float> preEmphL, preEmphR;
    juce::dsp::IIR::Filter<float> deEmphL,  deEmphR;
    juce::dsp::IIR::Filter<float> bbdLpL,   bbdLpR;
    juce::dsp::IIR::Filter<float> toneHpL,  toneHpR;
    juce::dsp::IIR::Filter<float> toneMidL, toneMidR;
    juce::dsp::IIR::Filter<float> toneLpL,  toneLpR;

    juce::SmoothedValue<float> depthSmooth, lagSmooth, mixSmooth;
};

} // namespace LushDSP
