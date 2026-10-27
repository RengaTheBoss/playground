// Headless checks for the Playground plugin (engine, pattern player, MIDI export, state, editor snapshot).
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (! (cond)) { ++failures; std::printf ("FAIL line %d: %s\n", __LINE__, #cond); } \
    } while (false)

static void setParam (PlaygroundProcessor& p, const char* id, float normalised)
{
    p.apvts.getParameter (id)->setValueNotifyingHost (normalised);
}

struct Stats { float peak = 0.0f; double sumSq = 0.0; long n = 0; bool finite = true; };

static void accumulate (Stats& s, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const float* d = b.getReadPointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            if (! std::isfinite (d[i])) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (d[i]));
            s.sumSq += (double) d[i] * d[i];
            ++s.n;
        }
    }
}

static int countNoteOns (const juce::MidiMessageSequence& seq)
{
    int n = 0;
    for (int i = 0; i < seq.getNumEvents(); ++i)
        if (seq.getEventPointer (i)->message.isNoteOn()) ++n;
    return n;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // ---- scale maths ------------------------------------------------------
    {
        const auto& minor = pg::scaleTable()[1];
        CHECK (pg::snapToScale (minor, 0, 60) == 60);
        CHECK (pg::snapToScale (minor, 0, 61) == 60);   // tie goes down
        CHECK (pg::snapToScale (minor, 0, 64) == 63);
        CHECK (pg::snapToScale (minor, 2, 63) == 62);   // D minor: D E F G A Bb C
        CHECK (pg::degreeToSemitone (minor, 0) == 0);
        CHECK (pg::degreeToSemitone (minor, 7) == 12);
        CHECK (pg::degreeToSemitone (minor, 9) == 15);
        CHECK (pg::degreeToSemitone (minor, -1) == -2);
        const auto& pent = pg::scaleTable()[7];
        CHECK (pg::degreeToSemitone (pent, 5) == 12);
        CHECK (pg::degreeToSemitone (pent, 7) == 16);
    }

    // ---- sound engine: a held note makes sound, never NaN, never clips ------------------------
    {
        const float morphs[] = { 0.0f, 0.34f, 0.67f, 1.0f };
        const float brights[] = { 0.0f, 0.5f, 1.0f };
        const float times[] = { 0.0f, 1.0f };

        for (float m : morphs)
            for (float b : brights)
                for (float t : times)
                {
                    PlaygroundProcessor p;
                    p.setRateAndBufferSizeDetails (44100.0, 512);
                    p.prepareToPlay (44100.0, 512);
                    setParam (p, "morph", m);
                    setParam (p, "bright", b);
                    setParam (p, "time", t);
                    setParam (p, "motion", 1.0f);
                    setParam (p, "space", 1.0f);

                    Stats s;
                    juce::AudioBuffer<float> buf (2, 512);

                    for (int blk = 0; blk < 60; ++blk)
                    {
                        juce::MidiBuffer midi;
                        if (blk == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0);
                        if (blk == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 84, 0.9f), 0);
                        if (blk == 40) midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
                        p.processBlock (buf, midi);
                        accumulate (s, buf);
                    }

                    CHECK (s.finite);
                    CHECK (s.peak > 0.001f);
                    CHECK (s.peak <= 1.0f);
                }
        std::printf ("sound sweep ok\n");
    }

    // ---- release really ends: after note-off + tail the output is silent ---------------------
    {
        PlaygroundProcessor p;
        p.setRateAndBufferSizeDetails (44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        setParam (p, "time", 0.0f);
        setParam (p, "space", 0.0f);
        juce::AudioBuffer<float> buf (2, 512);
        Stats tail;

        for (int blk = 0; blk < 200; ++blk)
        {
            juce::MidiBuffer midi;
            if (blk == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            if (blk == 5) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            p.processBlock (buf, midi);
            if (blk >= 150) accumulate (tail, buf);
        }
        CHECK (tail.peak < 1.0e-4f);
    }

    // ---- pattern player (free-running, 120 bpm: 16 steps = 2 s) --------------------------------
    {
        PlaygroundProcessor p;
        p.setRateAndBufferSizeDetails (44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        p.setCell (0, 0, true);
        p.setCell (4, 3, true);
        p.setCell (8, 0, true);
        p.setCell (12, 5, true);
        setParam (p, "loop", 1.0f);

        Stats s;
        juce::AudioBuffer<float> buf (2, 512);
        int maxStep = -1;

        for (int blk = 0; blk < 172; ++blk)
        {
            juce::MidiBuffer midi;
            p.processBlock (buf, midi);
            accumulate (s, buf);
            maxStep = juce::jmax (maxStep, p.getCurrentStep());
        }

        CHECK (p.getTriggerCount() == 4);
        CHECK (maxStep >= 14);
        CHECK (s.finite);
        CHECK (s.peak > 0.01f);

        setParam (p, "loop", 0.0f);
        juce::MidiBuffer midi;
        p.processBlock (buf, midi);
        CHECK (p.getCurrentStep() == -1);
        std::printf ("pattern player ok (peak %.3f)\n", s.peak);
    }

    // ---- MIDI export -------------------------------------------------------------------
    {
        PlaygroundProcessor p;
        CHECK (! p.exportMidi().existsAsFile());   // empty pattern

        p.setCell (0, 0, true);
        p.setCell (4, 3, true);
        p.setCell (8, 0, true);
        p.setCell (12, 5, true);

        for (int pass = 0; pass < 2; ++pass)
        {
            const bool chord = pass == 1;
            setParam (p, "chord", chord ? 1.0f : 0.0f);
            auto f = p.exportMidi();
            CHECK (f.existsAsFile());

            juce::FileInputStream in (f);
            juce::MidiFile mf;
            CHECK (in.openedOk());
            CHECK (mf.readFrom (in));
            CHECK (mf.getNumTracks() == 1);
            CHECK (mf.getTimeFormat() == 480);

            const auto* tr = mf.getTrack (0);
            CHECK (countNoteOns (*tr) == (chord ? 12 : 4));

            // C minor, default octave: row 0 -> 60, row 3 (4th degree) -> 65
            int firstNote = -1, secondTick = -1, secondNote = -1;
            for (int i = 0; i < tr->getNumEvents(); ++i)
            {
                const auto& m = tr->getEventPointer (i)->message;
                if (m.isNoteOn())
                {
                    if (firstNote < 0) firstNote = m.getNoteNumber();
                    else if (secondTick < 0 && m.getTimeStamp() > 0) { secondTick = (int) m.getTimeStamp(); secondNote = m.getNoteNumber(); }
                }
            }
            CHECK (firstNote == 60);
            CHECK (secondTick == 480);
            CHECK (secondNote == 65);

            if (chord)
            {
                // first chord: C Eb G (degrees 0, 2, 4 of C minor)
                bool c = false, eb = false, g = false;
                for (int i = 0; i < tr->getNumEvents(); ++i)
                {
                    const auto& m = tr->getEventPointer (i)->message;
                    if (m.isNoteOn() && m.getTimeStamp() == 0.0)
                    {
                        c |= m.getNoteNumber() == 60;
                        eb |= m.getNoteNumber() == 63;
                        g |= m.getNoteNumber() == 67;
                    }
                }
                CHECK (c && eb && g);
            }
        }
        std::printf ("midi export ok\n");
    }

    // ---- random melody is sane ---------------------------------------------------------
    {
        PlaygroundProcessor p;
        for (int run = 0; run < 100; ++run)
        {
            p.randomiseMelody();
            int notes = 0;
            for (int s = 0; s < PlaygroundProcessor::numSteps; ++s)
            {
                int inStep = 0;
                for (int r = 0; r < PlaygroundProcessor::numRows; ++r) inStep += p.getCell (s, r) ? 1 : 0;
                CHECK (inStep <= 1);
                notes += inStep;
            }
            CHECK (notes >= 4);
        }
        p.clearPattern();
        CHECK (! p.exportMidi().existsAsFile());
    }

    // ---- state round trip ---------------------------------------------------------------
    {
        PlaygroundProcessor a;
        a.setCell (4, 3, true);
        a.setCell (15, 7, true);
        setParam (a, "morph", 0.8f);

        juce::MemoryBlock mb;
        a.getStateInformation (mb);

        PlaygroundProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (b.getCell (4, 3));
        CHECK (b.getCell (15, 7));
        CHECK (! b.getCell (1, 1));
        CHECK (std::abs (b.apvts.getRawParameterValue ("morph")->load() - 0.8f) < 0.01f);
    }

    // ---- editor snapshot ----------------------------------------------------------------
    {
        PlaygroundProcessor p;
        p.setRateAndBufferSizeDetails (44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        setParam (p, "morph", 0.72f);
        setParam (p, "bright", 0.78f);
        setParam (p, "motion", 0.4f);
        setParam (p, "space", 0.5f);
        setParam (p, "loop", 1.0f);
        p.randomiseMelody();

        juce::AudioBuffer<float> buf (2, 512);
        for (int blk = 0; blk < 90; ++blk)
        {
            juce::MidiBuffer midi;
            p.processBlock (buf, midi);
        }

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        CHECK (ed != nullptr);

        if (ed != nullptr)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);

            juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "editor.png");
            out.deleteFile();
            if (auto os = out.createOutputStream())
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (img, *os);
            }
            std::printf ("snapshot: %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        }
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
