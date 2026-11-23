#pragma once

#include <juce_dsp/juce_dsp.h>

// Jazz Box signal path (inspired by clean jazz amp sims such as Sonatura Solid Soul
// and the Polytone-style "amplify cleanly, shape with cab + mic + room" approach):
//
//   Squeeze (soft-knee compressor) -> Warmth (2x oversampled asymmetric tube-ish saturation)
//   -> Cab/Mic EQ (80 Hz HPF, Wood bell @ 200 Hz, body bell @ 700 Hz, pick-click dip @ 2.5 kHz,
//      Dark ribbon-style low-pass + high shelf) -> Room (small studio reverb) -> Output gain
//
// The class is shared by the plugin and the offline render tool so both sound identical.

struct JazzBoxParams
{
    float warmth   = 0.35f; // 0..1  drive into the saturation stage
    float wood     = 0.50f; // 0..1  0..+7 dB bell at 200 Hz
    float dark     = 0.50f; // 0..1  low-pass 9 kHz -> 2.5 kHz
    float squeeze  = 0.40f; // 0..1  compression amount (auto makeup)
    float room     = 0.25f; // 0..1  room reverb amount
    float outputDb = 0.0f;  // -12..+12 dB
};

class JazzBoxDSP
{
public:
    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };

        for (auto* f : { &hpf, &woodBell, &bodyBell, &clickDip, &lowPass, &highShelf })
            f->prepare (spec);

        compressor.prepare (spec);
        compressor.setAttack (15.0f);
        compressor.setRelease (120.0f);

        oversampling.initProcessing ((size_t) maxBlockSize);

        reverb.prepare (spec);

        *hpf.state      = Coeffs::makeHighPass (sampleRate, 80.0f, 0.707f);
        *bodyBell.state = Coeffs::makePeakFilter (sampleRate, 700.0f, 0.9f, juce::Decibels::decibelsToGain (1.5f));
        *clickDip.state = Coeffs::makePeakFilter (sampleRate, 2500.0f, 1.0f, juce::Decibels::decibelsToGain (-2.5f));

        outputGain.reset (sampleRate, 0.05);
        makeupGain.reset (sampleRate, 0.05);
        drive.reset (sampleRate, 0.05);

        applyParams (params, true);
        reset();
    }

    void reset()
    {
        for (auto* f : { &hpf, &woodBell, &bodyBell, &clickDip, &lowPass, &highShelf })
            f->reset();
        compressor.reset();
        oversampling.reset();
        reverb.reset();
    }

    int getLatencySamples() const { return (int) std::round (oversampling.getLatencyInSamples()); }

    void setParams (const JazzBoxParams& p) { applyParams (p, false); }

    // Processes a stereo buffer in place.
    void process (juce::AudioBuffer<float>& buffer)
    {
        jassert (buffer.getNumChannels() >= 2);
        const int numSamples = buffer.getNumSamples();

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        // 1. Squeeze
        if (params.squeeze > 0.001f)
            compressor.process (ctx);
        applyRampedGain (block, makeupGain);

        // 2. Warmth (2x oversampled)
        {
            auto up = oversampling.processSamplesUp (block);
            const float bias = 0.12f * params.warmth;
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                auto driveCopy = drive;
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float k   = driveCopy.getNextValue();
                    const float off = std::tanh (k * bias);
                    const float norm = k * (1.0f - off * off); // unity small-signal gain
                    d[i] = (std::tanh (k * (d[i] + bias)) - off) / norm;
                }
                if (ch == up.getNumChannels() - 1)
                    drive = driveCopy;
            }
            oversampling.processSamplesDown (block);
        }

        // 3. Cab / mic voicing
        hpf.process (ctx);       // also removes DC introduced by the bias
        woodBell.process (ctx);
        bodyBell.process (ctx);
        clickDip.process (ctx);
        lowPass.process (ctx);
        highShelf.process (ctx);

        // 4. Room
        if (params.room > 0.001f)
            reverb.process (ctx);

        // 5. Output
        applyRampedGain (block, outputGain);
    }

private:
    static void applyRampedGain (juce::dsp::AudioBlock<float>& block, juce::SmoothedValue<float>& g)
    {
        if (! g.isSmoothing())
        {
            block.multiplyBy (g.getTargetValue());
            return;
        }
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            const float v = g.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                block.getChannelPointer (ch)[i] *= v;
        }
    }

    static bool changed (float a, float b) { return std::abs (a - b) > 1.0e-4f; }

    void applyParams (const JazzBoxParams& p, bool force)
    {
        if (force || changed (p.squeeze, params.squeeze))
        {
            const float thresholdDb = -6.0f - 24.0f * p.squeeze;
            compressor.setThreshold (thresholdDb);
            compressor.setRatio (1.5f + 2.5f * p.squeeze);
            makeupGain.setTargetValue (juce::Decibels::decibelsToGain (6.0f * p.squeeze));
        }

        if (force || changed (p.warmth, params.warmth))
            drive.setTargetValue (1.0f + 5.0f * p.warmth);

        if (force || changed (p.wood, params.wood))
            *woodBell.state = Coeffs::makePeakFilter (sampleRate, 200.0f, 0.8f,
                                                      juce::Decibels::decibelsToGain (7.0f * p.wood));

        if (force || changed (p.dark, params.dark))
        {
            const float cutoff = 9000.0f * std::pow (2500.0f / 9000.0f, p.dark);
            *lowPass.state   = Coeffs::makeLowPass (sampleRate, cutoff, 0.707f);
            *highShelf.state = Coeffs::makeHighShelf (sampleRate, 4000.0f, 0.7f,
                                                      juce::Decibels::decibelsToGain (-3.0f * p.dark));
        }

        if (force || changed (p.room, params.room))
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize   = 0.35f + 0.30f * p.room;
            rp.damping    = 0.75f;
            rp.width      = 1.0f;
            // juce::dsp::Reverb scales dry by 2 and wet by 3 internally; compensate so
            // dry stays at unity and the knob only adds room.
            rp.wetLevel   = 0.35f * p.room / 3.0f;
            rp.dryLevel   = (1.0f - 0.25f * p.room) / 2.0f;
            rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
        }

        if (force || changed (p.outputDb, params.outputDb))
            outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb));

        params = p;
    }

    double sampleRate = 44100.0;
    JazzBoxParams params;

    Filter hpf, woodBell, bodyBell, clickDip, lowPass, highShelf;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> outputGain { 1.0f }, makeupGain { 1.0f }, drive { 1.0f };
};
