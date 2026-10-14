#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DelayTimelineComponent;
class TimeModeToggle;
class TopControlsLookAndFeel;

class IXMultitapDelayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit IXMultitapDelayAudioProcessorEditor (IXMultitapDelayAudioProcessor&);
    ~IXMultitapDelayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    void configureLabel (juce::Label&, const juce::String&);
    void savePreset();
    void loadPreset();
    juce::File getPresetDirectory() const;

    IXMultitapDelayAudioProcessor& audioProcessor;
    std::unique_ptr<TopControlsLookAndFeel> topControlsLookAndFeel;
    juce::Typeface::Ptr titleTypeface;

    juce::Label titleLabel;
    juce::Label presetCaptionLabel;
    juce::Label presetNameLabel;
    juce::TextButton loadPresetButton;
    juce::TextButton savePresetButton;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    juce::Label tapCountLabel;
    juce::Label feedbackLabel;
    juce::Label feedbackHighCutLabel;
    juce::Label dryLabel;
    juce::Label wetLabel;
    juce::Label timeModeLabel;

    juce::Slider tapCountSlider;
    juce::Slider feedbackSlider;
    juce::Slider feedbackHighCutSlider;
    juce::Slider drySlider;
    juce::Slider wetSlider;
    std::unique_ptr<TimeModeToggle> timeModeToggle;

    std::unique_ptr<DelayTimelineComponent> timeline;

    std::unique_ptr<SliderAttachment> tapCountAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<SliderAttachment> feedbackHighCutAttachment;
    std::unique_ptr<SliderAttachment> dryAttachment;
    std::unique_ptr<SliderAttachment> wetAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IXMultitapDelayAudioProcessorEditor)
};
