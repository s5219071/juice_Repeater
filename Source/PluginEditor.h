#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <memory>

class JuiceRepeaterAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit JuiceRepeaterAudioProcessorEditor (JuiceRepeaterAudioProcessor&);
    ~JuiceRepeaterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class RepeaterLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        RepeaterLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                               float, float, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool, bool) override;
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    JuiceRepeaterAudioProcessor& audioProcessor;
    RepeaterLookAndFeel lookAndFeel;

    juce::Slider lengthKnob;
    juce::ToggleButton softButton;

    std::unique_ptr<SliderAttachment> lengthAttachment;
    std::unique_ptr<ButtonAttachment> softAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceRepeaterAudioProcessorEditor)
};
