#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace pg
{
constexpr float kTwoPi = 6.28318530717958647692f;

/** Sound settings shared between the processor (writes) and the voices (read). */
struct SoundParams
{
    std::atomic<float> morph  { 0.35f };
    std::atomic<float> bright { 0.60f };
    std::atomic<float> time   { 0.35f };
    std::atomic<float> motion { 0.15f };
};

namespace dsp
{
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

inline float fract (float x) { return x - std::floor (x); }

/** 0 = sine, 1 = triangle, 2 = saw, 3 = square. All start at zero crossing, rising. */
inline float shape (int which, float phase, float dt)
{
    switch (which)
    {
        case 0:  return std::sin (kTwoPi * phase);
        case 1:  return 1.0f - 4.0f * std::abs (fract (phase + 0.25f) - 0.5f);
        case 2:
        {
            const float t = fract (phase + 0.5f);
            return 2.0f * t - 1.0f - polyBlep (t, dt);
        }
        default:
        {
            const float t = fract (phase + 0.5f);
            float v = t < 0.5f ? 1.0f : -1.0f;
            v += polyBlep (t, dt);
            v -= polyBlep (fract (t + 0.5f), dt);
            return v;
        }
    }
}

/** Continuous morph: 0 = sine, 0.33 = triangle, 0.66 = saw, 1 = square. */
inline float morphWave (float morph, float phase, float dt)
{
    static constexpr float gains[4] = { 1.0f, 1.0f, 0.55f, 0.5f };
    const float m = juce::jlimit (0.0f, 1.0f, morph) * 3.0f;
    const int i = juce::jmin ((int) m, 2);
    const float f = m - (float) i;
    const float a = shape (i, phase, dt) * gains[i];
    const float b = shape (i + 1, phase, dt) * gains[i + 1];
    return a + (b - a) * f;
}

/** Zavalishin TPT state variable filter (low-pass output). */
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void set (float cutoff, float k, float sampleRate)
    {
        const float g = std::tan (3.14159265f * cutoff / sampleRate);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float lowpass (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    void reset() { ic1 = ic2 = 0.0f; }
};
} // namespace dsp

class PlaySound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class PlayVoice : public juce::SynthesiserVoice
{
public:
    explicit PlayVoice (const SoundParams& p) : params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<PlaySound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        sr = (float) getSampleRate();
        baseFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        vel = velocity;

        phaseA = 0.0f;
        phaseB = 0.37f;
        lfoPhase = 0.0f;
        filterL.reset();
        filterR.reset();

        const float t = params.time.load();

        adsr.setSampleRate ((double) sr);
        adsr.setParameters ({ 0.002f + t * t * 0.9f,   // attack
                              0.15f + t * 0.7f,        // decay
                              t * 0.85f,               // sustain
                              0.12f + t * 1.6f });     // release
        adsr.noteOn();

        fEnv = 1.0f;
        fEnvDecay = std::exp (-1.0f / ((0.10f + t * 0.8f) * sr));
        fEnvAmount = 3.0f * (1.0f - 0.7f * t);
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    using juce::SynthesiserVoice::renderNextBlock;

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (! adsr.isActive())
        {
            clearCurrentNote();
            return;
        }

        const float morph  = params.morph.load();
        const float bright = params.bright.load();
        const float motion = params.motion.load();

        const float detune = 0.0035f;   // about +/- 6 cents
        const float freqA = baseFreq * (1.0f - detune);
        const float freqB = baseFreq * (1.0f + detune);
        const float incA = freqA / sr;
        const float incB = freqB / sr;

        const float lfoRate = 0.35f + motion * 6.5f;
        const float lfoInc = kTwoPi * lfoRate * 16.0f / sr;
        const float keyTrack = 0.3f * std::log2 (baseFreq / 261.63f);
        const float velOct = (vel - 0.5f) * 1.0f;
        const float k = 1.2f - 0.5f * bright;
        const float gain = 0.22f * (0.35f + 0.65f * vel);
        const float maxCutoff = juce::jmin (18000.0f, sr * 0.45f);

        const int nCh = out.getNumChannels();
        float* l = out.getWritePointer (0, startSample);
        float* r = nCh > 1 ? out.getWritePointer (1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            if ((i & 15) == 0)
            {
                const float lfo = std::sin (lfoPhase);
                lfoPhase += lfoInc;
                if (lfoPhase > kTwoPi) lfoPhase -= kTwoPi;

                const float oct = bright * 7.8f + keyTrack + velOct
                                  + fEnvAmount * fEnv * (1.0f - 0.6f * bright)
                                  + lfo * motion * 2.2f;
                const float cutoff = juce::jlimit (40.0f, maxCutoff, 70.0f * std::exp2 (oct));
                filterL.set (cutoff, k, sr);
                filterR.a1 = filterL.a1;
                filterR.a2 = filterL.a2;
                filterR.a3 = filterL.a3;
            }

            const float oa = dsp::morphWave (morph, phaseA, incA);
            const float ob = dsp::morphWave (morph, phaseB, incB);

            phaseA += incA; if (phaseA >= 1.0f) phaseA -= 1.0f;
            phaseB += incB; if (phaseB >= 1.0f) phaseB -= 1.0f;

            const float env = adsr.getNextSample();
            fEnv *= fEnvDecay;

            const float sl = filterL.lowpass (oa * 0.75f + ob * 0.25f) * env * gain;
            const float sr_ = filterR.lowpass (oa * 0.25f + ob * 0.75f) * env * gain;

            l[i] += sl;
            if (r != nullptr) r[i] += sr_; else l[i] += sr_;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    const SoundParams& params;
    juce::ADSR adsr;
    dsp::Svf filterL, filterR;

    float sr = 44100.0f, baseFreq = 440.0f, vel = 0.8f;
    float phaseA = 0.0f, phaseB = 0.0f, lfoPhase = 0.0f;
    float fEnv = 1.0f, fEnvDecay = 0.999f, fEnvAmount = 2.0f;
};
} // namespace pg
