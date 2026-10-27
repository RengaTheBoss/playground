#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Scales.h"
#include "Synth.h"

class PlaygroundProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSteps = 16;
    static constexpr int numRows = 8;

    PlaygroundProcessor();
    ~PlaygroundProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Playground"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Used by the editor
    juce::AudioProcessorValueTreeState apvts;

    bool getCell (int step, int row) const;
    void setCell (int step, int row, bool on);
    void clearPattern();
    void randomiseMelody();
    void randomiseSound();

    /** Plays a short preview note for a grid row (picked up by the audio thread). */
    void previewRow (int row);

    int noteForDegree (int degree) const;
    int rowToNote (int row) const { return noteForDegree (row); }
    int getScaleSize() const;
    bool isTonicRow (int row) const { return (row % getScaleSize()) == 0; }

    /** Writes the pattern to a temporary .mid file. Returns an invalid File if the pattern is empty. */
    juce::File exportMidi();

    int getCurrentStep() const { return currentStep.load(); }
    int getTriggerCount() const { return triggerCount.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void runPattern (juce::MidiBuffer& events, int numSamples);
    void emitStep (juce::MidiBuffer& events, long long stepIndex, int position);
    void releaseHeld (juce::MidiBuffer& events, int position);
    int rootIndex() const   { return (int) rootP->load(); }
    int scaleIndex() const  { return juce::jlimit (0, pg::numScales - 1, (int) scaleP->load()); }
    int octaveShift() const { return (int) octaveP->load() - 2; }

    pg::SoundParams soundParams;
    juce::Synthesiser synth;
    juce::Reverb reverb;
    juce::SmoothedValue<float> masterGain;

    std::atomic<float>* morphP = nullptr;
    std::atomic<float>* brightP = nullptr;
    std::atomic<float>* timeP = nullptr;
    std::atomic<float>* motionP = nullptr;
    std::atomic<float>* spaceP = nullptr;
    std::atomic<float>* volumeP = nullptr;
    std::atomic<float>* rootP = nullptr;
    std::atomic<float>* scaleP = nullptr;
    std::atomic<float>* octaveP = nullptr;
    std::atomic<float>* chordP = nullptr;
    std::atomic<float>* snapP = nullptr;
    std::atomic<float>* loopP = nullptr;

    std::array<std::atomic<uint8_t>, numSteps> pattern;

    // pattern player state (audio thread only, except the atomics)
    int patternMode = 0;               // 0 = off, 1 = following host, 2 = free running
    double freeStep = 0.0;
    long long lastTriggered = -1;
    std::array<bool, 128> held {};
    std::array<int, 128> snappedFor {};
    std::atomic<int> currentStep { -1 };
    std::atomic<int> triggerCount { 0 };
    std::atomic<double> lastBpm { 120.0 };
    double currentSampleRate = 44100.0;

    // preview notes
    std::atomic<int> previewRequest { -1 };
    int previewNote = -1;
    int previewSamplesLeft = 0;

    int exportCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaygroundProcessor)
};
