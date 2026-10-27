#include "PluginEditor.h"

using namespace pgc;

//==============================================================================
// Look and feel
//==============================================================================
PGLookAndFeel::PGLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg0);

    setColour (juce::TextButton::buttonColourId, btn);
    setColour (juce::TextButton::buttonOnColourId, mint);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07130e));

    setColour (juce::ComboBox::backgroundColourId, btn);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, mint);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1f30));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2d3550));
    setColour (juce::PopupMenu::highlightedTextColourId, mint);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1a1f30));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, btn);
}

juce::Font PGLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.42f), juce::Font::bold));
}

void PGLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float trackW = 4.5f;
    const float arcR = r - trackW;
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (btn);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
        g.setGradientFill (juce::ColourGradient (violet, c.x - r, c.y + r, mint, c.x + r, c.y - r, false));
        g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = arcR - 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343c5a), c.x, c.y - bodyR,
                                             juce::Colour (0xff171b29), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    juce::Path p;
    p.addRoundedRectangle (-1.7f, -bodyR + 3.0f, 3.4f, bodyR * 0.48f, 1.7f);
    g.setColour (text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
// XY pad
//==============================================================================
XYPad::XYPad (juce::AudioProcessorValueTreeState& state, const juce::String& xId, const juce::String& yId)
    : xAttach (*state.getParameter (xId), [this] (float v) { xv = v; repaint(); }),
      yAttach (*state.getParameter (yId), [this] (float v) { yv = v; repaint(); })
{
    setTooltip ("Drag!  Left-right = waveform (sine > triangle > saw > square).  Up-down = brightness.");
    xAttach.sendInitialUpdate();
    yAttach.sendInitialUpdate();
}

juce::Rectangle<float> XYPad::inner() const
{
    return getLocalBounds().toFloat().reduced (22.0f);
}

void XYPad::moveTo (juce::Point<float> p)
{
    const auto r = inner();
    const float nx = juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - r.getY()) / r.getHeight());
    xAttach.setValueAsPartOfGesture (nx);
    yAttach.setValueAsPartOfGesture (ny);
}

void XYPad::mouseDown (const juce::MouseEvent& e)
{
    xAttach.beginGesture();
    yAttach.beginGesture();
    moveTo (e.position);
}

void XYPad::mouseDrag (const juce::MouseEvent& e) { moveTo (e.position); }

void XYPad::mouseUp (const juce::MouseEvent&)
{
    xAttach.endGesture();
    yAttach.endGesture();
}

void XYPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff20263b), b.getX(), b.getBottom(),
                                             juce::Colour (0xff10131d), b.getRight(), b.getY(), false));
    g.fillRoundedRectangle (b, 16.0f);

    const auto r = inner();

    // grid
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i <= 4; ++i)
    {
        const float fx = r.getX() + r.getWidth() * (float) i / 4.0f;
        const float fy = r.getY() + r.getHeight() * (float) i / 4.0f;
        g.drawLine (fx, r.getY(), fx, r.getBottom(), 1.0f);
        g.drawLine (r.getX(), fy, r.getRight(), fy, 1.0f);
    }

    const float px = r.getX() + xv * r.getWidth();
    const float py = r.getBottom() - yv * r.getHeight();

    // live waveform preview along the middle
    {
        juce::Path wave;
        const int n = 160;
        const float cy = r.getCentreY();
        const float amp = r.getHeight() * 0.16f;

        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n * 2.0f;
            const float v = pg::dsp::morphWave (xv, pg::dsp::fract (ph), 0.01f);
            const float x = r.getX() + r.getWidth() * (float) i / (float) n;
            if (i == 0) wave.startNewSubPath (x, cy - v * amp);
            else        wave.lineTo (x, cy - v * amp);
        }

        g.setColour (violet.withAlpha (0.20f + 0.35f * yv));
        g.strokePath (wave, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // crosshair
    g.setColour (pink.withAlpha (0.22f));
    g.drawLine (px, r.getY(), px, r.getBottom(), 1.0f);
    g.drawLine (r.getX(), py, r.getRight(), py, 1.0f);

    // glow + puck
    g.setGradientFill (juce::ColourGradient (pink.withAlpha (0.45f), px, py, pink.withAlpha (0.0f), px + 80.0f, py, true));
    g.fillEllipse (px - 80.0f, py - 80.0f, 160.0f, 160.0f);

    g.setColour (pink);
    g.fillEllipse (px - 13.0f, py - 13.0f, 26.0f, 26.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (px - 7.0f, py - 7.0f, 14.0f, 14.0f);

    g.setColour (dim);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("BRIGHT", (int) b.getX() + 14, (int) b.getY() + 6, 100, 14, juce::Justification::centredLeft);
    g.drawText ("MORPH  sine > tri > saw > square", (int) b.getX() + 14, (int) b.getBottom() - 20,
                (int) b.getWidth() - 28, 14, juce::Justification::centredRight);
}

//==============================================================================
// Step grid
//==============================================================================
juce::Rectangle<float> StepGrid::cellRect (int step, int row) const
{
    const float cw = (float) getWidth() / (float) PlaygroundProcessor::numSteps;
    const float ch = (float) getHeight() / (float) PlaygroundProcessor::numRows;
    const int visualRow = PlaygroundProcessor::numRows - 1 - row;
    return juce::Rectangle<float> (cw * (float) step, ch * (float) visualRow, cw, ch).reduced (1.5f);
}

bool StepGrid::cellAt (juce::Point<int> pos, int& step, int& row) const
{
    if (! getLocalBounds().contains (pos)) return false;

    const float cw = (float) getWidth() / (float) PlaygroundProcessor::numSteps;
    const float ch = (float) getHeight() / (float) PlaygroundProcessor::numRows;
    step = juce::jlimit (0, PlaygroundProcessor::numSteps - 1, (int) ((float) pos.x / cw));
    row = PlaygroundProcessor::numRows - 1 - juce::jlimit (0, PlaygroundProcessor::numRows - 1, (int) ((float) pos.y / ch));
    return true;
}

void StepGrid::setPlayhead (int step)
{
    if (step != playhead)
    {
        playhead = step;
        repaint();
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    int s, r;
    if (! cellAt (e.getPosition(), s, r)) return;

    paintValue = ! proc.getCell (s, r);
    proc.setCell (s, r, paintValue);
    if (paintValue) proc.previewRow (r);
    lastStep = s;
    lastRow = r;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    int s, r;
    if (! cellAt (e.getPosition(), s, r)) return;
    if (s == lastStep && r == lastRow) return;

    proc.setCell (s, r, paintValue);
    if (paintValue && r != lastRow) proc.previewRow (r);
    lastStep = s;
    lastRow = r;
    repaint();
}

void StepGrid::paint (juce::Graphics& g)
{
    for (int step = 0; step < PlaygroundProcessor::numSteps; ++step)
    {
        const bool isPlayhead = (step == playhead);

        for (int row = 0; row < PlaygroundProcessor::numRows; ++row)
        {
            auto rc = cellRect (step, row);
            const bool active = proc.getCell (step, row);
            const bool beatGroup = ((step / 4) % 2) == 0;

            if (active)
            {
                g.setColour (mint.withAlpha (isPlayhead ? 0.35f : 0.14f));
                g.fillRoundedRectangle (rc.expanded (2.0f), 6.0f);

                g.setGradientFill (juce::ColourGradient (mint.brighter (isPlayhead ? 0.4f : 0.0f), rc.getX(), rc.getY(),
                                                         mint.darker (0.35f), rc.getX(), rc.getBottom(), false));
                g.fillRoundedRectangle (rc, 4.0f);
            }
            else
            {
                juce::Colour c = proc.isTonicRow (row) ? cellT : cell;
                if (! beatGroup) c = c.brighter (0.08f);
                if (isPlayhead) c = c.brighter (0.35f);
                g.setColour (c);
                g.fillRoundedRectangle (rc, 4.0f);
            }
        }
    }
}

//==============================================================================
// Drag pad
//==============================================================================
void DragPad::mouseDrag (const juce::MouseEvent& e)
{
    if (! started && e.getDistanceFromDragStart() > 5)
    {
        started = true;
        if (onDragStart) onDragStart();
    }
}

void DragPad::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setGradientFill (juce::ColourGradient (violet.withAlpha (hover ? 0.45f : 0.30f), b.getX(), b.getY(),
                                             pink.withAlpha (hover ? 0.45f : 0.30f), b.getRight(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 14.0f);

    juce::Path outline;
    outline.addRoundedRectangle (b.reduced (3.0f), 11.0f);
    juce::Path dashed;
    const float dashes[] = { 7.0f, 5.0f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (text.withAlpha (hover ? 0.9f : 0.55f));
    g.fillPath (dashed);

    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    g.drawText ("DRAG MIDI  >  FL STUDIO", getLocalBounds().withTrimmedBottom (20), juce::Justification::centred);

    g.setColour (dim.brighter (0.3f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("grab me and drop onto the Piano roll or Playlist", getLocalBounds().withTrimmedTop (36),
                juce::Justification::centred);
}

//==============================================================================
// Editor
//==============================================================================
PlaygroundEditor::PlaygroundEditor (PlaygroundProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      pad (p.apvts, "morph", "bright"), grid (p)
{
    setLookAndFeel (&laf);
    setSize (920, 556);

    addAndMakeVisible (pad);

    addKnob (timeKnob,   "TIME",   "time",   "Short plucks  <->  long, slow pads");
    addKnob (motionKnob, "MOTION", "motion", "Movement: a wobble on the brightness");
    addKnob (spaceKnob,  "SPACE",  "space",  "Reverb");
    addKnob (volumeKnob, "VOLUME", "volume", "Output level");

    soundDice.setTooltip ("Surprise me: random sound");
    soundDice.onClick = [this] { proc.randomiseSound(); };
    addAndMakeVisible (soundDice);

    // key / scale / octave
    rootBox.addItemList (juce::StringArray (pg::noteNames(), 12), 1);
    for (int i = 0; i < pg::numScales; ++i) scaleBox.addItem (pg::scaleTable()[i].name, i + 1);
    octBox.addItemList (juce::StringArray { "Oct -2", "Oct -1", "Oct 0", "Oct +1", "Oct +2" }, 1);

    rootBox.setTooltip ("Key");
    scaleBox.setTooltip ("Scale: every note you draw (and the exported MIDI) fits it");
    octBox.setTooltip ("Pattern octave");

    addAndMakeVisible (rootBox);
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (octBox);

    rootAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "root", rootBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    octAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "octave", octBox);

    setupToggle (chordBtn, "CHORD", "chord", "Every note becomes a chord (triad in the scale)");
    setupToggle (snapBtn,  "SNAP",  "snap",  "Keys you play are snapped to the scale - no wrong notes");
    setupToggle (loopBtn,  "LOOP",  "loop",  "Play the pattern. Follows FL's tempo; when FL is stopped it just loops");

    addAndMakeVisible (grid);

    melodyDice.setTooltip ("Random melody in the chosen key and scale");
    melodyDice.onClick = [this]
    {
        proc.randomiseMelody();
        grid.repaint();
    };
    addAndMakeVisible (melodyDice);

    clearBtn.onClick = [this]
    {
        proc.clearPattern();
        grid.repaint();
    };
    addAndMakeVisible (clearBtn);

    saveBtn.setTooltip ("Save the pattern as a .mid file");
    saveBtn.onClick = [this] { saveMidiAs(); };
    addAndMakeVisible (saveBtn);

    dragPad.setTooltip ("Drag the MIDI file out of the plugin");
    dragPad.onDragStart = [this]
    {
        auto f = proc.exportMidi();

        if (! f.existsAsFile())
        {
            showInfo ("The pattern is empty - click some cells first.");
            return;
        }

        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, &dragPad);
    };
    addAndMakeVisible (dragPad);

    info.setFont (juce::Font (juce::FontOptions (12.5f)));
    info.setColour (juce::Label::textColourId, dim);
    info.setJustificationType (juce::Justification::centred);
    info.setText ("Click cells to draw notes  -  LOOP to hear them  -  drag the purple pad into FL Studio",
                  juce::dontSendNotification);
    addAndMakeVisible (info);

    startTimerHz (30);
}

PlaygroundEditor::~PlaygroundEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PlaygroundEditor::addKnob (Knob& k, const juce::String& name, const juce::String& id, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setMouseDragSensitivity (160);
    k.slider.setPopupDisplayEnabled (true, true, this);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);

    if (auto* param = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    k.label.setColour (juce::Label::textColourId, dim);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);
}

void PlaygroundEditor::setupToggle (juce::TextButton& b, const juce::String& text, const juce::String& id, const juce::String& tip)
{
    b.setButtonText (text);
    b.setTooltip (tip);
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);

    auto att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);

    if (id == "chord")     chordAtt = std::move (att);
    else if (id == "snap") snapAtt = std::move (att);
    else                   loopAtt = std::move (att);
}

void PlaygroundEditor::showInfo (const juce::String& msg)
{
    info.setText (msg, juce::dontSendNotification);
    info.setColour (juce::Label::textColourId, pink);
    infoTicks = 90;
}

void PlaygroundEditor::saveMidiAs()
{
    auto f = proc.exportMidi();

    if (! f.existsAsFile())
    {
        showInfo ("The pattern is empty - click some cells first.");
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Save MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Playground pattern.mid"),
        "*.mid");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [f] (const juce::FileChooser& fc)
                          {
                              auto dest = fc.getResult();
                              if (dest != juce::File())
                                  f.copyFileTo (dest.withFileExtension ("mid"));
                          });
}

void PlaygroundEditor::timerCallback()
{
    const int s = proc.getCurrentStep();

    if (s != shownStep)
    {
        shownStep = s;
        grid.setPlayhead (s);
    }

    if (infoTicks > 0 && --infoTicks == 0)
    {
        info.setText ("Click cells to draw notes  -  LOOP to hear them  -  drag the purple pad into FL Studio",
                      juce::dontSendNotification);
        info.setColour (juce::Label::textColourId, dim);
    }
}

void PlaygroundEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bg1, 0.0f, 0.0f, bg0, (float) getWidth(), (float) getHeight(), false));
    g.fillAll();

    // right-hand panel
    g.setColour (panel.withAlpha (0.85f));
    g.fillRoundedRectangle (juce::Rectangle<float> (376.0f, 62.0f, 532.0f, 468.0f), 18.0f);

    // title
    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("PLAYGROUND", 24, 14, 260, 36, juce::Justification::centredLeft);

    g.setColour (mint);
    g.fillEllipse (212.0f, 28.0f, 8.0f, 8.0f);

    g.setColour (dim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("play with sound, sketch some MIDI", 520, 20, 376, 24, juce::Justification::centredRight);

    // captions above the selectors
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("KEY",   388, 72, 64, 12, juce::Justification::centredLeft);
    g.drawText ("SCALE", 460, 72, 160, 12, juce::Justification::centredLeft);
    g.drawText ("OCTAVE", 628, 72, 80, 12, juce::Justification::centredLeft);
}

void PlaygroundEditor::resized()
{
    // left column
    pad.setBounds (24, 72, 340, 296);

    const int kw = 85;
    Knob* knobs[] = { &timeKnob, &motionKnob, &spaceKnob, &volumeKnob };
    for (int i = 0; i < 4; ++i)
    {
        knobs[i]->slider.setBounds (24 + i * kw + 5, 380, kw - 10, 70);
        knobs[i]->label.setBounds (24 + i * kw, 450, kw, 16);
    }

    soundDice.setBounds (24, 482, 340, 38);

    // right column
    const int rx = 388;
    rootBox.setBounds (rx, 86, 64, 28);
    scaleBox.setBounds (rx + 72, 86, 160, 28);
    octBox.setBounds (rx + 240, 86, 80, 28);

    chordBtn.setBounds (rx + 336, 86, 54, 28);
    snapBtn.setBounds  (rx + 395, 86, 54, 28);
    loopBtn.setBounds  (rx + 454, 86, 54, 28);

    grid.setBounds (rx - 2, 126, 512, 256);

    melodyDice.setBounds (rx, 396, 200, 34);
    clearBtn.setBounds   (rx + 208, 396, 90, 34);
    saveBtn.setBounds    (rx + 398, 396, 110, 34);

    dragPad.setBounds (rx, 442, 508, 78);

    info.setBounds (24, 530, 872, 20);
}
