#pragma once

#include <juce_dsp/juce_dsp.h>

// Stoner Box signal path (a whole doom rig per channel, 8x oversampled where it distorts):
//
//   input -> tighten -> OVERDRIVE (chug: keeps the pick attack) -> FUZZ layered on top (drone, second half
//   of the Fuzz knob) -> preamp (triode) -> power amp with sag and a "depth" low boost inside the clipping
//   (Doom) -> auto gate -> rig voicing, Mids, Tone -> cab (closed-back thump, gentle low cut, steep top)
//   -> sub-octave (top of Doom) -> Wall (right side = rig B mids/highs as a drifting second take; lows
//   identical and centred, crossover-aligned delay) -> Space (dark room, lows dry) -> glue comp -> Output -> soft limiter
//
// Lessons measured on a real take (v1): low cuts after the clipping removed the 63-80 Hz fundamentals,
// a parallel clean-lows path cancelled the fuzz's own fundamentals, ~60 dB of stacked gain turned
// chords into buzz, and a delayed double summed into the lows notched 50-63 Hz. None of that is here.
//
// Rigs: 0 = Doom (Muff-style fuzz into an Orange/Matamp-style head, 4x12)
//       1 = Desert (fuzz into an Ampeg-style bass rig, 8x10)
//       2 = Wizard (germanium fuzz into a cranked Sunn/Laney-style amp)

struct StonerBoxParams
{
    int   rig      = 0;     // 0..2
    float fuzz     = 0.70f; // 0..1: first half overdrive (chug), second half fuzz on top (drone)
    float tone     = 0.50f; // 0..1 dark -> bright
    float mids     = 0.55f; // 0..1 (0 = Muff scoop, 0.5 = flat, 1 = push)
    float doom     = 0.50f; // 0..1 low-end depth in the power amp, cab thump, slower bloom; sub-octave at the top
    float wall     = 0.50f; // 0..1 mono -> dual-rig stereo
    float space    = 0.15f; // 0..1 room
    float outputDb = 0.0f;  // -12..+12 dB
};

class StonerBoxDSP
{
public:
    void prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        osRate = sampleRate * 8.0;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        oversampling.initProcessing ((size_t) maxBlockSize);
        reverb.prepare (spec);
        wetBuf.setSize (2, maxBlockSize);
        gateBuf.assign ((size_t) maxBlockSize, 1.0f);
        subSrc.assign ((size_t) maxBlockSize, 0.0f);
        wallDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.02) + 4);
        wallDelay.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });

        applyParams (params, true);
        outputGain.reset (sampleRate, 0.05);
        reset();
    }

    void reset()
    {
        oversampling.reset();
        reverb.reset();
        for (auto& c : chans) c.resetState();
        for (auto* b : { &subTapLp, &subLp[0], &subLp[1], &xLpA[0], &xLpA[1], &xHpA[0], &xHpA[1], &xHpB[0], &xHpB[1], &wetHp[0], &wetHp[1], &wetLp[0], &wetLp[1] })
            b->reset();
        gateEnv = 0.0f; gateGain = 0.0f; gateHold = 0; gateOpen = false;
        sub = {}; compEnv = 0.0f; compGainDb = 0.0f; driftPhase = 0.0; wallDelay.reset();
    }

    int getLatencySamples() const { return (int) std::round (oversampling.getLatencyInSamples()); }
    void setParams (const StonerBoxParams& p) { applyParams (p, false); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        jassert (buffer.getNumChannels() >= 2);
        const int n = buffer.getNumSamples();
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);

        // 1. Gate detector (clean input) + clean low tap for the sub-octave
        {
            const float aEnv = std::exp (-1.0f / (0.003f * (float) sampleRate));
            const float aOpen = std::exp (-1.0f / (0.0007f * (float) sampleRate));
            const float aClose = std::exp (-1.0f / ((0.12f - 0.06f * params.fuzz) * (float) sampleRate));
            const float thrOpen = juce::Decibels::decibelsToGain (-50.0f), thrClose = juce::Decibels::decibelsToGain (-58.0f);
            const int holdSamples = (int) (0.06 * sampleRate);
            for (int i = 0; i < n; ++i)
            {
                const float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                gateEnv = x > gateEnv ? x : aEnv * gateEnv + (1.0f - aEnv) * x;
                if (gateEnv > thrOpen) { gateOpen = true; gateHold = holdSamples; }
                else if (gateEnv < thrClose && --gateHold <= 0) gateOpen = false;
                const float target = gateOpen ? 1.0f : 0.0f;
                const float a = target > gateGain ? aOpen : aClose;
                gateGain = a * gateGain + (1.0f - a) * target;
                gateBuf[(size_t) i] = gateGain;
                subSrc[(size_t) i] = subTapLp.process (L[i]);
            }
        }

        // 2. Overdrive -> fuzz -> preamp -> power amp, 8x oversampled
        {
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
            auto up = oversampling.processSamplesUp (block);
            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto& c = chans[ch];
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = c.processNonlinear (d[i]);
            }
            oversampling.processSamplesDown (block);
        }

        // 3. Gate, voicing + cab
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            auto& c = chans[(size_t) ch];
            for (int i = 0; i < n; ++i)
                d[i] = c.processLinear (d[i] * gateBuf[(size_t) i]);
        }

        // 4. Sub-octave (top of Doom): flip-flop on the clean low tap, rounded into a sine-ish tone
        const float subAmt = juce::jlimit (0.0f, 1.0f, (params.doom - 0.7f) / 0.3f) * 0.5f;
        if (subAmt > 0.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float src = subSrc[(size_t) i];
                sub.env += 0.002f * (std::abs (src) - sub.env);
                const bool pos = src > 0.0f;
                if (pos && ! sub.wasPos) sub.flip = -sub.flip;
                sub.wasPos = pos;
                const float s = subLp[1].process (subLp[0].process (sub.flip * sub.env)) * subAmt * gateBuf[(size_t) i];
                L[i] += s; R[i] += s;
            }
        }

        // 5. Wall. Left = rig A. Right = rig A's lows (identical, so the low end stays centred and can't
        //    cancel) + rig B's mids/highs played ~12.5 ms later with a slow drift, like a second take.
        //    The delay is a whole number of cycles at the 160 Hz crossover, so the bands rejoin in phase.
        if (params.wall > 0.001f)
        {
            const float w = params.wall;
            const float base = (float) sampleRate * 0.0125f;
            const double inc = juce::MathConstants<double>::twoPi * 0.11 / sampleRate;
            for (int i = 0; i < n; ++i)
            {
                driftPhase += inc;
                if (driftPhase > juce::MathConstants<double>::twoPi) driftPhase -= juce::MathConstants<double>::twoPi;
                const float drift = (float) (std::sin (driftPhase) + 0.4 * std::sin (2.3 * driftPhase)) * 0.0003f * (float) sampleRate;
                const float a = L[i];
                const float lowA = xLpA[1].process (xLpA[0].process (a));
                const float highA = xHpA[1].process (xHpA[0].process (a));
                wallDelay.pushSample (0, R[i]);
                const float highB = xHpB[1].process (xHpB[0].process (wallDelay.popSample (0, base + drift)));
                L[i] = lowA + highA;
                R[i] = lowA + (1.0f - w) * highA + w * highB;
            }
        }
        else
        {
            for (auto& b : xLpA) b.reset();
            for (auto& b : xHpA) b.reset();
            for (auto& b : xHpB) b.reset();
            std::copy (L, L + n, R);
        }

        // 6. Space: dark room on a copy, lows kept dry
        if (params.space > 0.001f)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = wetBuf.getWritePointer (ch);
                const auto* d = buffer.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                    w[i] = wetLp[(size_t) ch].process (wetHp[(size_t) ch].process (d[i]));
            }
            juce::dsp::AudioBlock<float> wb (wetBuf.getArrayOfWritePointers(), 2, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> wctx (wb);
            reverb.process (wctx);
            for (int ch = 0; ch < 2; ++ch)
                buffer.addFrom (ch, 0, wetBuf, ch, 0, n);
        }

        // 7. Glue compressor (linked, slow enough to keep the chug), Output, soft limiter
        {
            const float aDet = std::exp (-1.0f / (0.010f * (float) sampleRate));
            const float aAtt = std::exp (-1.0f / (0.025f * (float) sampleRate));
            const float aRel = std::exp (-1.0f / (0.200f * (float) sampleRate));
            for (int i = 0; i < n; ++i)
            {
                const float x2 = 0.5f * (L[i] * L[i] + R[i] * R[i]);
                compEnv = aDet * compEnv + (1.0f - aDet) * x2;
                const float lvl = 10.0f * std::log10 (compEnv + 1.0e-12f);
                const float over = lvl - (-20.0f);
                const float targetDb = over > 0.0f ? -over * (1.0f - 1.0f / 2.0f) : 0.0f;
                const float a = targetDb < compGainDb ? aAtt : aRel;
                compGainDb = a * compGainDb + (1.0f - a) * targetDb;
                const float g = juce::Decibels::decibelsToGain (compGainDb) * outputGain.getNextValue();
                L[i] = softLimit (L[i] * g);
                R[i] = softLimit (R[i] * g);
            }
        }
    }

private:
    // ---------------------------------------------------------------- small building blocks
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (const std::array<float, 6>& c)
        {
            const float inv = 1.0f / c[3];
            b0 = c[0] * inv; b1 = c[1] * inv; b2 = c[2] * inv; a1 = c[4] * inv; a2 = c[5] * inv;
        }
        void reset() { z1 = z2 = 0.0f; }
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    struct OnePole // lp() = low-pass, hp() = x - low-pass
    {
        float a = 0.0f, z = 0.0f;
        void setCutoff (double fc, double fs) { a = (float) std::exp (-juce::MathConstants<double>::twoPi * fc / fs); }
        float lp (float x) { z = x + a * (z - x); return z; }
        float hp (float x) { return x - lp (x); }
    };

    using C = juce::dsp::IIR::ArrayCoefficients<float>;

    static float softLimit (float x)
    {
        const float k = 0.70f, room = 0.26f, ax = std::abs (x);
        if (ax <= k) return x;
        return std::copysign (k + room * std::tanh ((ax - k) / room), x);
    }

    // One complete rig (one per channel, so Wall can make left and right slightly different rigs)
    struct Channel
    {
        OnePole inHp, odPreLp, odLp, fzPreLp, fzLp, toneLp, toneHp, dcBlock, ampLp, depthLp;
        float odGain = 2, fzGain = 1, fzMix = 0, fzBias = 0.1f, muffMix = 0.5f, ampGain = 1.5f;
        float powerGain = 1.5f, depth = 0.5f, sagAmt = 0.3f, makeup = 1.0f;
        float sagEnv = 0.0f, sagAtt = 0.0f, sagRel = 0.0f, biasEnv = 0.0f, biasA = 0.0f;
        bool germanium = false;
        std::array<Biquad, 10> eq;

        void resetState()
        {
            for (auto* f : { &inHp, &odPreLp, &odLp, &fzPreLp, &fzLp, &toneLp, &toneHp, &dcBlock, &ampLp, &depthLp }) f->z = 0.0f;
            for (auto& b : eq) b.reset();
            sagEnv = 0.0f; biasEnv = 0.0f;
        }

        float processNonlinear (float x)
        {
            // --- overdrive: tighten, tame the top before clipping, soft asymmetric clip (keeps the chug)
            float s = odPreLp.lp (inHp.hp (x)) * odGain;
            s = s >= 0.0f ? std::tanh (s) : std::tanh (0.8f * s) * 1.1f;
            s = odLp.lp (s);

            // --- fuzz layered on top (second half of the knob): sustain + drone, Muff-style tone blend
            if (fzMix > 0.0f)
            {
                float f = fzPreLp.lp (s) * fzGain;
                biasEnv = biasA * biasEnv + (1.0f - biasA) * std::abs (f);
                const float b = germanium ? fzBias - 0.25f * juce::jmin (biasEnv, 2.0f) : fzBias;
                f = std::tanh (f + b) - std::tanh (b);
                f = fzLp.lp (f);
                const float lo = toneLp.lp (f), hi = toneHp.hp (f);
                f = (1.0f - muffMix) * lo + muffMix * hi * 1.4f;
                s = (1.0f - fzMix) * s + fzMix * f * 1.2f;
            }

            // --- preamp: triode-ish asymmetric clip, no low cut (keeps the 70-100 Hz fundamentals)
            s = dcBlock.hp (s) * ampGain;
            s = s >= 0.0f ? std::tanh (s) : s / (1.0f + 0.6f * std::abs (s));
            s = ampLp.lp (s);

            // --- power amp: Doom boosts the lows INTO the clipping (they saturate and pump), sag blooms
            s += depth * depthLp.lp (s);
            const float e = std::abs (s);
            sagEnv = e > sagEnv ? sagAtt * sagEnv + (1.0f - sagAtt) * e : sagRel * sagEnv + (1.0f - sagRel) * e;
            s = std::tanh (powerGain * s * (1.0f - sagAmt * juce::jmin (1.0f, sagEnv)));
            return s * makeup;
        }

        float processLinear (float x)
        {
            for (auto& b : eq) x = b.process (x);
            return x;
        }
    };

    struct Rig
    {
        float inHp, odMax, fzMax, fzBias, toneLp, toneHp, muffMix, ampGain, ampLp, sag;
        float voiceF, voiceG, voiceQ;
        float cabHp, cabResF, cabResG, cabBreakF, cabBreakG, cabLp1, cabLp2;
        bool germanium;
    };

    static const Rig& rigFor (int index)
    {
        static const Rig rigs[3] = {
            // Doom: Muff into an Orange/Matamp-style head, closed-back 4x12
            { 55, 9.0f, 7.0f, 0.10f, 420, 1100, 0.45f, 1.8f, 7500, 0.30f,  320, -3.5f, 0.9f,
              45, 95, 3.0f, 2300, 4.5f, 5400, 7500, false },
            // Desert: fuzz into an Ampeg-style bass rig, 8x10: fuller low mids, darker top
            { 40, 7.0f, 6.0f, 0.06f, 380, 1000, 0.55f, 1.6f, 6500, 0.40f,  300, -2.0f, 0.9f,
              40, 85, 3.5f, 1900, 4.0f, 4800, 6500, false },
            // Wizard: germanium fuzz into a cranked Sunn/Laney-style amp, open and raw
            { 70, 8.0f, 6.0f, 0.25f, 500, 1300, 0.50f, 2.2f, 8000, 0.25f,  350, -3.5f, 0.9f,
              50, 100, 2.5f, 2600, 4.5f, 5600, 8000, true },
        };
        return rigs[juce::jlimit (0, 2, index)];
    }

    static bool changed (float a, float b) { return std::abs (a - b) > 1.0e-4f; }

    void configureChannel (Channel& c, const StonerBoxParams& p, float v)
    {
        // v = 0: rig A (left / mono). v = 1: rig B for the right side (more drive, other voicing, other mic spot)
        const auto& r = rigFor (p.rig);
        const double os = osRate;
        c.germanium = r.germanium;
        c.inHp.setCutoff (r.inHp, os);
        c.odPreLp.setCutoff (4500.0 * (1.0 - 0.15 * v), os);
        c.odLp.setCutoff (6500.0, os);
        c.fzPreLp.setCutoff (3500.0, os);
        c.fzLp.setCutoff (5500.0 * (1.0 - 0.1 * v), os);
        c.toneLp.setCutoff (r.toneLp, os);
        c.toneHp.setCutoff (r.toneHp, os);
        c.dcBlock.setCutoff (20.0, os);
        c.ampLp.setCutoff (r.ampLp, os);
        c.depthLp.setCutoff (110.0, os);

        // Fuzz knob: 0..0.5 brings the overdrive up, 0.5..1 layers the fuzz on top
        const float od = juce::jmin (1.0f, p.fuzz * 2.0f);
        const float fz = juce::jlimit (0.0f, 1.0f, (p.fuzz - 0.5f) * 2.0f);
        c.odGain = (1.0f + (r.odMax - 1.0f) * od) * (1.0f + 0.25f * v);
        c.fzMix = fz;
        c.fzGain = 1.0f + (r.fzMax - 1.0f) * fz;
        c.fzBias = r.fzBias * (1.0f + 0.6f * v);
        c.muffMix = juce::jlimit (0.25f, 0.75f, r.muffMix + 0.2f * (p.mids - 0.5f));
        c.ampGain = r.ampGain * (1.0f + 0.15f * v);
        c.powerGain = 1.2f + 0.5f * p.fuzz;
        c.depth = 0.6f + 2.4f * p.doom;
        c.sagAmt = r.sag;
        c.sagAtt = (float) std::exp (-1.0 / (0.010 * os));
        c.sagRel = (float) std::exp (-1.0 / ((0.10 + 0.25 * p.doom) * os));
        c.biasA = (float) std::exp (-1.0 / (0.03 * os));
        c.makeup = 0.9f;

        // --- linear voicing + cab (base rate)
        const double fs = sampleRate;
        const float vf = 1.0f + 0.15f * v;
        auto& e = c.eq;
        e[0].set (C::makePeakFilter (fs, r.voiceF * vf, r.voiceQ, juce::Decibels::decibelsToGain (r.voiceG)));
        // Mids (the only mid control): -8 dB Muff scoop .. +5 dB push around 650 Hz
        const float midDb = p.mids < 0.5f ? -16.0f * (0.5f - p.mids) : 10.0f * (p.mids - 0.5f);
        e[1].set (C::makePeakFilter (fs, 650.0f * vf, 0.8f, juce::Decibels::decibelsToGain (midDb)));
        // Tone: tilt around 1 kHz, +-5 dB
        e[2].set (C::makeHighShelf (fs, 1000.0f, 0.6f, juce::Decibels::decibelsToGain (10.0f * (p.tone - 0.5f))));
        // Cab: gentle low cut, closed-back thump (grows with Doom), mild cone breakup, steep top
        e[3].set (C::makeHighPass (fs, r.cabHp, 0.7f));
        e[4].set (C::makePeakFilter (fs, r.cabResF, 1.2f, juce::Decibels::decibelsToGain (r.cabResG * (0.4f + 1.2f * p.doom))));
        e[5].set (C::makePeakFilter (fs, r.cabBreakF * vf, 1.0f, juce::Decibels::decibelsToGain (r.cabBreakG)));
        const float lpScale = 0.85f + 0.35f * p.tone;
        e[6].set (C::makeLowPass (fs, juce::jmin (r.cabLp1 * lpScale, (float) fs * 0.45f), 0.7f));
        e[7].set (C::makeLowPass (fs, juce::jmin (r.cabLp2 * lpScale, (float) fs * 0.45f), 1.0f));
        // second mic spot on rig B: small dip/peak shift in the upper mids
        e[8].set (C::makePeakFilter (fs, 1500.0f, 1.0f, juce::Decibels::decibelsToGain (-2.0f * v)));
        // Doom weight after the cab: -1 .. +4 dB low shelf
        e[9].set (C::makeLowShelf (fs, 90.0f, 0.7f, juce::Decibels::decibelsToGain (-1.0f + 5.0f * p.doom)));
    }

    void applyParams (const StonerBoxParams& p, bool force)
    {
        const bool tonal = force || p.rig != params.rig || changed (p.fuzz, params.fuzz) || changed (p.tone, params.tone)
                        || changed (p.mids, params.mids) || changed (p.doom, params.doom) || changed (p.wall, params.wall);
        if (tonal)
        {
            configureChannel (chans[0], p, 0.0f);
            configureChannel (chans[1], p, 1.0f); // rig B: Wall decides how much of it the right side hears
            subTapLp.set (C::makeLowPass (sampleRate, 110.0f, 0.7f));
            subLp[0].set (C::makeLowPass (sampleRate, 70.0f, 0.7f));
            subLp[1].set (C::makeLowPass (sampleRate, 70.0f, 0.7f));
            for (int i = 0; i < 2; ++i) // Linkwitz-Riley crossover at 160 Hz for Wall
            {
                xLpA[(size_t) i].set (C::makeLowPass (sampleRate, 160.0f, 0.7071f));
                xHpA[(size_t) i].set (C::makeHighPass (sampleRate, 160.0f, 0.7071f));
                xHpB[(size_t) i].set (C::makeHighPass (sampleRate, 160.0f, 0.7071f));
            }
            for (auto& b : wetHp)  b.set (C::makeHighPass (sampleRate, 280.0f, 0.707f));
            for (auto& b : wetLp)  b.set (C::makeLowPass (sampleRate, 3500.0f, 0.707f));
        }

        if (force || changed (p.space, params.space))
        {
            juce::dsp::Reverb::Parameters rp;
            rp.roomSize = 0.45f + 0.45f * p.space;
            rp.damping  = 0.7f;
            rp.width    = 1.0f;
            rp.wetLevel = 0.55f * p.space / 3.0f; // juce::dsp::Reverb scales wet by 3
            rp.dryLevel = 0.0f;                   // wet-only: the dry signal is never touched
            reverb.setParameters (rp);
        }

        if (force || changed (p.outputDb, params.outputDb))
            outputGain.setTargetValue (juce::Decibels::decibelsToGain (p.outputDb - 6.0f));

        params = p;
    }

    double sampleRate = 44100.0, osRate = 352800.0;
    StonerBoxParams params;

    std::array<Channel, 2> chans;
    Biquad subTapLp;
    std::array<Biquad, 2> subLp, xLpA, xHpA, xHpB, wetHp, wetLp;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> wallDelay { 4096 };
    double driftPhase = 0.0;
    juce::dsp::Oversampling<float> oversampling { 2, 3, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuf;
    std::vector<float> gateBuf, subSrc;

    struct SubState { float env = 0.0f, flip = 1.0f; bool wasPos = false; } sub;
    float gateEnv = 0.0f, gateGain = 0.0f; int gateHold = 0; bool gateOpen = false;
    float compEnv = 0.0f, compGainDb = 0.0f;
    juce::SmoothedValue<float> outputGain { 1.0f };
};
