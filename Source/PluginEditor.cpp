#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int numberOfTaps = 8;

juce::String tapParameterId (int tapIndex, const char* suffix)
{
    return "tap" + juce::String (tapIndex + 1) + suffix;
}

const juce::StringArray& divisionNames()
{
    static const juce::StringArray names {
        "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/2.", "1/4.", "1/8.", "1/16.", "1/32.",
        "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };
    return names;
}

float divisionIndexToQuarterNotes (int divisionIndex)
{
    static constexpr std::array<float, 17> quarterNotes {
        4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f,
        3.0f, 1.5f, 0.75f, 0.375f, 0.1875f,
        4.0f / 3.0f, 2.0f / 3.0f, 1.0f / 3.0f,
        1.0f / 6.0f, 1.0f / 12.0f
    };

    return quarterNotes[static_cast<size_t> (
        juce::jlimit (0, static_cast<int> (quarterNotes.size()) - 1,
                      divisionIndex))];
}

juce::Colour tapColour (int tapIndex)
{
    static const std::array<juce::Colour, numberOfTaps> colours {
        juce::Colour (0xff65aeb8),
        juce::Colour (0xffc08b5c),
        juce::Colour (0xff8d78b5),
        juce::Colour (0xff86a866),
        juce::Colour (0xffb96f88),
        juce::Colour (0xff668fbd),
        juce::Colour (0xffb5a05d),
        juce::Colour (0xffa66fa8)
    };

    return colours[static_cast<size_t> (juce::jlimit (0, numberOfTaps - 1,
                                                       tapIndex))];
}

void configureInspectorSlider (juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 22);
    slider.setTextValueSuffix (suffix);
    slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff8f98a8));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd7dbe2));
}

class TapInspectorComponent : public juce::Component
{
public:
    TapInspectorComponent (juce::AudioProcessorValueTreeState& stateToUse,
                           int tapIndexToUse,
                           bool beatMode)
        : state (stateToUse), tapIndex (tapIndexToUse)
    {
        title.setText ("TAP " + juce::String (tapIndex + 1),
                       juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, tapColour (tapIndex));
        addAndMakeVisible (title);

        configureInspectorSlider (timeSlider, " ms");
        configureInspectorSlider (gainSlider, " dB");
        configureInspectorSlider (humanizeSlider, " ms");
        configureInspectorSlider (panSlider, " %");

        for (auto* slider : { &timeSlider, &gainSlider,
                              &humanizeSlider, &panSlider })
            addAndMakeVisible (*slider);

        for (int index = 0; index < divisionNames().size(); ++index)
            divisionBox.addItem (divisionNames()[index], index + 1);
        addAndMakeVisible (divisionBox);

        timeSlider.setVisible (! beatMode);
        divisionBox.setVisible (beatMode);

        timeAttachment = std::make_unique<SliderAttachment> (
            state, tapParameterId (tapIndex, "Time"), timeSlider);
        divisionAttachment = std::make_unique<ComboBoxAttachment> (
            state, tapParameterId (tapIndex, "Division"), divisionBox);
        gainAttachment = std::make_unique<SliderAttachment> (
            state, tapParameterId (tapIndex, "Gain"), gainSlider);
        humanizeAttachment = std::make_unique<SliderAttachment> (
            state, tapParameterId (tapIndex, "Humanize"), humanizeSlider);
        panAttachment = std::make_unique<SliderAttachment> (
            state, tapParameterId (tapIndex, "Pan"), panSlider);

        setSize (340, 250);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1a1e25));
        g.setColour (juce::Colour (0xffaab0ba));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        auto area = getLocalBounds().reduced (14);
        area.removeFromTop (38);
        const juce::String timeText = timeSlider.isVisible() ? "TIME" : "DIVISION";
        g.drawText (timeText, area.removeFromTop (42).removeFromLeft (78),
                    juce::Justification::centredLeft);
        g.drawText ("GAIN", area.removeFromTop (42).removeFromLeft (78),
                    juce::Justification::centredLeft);
        g.drawText ("HUMANIZE", area.removeFromTop (42).removeFromLeft (78),
                    juce::Justification::centredLeft);
        g.drawText ("PAN", area.removeFromTop (42).removeFromLeft (78),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        title.setBounds (area.removeFromTop (38));

        auto placeControl = [&area] (juce::Component& control)
        {
            auto row = area.removeFromTop (42);
            row.removeFromLeft (78);
            control.setBounds (row.reduced (2, 7));
        };

        auto timeRow = area.removeFromTop (42);
        timeRow.removeFromLeft (78);
        timeSlider.setBounds (timeRow.reduced (2, 7));
        divisionBox.setBounds (timeRow.reduced (2, 7));
        placeControl (gainSlider);
        placeControl (humanizeSlider);
        placeControl (panSlider);
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::AudioProcessorValueTreeState& state;
    int tapIndex = 0;
    juce::Label title;
    juce::Slider timeSlider;
    juce::ComboBox divisionBox;
    juce::Slider gainSlider;
    juce::Slider humanizeSlider;
    juce::Slider panSlider;
    std::unique_ptr<SliderAttachment> timeAttachment;
    std::unique_ptr<ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
};
}

class DelayTimelineComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit DelayTimelineComponent (IXMultitapDelayAudioProcessor& processorToUse)
        : processor (processorToUse), state (processorToUse.getAPVTS())
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff151920));
        g.fillRoundedRectangle (bounds, 10.0f);

        auto titleArea = getLocalBounds().reduced (18).removeFromTop (30);
        g.setColour (juce::Colour (0xffd7dbe2));
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (isBeatMode() ? "BEAT GRID" : "TIME (MS)", titleArea,
                    juce::Justification::centredLeft);

        auto track = getTrackBounds();
        g.setColour (juce::Colour (0xff101319));
        g.fillRoundedRectangle (track, 7.0f);

        drawGrid (g, track);

        const float centreY = track.getCentreY();
        g.setColour (juce::Colour (0xff606874).withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (centreY), track.getX(), track.getRight());

        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.setColour (juce::Colour (0xff7f8793));
        g.drawText ("L", juce::Rectangle<float> (track.getX() + 8.0f,
                                                  track.getY() + 6.0f,
                                                  18.0f, 18.0f),
                    juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (track.getX() + 8.0f,
                                                  track.getBottom() - 24.0f,
                                                  18.0f, 18.0f),
                    juce::Justification::centred);

        const int activeTaps = getActiveTapCount();
        for (int tapIndex = 0; tapIndex < activeTaps; ++tapIndex)
            drawTap (g, track, tapIndex);

        drawTimelineLabels (g, track);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragTap = hitTestTap (event.position);
        selectedTap = dragTap;
        hasDragged = false;

        if (dragTap >= 0)
        {
            if (auto* parameter = getDraggedParameter (dragTap))
                parameter->beginChangeGesture();
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragTap < 0)
            return;

        if (event.getDistanceFromDragStart() > 3)
            hasDragged = true;

        setTapFromMouseX (dragTap, event.position.x);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragTap < 0)
            return;

        if (auto* parameter = getDraggedParameter (dragTap))
            parameter->endChangeGesture();

        const int releasedTap = dragTap;
        dragTap = -1;

        if (! hasDragged)
            openInspector (releasedTap);
    }

private:
    juce::Rectangle<float> getTrackBounds() const
    {
        auto bounds = getLocalBounds().reduced (18);
        bounds.removeFromTop (38);
        bounds.removeFromBottom (28);
        return bounds.toFloat();
    }

    bool isBeatMode() const
    {
        return juce::roundToInt (state.getRawParameterValue ("timeMode")->load()) == 1;
    }

    int getActiveTapCount() const
    {
        return juce::jlimit (1, numberOfTaps,
            juce::roundToInt (state.getRawParameterValue ("tapCount")->load()));
    }

    float getTapDelayMs (int tapIndex) const
    {
        if (! isBeatMode())
            return state.getRawParameterValue (
                tapParameterId (tapIndex, "Time"))->load();

        const int divisionIndex = juce::roundToInt (
            state.getRawParameterValue (
                tapParameterId (tapIndex, "Division"))->load());
        const double bpm = juce::jmax (1.0, processor.getCurrentBpmForUI());
        return divisionIndexToQuarterNotes (divisionIndex)
            * 60000.0f / static_cast<float> (bpm);
    }

    float xForMilliseconds (float milliseconds,
                            juce::Rectangle<float> track) const
    {
        return juce::jmap (
            juce::jlimit (0.0f, processor.getMaximumDelayTimeMsForUI(), milliseconds),
            0.0f, processor.getMaximumDelayTimeMsForUI(),
            track.getX(), track.getRight());
    }

    float millisecondsForX (float x, juce::Rectangle<float> track) const
    {
        return juce::jmap (juce::jlimit (track.getX(), track.getRight(), x),
                           track.getX(), track.getRight(),
                           1.0f, processor.getMaximumDelayTimeMsForUI());
    }

    juce::Rectangle<float> nodeBounds (int tapIndex) const
    {
        const auto track = getTrackBounds();
        const float pan = state.getRawParameterValue (
            tapParameterId (tapIndex, "Pan"))->load() / 100.0f;
        const float gainDb = state.getRawParameterValue (
            tapParameterId (tapIndex, "Gain"))->load();
        const float gainAmount = juce::jlimit (0.0f, 1.0f,
                                               (gainDb + 60.0f) / 66.0f);
        const float diameter = 11.0f + 7.0f * gainAmount;
        const float y = track.getCentreY()
            + pan * track.getHeight() * 0.30f;
        const float x = xForMilliseconds (getTapDelayMs (tapIndex), track);
        return { x - diameter * 0.5f, y - diameter * 0.5f,
                 diameter, diameter };
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> track)
    {
        if (! isBeatMode())
        {
            for (int division = 1; division < 8; ++division)
            {
                const float x = track.getX()
                    + track.getWidth() * static_cast<float> (division) / 8.0f;
                g.setColour (juce::Colour (0xff6c7480).withAlpha (0.12f));
                g.drawVerticalLine (juce::roundToInt (x), track.getY(), track.getBottom());
            }
            return;
        }

        const float msPerQuarter = 60000.0f
            / static_cast<float> (juce::jmax (1.0, processor.getCurrentBpmForUI()));

        for (int divisionIndex = 0; divisionIndex < divisionNames().size(); ++divisionIndex)
        {
            const float milliseconds = divisionIndexToQuarterNotes (divisionIndex)
                * msPerQuarter;
            if (milliseconds > processor.getMaximumDelayTimeMsForUI())
                continue;

            const bool isStraight = divisionIndex <= 6;
            const float x = xForMilliseconds (milliseconds, track);
            g.setColour (tapColour (divisionIndex % numberOfTaps)
                             .withAlpha (isStraight ? 0.22f : 0.11f));
            g.drawVerticalLine (juce::roundToInt (x), track.getY(), track.getBottom());
        }
    }

    void drawTap (juce::Graphics& g,
                  juce::Rectangle<float> track,
                  int tapIndex)
    {
        const auto colour = tapColour (tapIndex);
        const auto node = nodeBounds (tapIndex);
        const float delayMs = getTapDelayMs (tapIndex);
        const float humanizeMs = state.getRawParameterValue (
            tapParameterId (tapIndex, "Humanize"))->load();

        if (humanizeMs > 0.0f)
        {
            const float left = xForMilliseconds (delayMs - humanizeMs, track);
            const float right = xForMilliseconds (delayMs + humanizeMs, track);
            auto range = juce::Rectangle<float> (
                left, node.getCentreY() - 12.0f,
                juce::jmax (2.0f, right - left), 24.0f);
            g.setColour (colour.withAlpha (0.14f));
            g.fillRoundedRectangle (range, 12.0f);
            g.setColour (colour.withAlpha (0.30f));
            g.drawRoundedRectangle (range, 12.0f, 1.0f);
        }

        g.setColour (colour.withAlpha (0.05f));
        g.fillEllipse (node.expanded (16.0f));
        g.setColour (colour.withAlpha (0.11f));
        g.fillEllipse (node.expanded (9.0f));
        g.setColour (colour.withAlpha (0.32f));
        g.fillEllipse (node.expanded (4.0f));
        g.setColour (colour);
        g.fillEllipse (node);

        if (tapIndex == selectedTap)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawEllipse (node.expanded (3.0f), 1.5f);
        }

        g.setColour (juce::Colour (0xff101319).withAlpha (0.9f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String (tapIndex + 1), node,
                    juce::Justification::centred);

        g.setColour (colour.withAlpha (0.88f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        const juce::String value = isBeatMode()
            ? divisionNames()[juce::roundToInt (state.getRawParameterValue (
                tapParameterId (tapIndex, "Division"))->load())]
            : juce::String (juce::roundToInt (delayMs)) + " ms";
        g.drawText (value,
                    juce::Rectangle<float> (node.getCentreX() - 35.0f,
                                            node.getBottom() + 7.0f,
                                            70.0f, 16.0f),
                    juce::Justification::centred);
    }

    void drawTimelineLabels (juce::Graphics& g,
                             juce::Rectangle<float> track)
    {
        g.setColour (juce::Colour (0xff747c88));
        g.setFont (juce::FontOptions (11.0f));
        const float labelY = track.getBottom() + 7.0f;
        g.drawText ("0", juce::Rectangle<float> (track.getX(), labelY, 50.0f, 16.0f),
                    juce::Justification::centredLeft);
        g.drawText (juce::String (juce::roundToInt (
                        processor.getMaximumDelayTimeMsForUI())) + " ms",
                    juce::Rectangle<float> (track.getRight() - 70.0f,
                                            labelY, 70.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    int hitTestTap (juce::Point<float> point) const
    {
        for (int tapIndex = getActiveTapCount() - 1; tapIndex >= 0; --tapIndex)
            if (nodeBounds (tapIndex).expanded (9.0f).contains (point))
                return tapIndex;
        return -1;
    }

    juce::RangedAudioParameter* getDraggedParameter (int tapIndex) const
    {
        return state.getParameter (tapParameterId (
            tapIndex, isBeatMode() ? "Division" : "Time"));
    }

    void setTapFromMouseX (int tapIndex, float mouseX)
    {
        auto track = getTrackBounds();

        if (! isBeatMode())
        {
            if (auto* parameter = state.getParameter (
                    tapParameterId (tapIndex, "Time")))
            {
                const float milliseconds = std::round (
                    millisecondsForX (mouseX, track));
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (milliseconds));
            }
            return;
        }

        const float targetMs = millisecondsForX (mouseX, track);
        const float msPerQuarter = 60000.0f
            / static_cast<float> (juce::jmax (1.0, processor.getCurrentBpmForUI()));
        int closestDivision = 0;
        float closestDistance = std::numeric_limits<float>::max();

        for (int divisionIndex = 0; divisionIndex < divisionNames().size(); ++divisionIndex)
        {
            const float divisionMs = divisionIndexToQuarterNotes (divisionIndex)
                * msPerQuarter;
            if (divisionMs > processor.getMaximumDelayTimeMsForUI())
                continue;

            const float distance = std::abs (divisionMs - targetMs);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestDivision = divisionIndex;
            }
        }

        if (auto* parameter = state.getParameter (
                tapParameterId (tapIndex, "Division")))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (closestDivision)));
    }

    void openInspector (int tapIndex)
    {
        auto inspector = std::make_unique<TapInspectorComponent> (
            state, tapIndex, isBeatMode());
        auto* parent = getParentComponent();
        if (parent == nullptr)
            return;

        const auto anchor = parent->getLocalArea (
            this, nodeBounds (tapIndex).expanded (5.0f).toNearestInt());
        juce::CallOutBox::launchAsynchronously (
            std::move (inspector), anchor, parent);
    }

    void timerCallback() override
    {
        repaint();
    }

    IXMultitapDelayAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    int selectedTap = -1;
    int dragTap = -1;
    bool hasDragged = false;
};

IXMultitapDelayAudioProcessorEditor::IXMultitapDelayAudioProcessorEditor (
    IXMultitapDelayAudioProcessor& processor)
    : AudioProcessorEditor (&processor), audioProcessor (processor)
{
    setOpaque (true);

    titleLabel.setText ("IX MULTITAP DELAY", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (23.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd2b66f));
    addAndMakeVisible (titleLabel);

    configureLabel (tapCountLabel, "TAPS");
    configureLabel (feedbackLabel, "FEEDBACK");
    configureLabel (feedbackHighCutLabel, "FB HIGH CUT");
    configureLabel (dryLabel, "DRY");
    configureLabel (wetLabel, "WET");
    configureLabel (timeModeLabel, "TIME MODE");

    tapCountSlider.setSliderStyle (juce::Slider::IncDecButtons);
    tapCountSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 22);
    addAndMakeVisible (tapCountSlider);

    for (auto* slider : { &feedbackSlider, &feedbackHighCutSlider,
                          &drySlider, &wetSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (0xffa7966e));
        slider->setColour (juce::Slider::thumbColourId,
                           juce::Colour (0xffd4c18d));
        addAndMakeVisible (*slider);
    }

    feedbackSlider.setTextValueSuffix (" %");
    feedbackHighCutSlider.setTextValueSuffix (" Hz");
    drySlider.setTextValueSuffix (" dB");
    wetSlider.setTextValueSuffix (" dB");

    timeModeBox.addItem ("Milliseconds", 1);
    timeModeBox.addItem ("Beats", 2);
    addAndMakeVisible (timeModeBox);

    timeline = std::make_unique<DelayTimelineComponent> (audioProcessor);
    addAndMakeVisible (*timeline);

    auto& state = audioProcessor.getAPVTS();
    tapCountAttachment = std::make_unique<SliderAttachment> (
        state, "tapCount", tapCountSlider);
    feedbackAttachment = std::make_unique<SliderAttachment> (
        state, "feedback", feedbackSlider);
    feedbackHighCutAttachment = std::make_unique<SliderAttachment> (
        state, "feedbackHighCut", feedbackHighCutSlider);
    dryAttachment = std::make_unique<SliderAttachment> (
        state, "dryVolDB", drySlider);
    wetAttachment = std::make_unique<SliderAttachment> (
        state, "wetVolDB", wetSlider);
    timeModeAttachment = std::make_unique<ComboBoxAttachment> (
        state, "timeMode", timeModeBox);

    setSize (1100, 680);
    setResizable (true, true);
    setResizeLimits (920, 560, 1500, 950);
}

IXMultitapDelayAudioProcessorEditor::~IXMultitapDelayAudioProcessorEditor() = default;

void IXMultitapDelayAudioProcessorEditor::configureLabel (
    juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffb8bec7));
    label.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
}

void IXMultitapDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e1116));
}

void IXMultitapDelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (20);
    auto top = bounds.removeFromTop (112);
    titleLabel.setBounds (top.removeFromLeft (238).removeFromTop (38));

    auto controls = top;
    const int controlWidth = controls.getWidth() / 6;

    auto placeControl = [] (juce::Rectangle<int> area,
                            juce::Label& label,
                            juce::Component& control)
    {
        label.setBounds (area.removeFromTop (20));
        control.setBounds (area.reduced (4, 0));
    };

    placeControl (controls.removeFromLeft (controlWidth), tapCountLabel, tapCountSlider);
    placeControl (controls.removeFromLeft (controlWidth), feedbackLabel, feedbackSlider);
    placeControl (controls.removeFromLeft (controlWidth), feedbackHighCutLabel,
                  feedbackHighCutSlider);
    placeControl (controls.removeFromLeft (controlWidth), dryLabel, drySlider);
    placeControl (controls.removeFromLeft (controlWidth), wetLabel, wetSlider);
    placeControl (controls, timeModeLabel, timeModeBox);

    bounds.removeFromTop (6);
    timeline->setBounds (bounds);
}
