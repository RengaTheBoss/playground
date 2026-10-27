#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <set>

namespace
{
constexpr double kGate = 0.85;   // note length as a fraction of one step
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PlaygroundProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using CP = juce::AudioParameterChoice;
    using BP = juce::AudioParameterBool;
    using juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);

    layout.add (std::make_unique<FP> (ParameterID { "morph", 1 },  "Morph",  unit, 0.35f));
    layout.add (std::make_unique<FP> (ParameterID { "bright", 1 }, "Bright", unit, 0.60f));
    layout.add (std::make_unique<FP> (ParameterID { "time", 1 },   "Time",   unit, 0.35f));
    layout.add (std::make_unique<FP> (ParameterID { "motion", 1 }, "Motion", unit, 0.15f));
    layout.add (std::make_unique<FP> (ParameterID { "space", 1 },  "Space",  unit, 0.25f));
    layout.add (std::make_unique<FP> (ParameterID { "volume", 1 }, "Volume", unit, 0.70f));

    juce::StringArray roots;
    for (int i = 0; i < 12; ++i) roots.add (pg::noteNames()[i]);
    layout.add (std::make_unique<CP> (ParameterID { "root", 1 }, "Key", roots, 0));

    juce::StringArray scales;
    for (int i = 0; i < pg::numScales; ++i) scales.add (pg::scaleTable()[i].name);
    layout.add (std::make_unique<CP> (ParameterID { "scale", 1 }, "Scale", scales, 1));

    layout.add (std::make_unique<CP> (ParameterID { "octave", 1 }, "Octave",
                                      juce::StringArray { "Oct -2", "Oct -1", "Oct 0", "Oct +1", "Oct +2" }, 2));

    layout.add (std::make_unique<BP> (ParameterID { "chord", 1 }, "Chord", false));
    layout.add (std::make_unique<BP> (ParameterID { "snap", 1 },  "Snap to scale", false));
    layout.add (std::make_unique<BP> (ParameterID { "loop", 1 },  "Loop pattern", false));

    return layout;
}

PlaygroundProcessor::PlaygroundProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    morphP   = apvts.getRawParameterValue ("morph");
    brightP  = apvts.getRawParameterValue ("bright");
    timeP    = apvts.getRawParameterValue ("time");
    motionP  = apvts.getRawParameterValue ("motion");
    spaceP   = apvts.getRawParameterValue ("space");
    volumeP  = apvts.getRawParameterValue ("volume");
    rootP    = apvts.getRawParameterValue ("root");
    scaleP   = apvts.getRawParameterValue ("scale");
    octaveP  = apvts.getRawParameterValue ("octave");
    chordP   = apvts.getRawParameterValue ("chord");
    snapP    = apvts.getRawParameterValue ("snap");
    loopP    = apvts.getRawParameterValue ("loop");

    for (auto& p : pattern) p.store (0);
    snappedFor.fill (-1);

    for (int i = 0; i < 10; ++i)
        synth.addVoice (new pg::PlayVoice (soundParams));

    synth.addSound (new pg::PlaySound());
}

//==============================================================================
void PlaygroundProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    masterGain.reset (sampleRate, 0.03);
    masterGain.setCurrentAndTargetValue (volumeP->load() * volumeP->load() * 1.6f);

    patternMode = 0;
    lastTriggered = -1;
    held.fill (false);
    snappedFor.fill (-1);
    previewNote = -1;
    previewSamplesLeft = 0;
    synth.allNotesOff (0, false);
}

bool PlaygroundProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
int PlaygroundProcessor::getScaleSize() const
{
    return pg::scaleTable()[scaleIndex()].count;
}

int PlaygroundProcessor::noteForDegree (int degree) const
{
    const auto& sc = pg::scaleTable()[scaleIndex()];
    const int note = 60 + rootIndex() + octaveShift() * 12 + pg::degreeToSemitone (sc, degree);
    return juce::jlimit (0, 127, note);
}

bool PlaygroundProcessor::getCell (int step, int row) const
{
    if (step < 0 || step >= numSteps || row < 0 || row >= numRows) return false;
    return (pattern[(size_t) step].load() >> row) & 1;
}

void PlaygroundProcessor::setCell (int step, int row, bool on)
{
    if (step < 0 || step >= numSteps || row < 0 || row >= numRows) return;
    auto& cell = pattern[(size_t) step];
    const uint8_t bit = (uint8_t) (1u << row);
    const uint8_t cur = cell.load();
    cell.store (on ? (uint8_t) (cur | bit) : (uint8_t) (cur & ~bit));
}

void PlaygroundProcessor::clearPattern()
{
    for (auto& p : pattern) p.store (0);
}

void PlaygroundProcessor::randomiseMelody()
{
    juce::Random rng;
    std::array<uint8_t, numSteps> p {};

    static const int starts[] = { 0, 2, 4 };
    int row = starts[rng.nextInt (3)];
    const float density = 0.45f + rng.nextFloat() * 0.25f;
    int count = 0;

    for (int s = 0; s < numSteps; ++s)
    {
        const bool strong = (s % 4) == 0;
        const float prob = strong ? 0.9f : density;

        if (rng.nextFloat() < prob)
        {
            const int r = rng.nextInt (100);
            int move = 0;
            if (r < 28) move = 0;
            else if (r < 52) move = 1;
            else if (r < 76) move = -1;
            else if (r < 86) move = 2;
            else if (r < 94) move = -2;
            else move = rng.nextBool() ? 3 : -3;

            row = juce::jlimit (0, numRows - 1, row + move);
            p[(size_t) s] = (uint8_t) (1u << row);
            ++count;
        }
    }

    for (int s = 0; s < numSteps && count < 4; s += 4)
    {
        if (p[(size_t) s] == 0)
        {
            p[(size_t) s] = (uint8_t) (1u << rng.nextInt (numRows));
            ++count;
        }
    }

    for (int s = 0; s < numSteps; ++s)
        pattern[(size_t) s].store (p[(size_t) s]);
}

void PlaygroundProcessor::randomiseSound()
{
    juce::Random rng;

    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    };

    set ("morph",  rng.nextFloat());
    set ("bright", 0.3f + 0.7f * rng.nextFloat());
    set ("time",   rng.nextFloat());
    set ("motion", rng.nextFloat() * rng.nextFloat());
    set ("space",  0.1f + 0.6f * rng.nextFloat());
}

void PlaygroundProcessor::previewRow (int row)
{
    previewRequest.store (rowToNote (row));
}

//==============================================================================
void PlaygroundProcessor::releaseHeld (juce::MidiBuffer& events, int position)
{
    for (int n = 0; n < 128; ++n)
    {
        if (held[(size_t) n])
        {
            events.addEvent (juce::MidiMessage::noteOff (1, n), position);
            held[(size_t) n] = false;
        }
    }
}

void PlaygroundProcessor::emitStep (juce::MidiBuffer& events, long long stepIndex, int position)
{
    const int step = (int) (((stepIndex % numSteps) + numSteps) % numSteps);
    const uint8_t mask = pattern[(size_t) step].load();
    if (mask == 0) return;

    const bool chord = chordP->load() > 0.5f;
    const float velocity = (step % 4 == 0) ? 0.85f : 0.70f;

    auto add = [&] (int degree)
    {
        const int note = noteForDegree (degree);
        if (! held[(size_t) note])
        {
            events.addEvent (juce::MidiMessage::noteOn (1, note, velocity), position);
            held[(size_t) note] = true;
        }
    };

    for (int row = 0; row < numRows; ++row)
    {
        if ((mask >> row) & 1)
        {
            add (row);
            if (chord)
            {
                add (row + 2);
                add (row + 4);
            }
        }
    }

    triggerCount.fetch_add (1);
}

void PlaygroundProcessor::runPattern (juce::MidiBuffer& events, int numSamples)
{
    const bool on = loopP->load() > 0.5f;

    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false, havePpq = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; }
        }
    }

    if (bpm < 20.0 || bpm > 400.0) bpm = 120.0;
    lastBpm.store (bpm);

    const int mode = ! on ? 0 : ((hostPlaying && havePpq) ? 1 : 2);

    if (mode != patternMode)
    {
        releaseHeld (events, 0);
        lastTriggered = -1;
        if (mode == 2) freeStep = 0.0;
        patternMode = mode;
    }

    if (mode == 0)
    {
        currentStep.store (-1);
        return;
    }

    const double sps = bpm / 60.0 * 4.0 / currentSampleRate;   // steps per sample
    const double s0 = (mode == 1) ? ppq * 4.0 : freeStep;
    const double s1 = s0 + sps * (double) numSamples;
    constexpr double eps = 1.0e-6;

    currentStep.store ((int) (((((long long) std::floor (s0)) % numSteps) + numSteps) % numSteps));

    auto offsetOf = [&] (double t)
    {
        return juce::jlimit (0, numSamples - 1, (int) std::floor ((t - s0) / sps));
    };

    for (long long k = (long long) std::floor (s0 - kGate - eps); (double) k < s1 - eps; ++k)
    {
        const double onTime = (double) k;
        const double offTime = (double) k + kGate;

        if (onTime >= s0 - eps && onTime < s1 - eps && k != lastTriggered)
        {
            const int pos = offsetOf (onTime);
            releaseHeld (events, pos);
            emitStep (events, k, pos);
            lastTriggered = k;
        }

        if (offTime >= s0 - eps && offTime < s1 - eps)
            releaseHeld (events, offsetOf (offTime));
    }

    if (mode == 2) freeStep = s1;
}

//==============================================================================
void PlaygroundProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    soundParams.morph.store  (morphP->load());
    soundParams.bright.store (brightP->load());
    soundParams.time.store   (timeP->load());
    soundParams.motion.store (motionP->load());

    juce::MidiBuffer events;

    // incoming MIDI, optionally snapped to the scale
    const bool snap = snapP->load() > 0.5f;
    const auto& scale = pg::scaleTable()[scaleIndex()];

    for (const auto meta : midi)
    {
        auto msg = meta.getMessage();

        if (msg.isNoteOn())
        {
            const int orig = msg.getNoteNumber();
            const int n = snap ? pg::snapToScale (scale, rootIndex(), orig) : orig;
            snappedFor[(size_t) orig] = n;
            msg.setNoteNumber (n);
        }
        else if (msg.isNoteOff())
        {
            const int orig = msg.getNoteNumber();
            if (snappedFor[(size_t) orig] >= 0)
            {
                msg.setNoteNumber (snappedFor[(size_t) orig]);
                snappedFor[(size_t) orig] = -1;
            }
        }

        events.addEvent (msg, meta.samplePosition);
    }

    if (numSamples > 0)
    {
        // preview notes from clicking the grid
        const int request = previewRequest.exchange (-1);

        if (request >= 0)
        {
            if (previewNote >= 0)
                events.addEvent (juce::MidiMessage::noteOff (1, previewNote), 0);

            events.addEvent (juce::MidiMessage::noteOn (1, request, 0.8f), 0);
            previewNote = request;
            previewSamplesLeft = (int) (0.22 * currentSampleRate);
        }
        else if (previewNote >= 0)
        {
            previewSamplesLeft -= numSamples;

            if (previewSamplesLeft <= 0)
            {
                events.addEvent (juce::MidiMessage::noteOff (1, previewNote), numSamples - 1);
                previewNote = -1;
            }
        }

        runPattern (events, numSamples);
    }

    synth.renderNextBlock (buffer, events, 0, numSamples);

    // space (reverb)
    const float space = spaceP->load();
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.55f + 0.4f * space;
    rp.damping = 0.45f;
    rp.wetLevel = 0.33f * space;
    rp.dryLevel = 0.5f - 0.1f * space;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    if (buffer.getNumChannels() >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    else if (buffer.getNumChannels() == 1)
        reverb.processMono (buffer.getWritePointer (0), numSamples);

    // master volume + safety soft clip
    const float v = volumeP->load();
    masterGain.setTargetValue (v * v * 1.6f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            d[i] = std::tanh (d[i] * g);
        }
    }
}

//==============================================================================
juce::File PlaygroundProcessor::exportMidi()
{
    constexpr int ppqn = 480;
    constexpr int stepTicks = ppqn / 4;
    const int gateTicks = (int) (stepTicks * 0.9);

    juce::MidiMessageSequence seq;
    const double bpm = juce::jlimit (20.0, 300.0, lastBpm.load());

    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm));
    tempo.setTimeStamp (0.0);
    seq.addEvent (tempo);

    auto sig = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    sig.setTimeStamp (0.0);
    seq.addEvent (sig);

    const bool chord = chordP->load() > 0.5f;
    int noteCount = 0;

    for (int step = 0; step < numSteps; ++step)
    {
        const uint8_t mask = pattern[(size_t) step].load();
        if (mask == 0) continue;

        std::set<int> notes;

        for (int row = 0; row < numRows; ++row)
        {
            if ((mask >> row) & 1)
            {
                notes.insert (noteForDegree (row));
                if (chord)
                {
                    notes.insert (noteForDegree (row + 2));
                    notes.insert (noteForDegree (row + 4));
                }
            }
        }

        const juce::uint8 velocity = (step % 4 == 0) ? 108 : 92;

        for (int n : notes)
        {
            auto on = juce::MidiMessage::noteOn (1, n, velocity);
            on.setTimeStamp ((double) (step * stepTicks));
            seq.addEvent (on);

            auto off = juce::MidiMessage::noteOff (1, n);
            off.setTimeStamp ((double) (step * stepTicks + gateTicks));
            seq.addEvent (off);
            ++noteCount;
        }
    }

    if (noteCount == 0)
        return {};

    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppqn);
    mf.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PlaygroundMidi");
    dir.createDirectory();

    auto file = dir.getChildFile ("Playground_" + juce::Time::getCurrentTime().formatted ("%H%M%S")
                                  + "_" + juce::String (++exportCounter) + ".mid");

    if (auto os = file.createOutputStream())
    {
        os->setPosition (0);
        os->truncate();
        mf.writeTo (*os);
        os->flush();
    }

    return file;
}

//==============================================================================
void PlaygroundProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::StringArray parts;
    for (auto& p : pattern) parts.add (juce::String ((int) p.load()));
    state.setProperty ("pattern", parts.joinIntoString (","), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PlaygroundProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);

        if (tree.isValid() && tree.hasType (apvts.state.getType()))
        {
            apvts.replaceState (tree);

            auto tokens = juce::StringArray::fromTokens (tree.getProperty ("pattern").toString(), ",", "");

            for (int i = 0; i < numSteps; ++i)
                pattern[(size_t) i].store (i < tokens.size() ? (uint8_t) (tokens[i].getIntValue() & 0xff) : (uint8_t) 0);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* PlaygroundProcessor::createEditor()
{
    return new PlaygroundEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlaygroundProcessor();
}
