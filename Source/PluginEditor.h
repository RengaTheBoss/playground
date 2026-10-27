#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include "PluginProcessor.h"

namespace pgc
{
const juce::Colour bg0   { 0xff0b0d13 };
const juce::Colour bg1   { 0xff171b29 };
const juce::Colour panel { 0xff141824 };
const juce::Colour cell  { 0xff1d2232 };
const juce::Colour cellT { 0xff262c42 };
const juce::Colour btn   { 0xff262c40 };
const juce::Colour mint  { 0xff62f5c6 };
const juce::Colour pink  { 0xffff5fb4 };
const juce::Colour violet{ 0xff8b7bff };
const juce::Colour text  { 0xffdfe5f5 };
const juce::Colour dim   { 0xff7d86a3 };
} // namespace pgc

//==============================================================================
class PGLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PGLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

//==============================================================================
/** Two parameters in one: X = morph, Y = bright. */
class XYPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    XYPad (juce::AudioProcessorValueTreeState& state, const juce::String& xId, const juce::String& yId);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> inner() const;
    void moveTo (juce::Point<float> p);

    float xv = 0.5f, yv = 0.5f;
    juce::ParameterAttachment xAttach, yAttach;
};

//==============================================================================
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (PlaygroundProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setPlayhead (int step);

private:
    bool cellAt (juce::Point<int> pos, int& step, int& row) const;
    juce::Rectangle<float> cellRect (int step, int row) const;

    PlaygroundProcessor& proc;
    int playhead = -1;
    bool paintValue = true;
    int lastStep = -1, lastRow = -1;
};

//==============================================================================
class DragPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragPad() { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    std::function<void()> onDragStart;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    bool started = false, hover = false;
};

//==============================================================================
class PlaygroundEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit PlaygroundEditor (PlaygroundProcessor&);
    ~PlaygroundEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    void timerCallback() override;
    void addKnob (Knob& k, const juce::String& name, const juce::String& id, const juce::String& tip);
    void setupToggle (juce::TextButton& b, const juce::String& text, const juce::String& id, const juce::String& tip);
    void showInfo (const juce::String& msg);
    void saveMidiAs();

    PlaygroundProcessor& proc;
    PGLookAndFeel laf;

    XYPad pad;
    Knob timeKnob, motionKnob, spaceKnob, volumeKnob;
    juce::TextButton soundDice { "RANDOM SOUND" };

    juce::ComboBox rootBox, scaleBox, octBox;
    juce::TextButton chordBtn, snapBtn, loopBtn;
    StepGrid grid;
    juce::TextButton melodyDice { "RANDOM MELODY" }, clearBtn { "CLEAR" }, saveBtn { "SAVE .MID" };
    DragPad dragPad;
    juce::Label info;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAtt, scaleAtt, octAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chordAtt, snapAtt, loopAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TooltipWindow tooltip { this, 500 };

    int shownStep = -2;
    int infoTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaygroundEditor)
};
