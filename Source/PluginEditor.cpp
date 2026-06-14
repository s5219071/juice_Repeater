#include "PluginEditor.h"

#include <array>
#include <cmath>

namespace
{
    const auto background = juce::Colour::fromString ("ff121212");
    const auto panel = juce::Colour::fromString ("ff191919");
    const auto panelEdge = juce::Colour::fromString ("ff30363a");
    const auto neon = juce::Colour::fromString ("ff00fbff");
    const auto text = juce::Colour::fromString ("fff2f7f7");
    const auto muted = juce::Colour::fromString ("ff788487");

    constexpr float knobStart = juce::MathConstants<float>::pi * 1.22f;
    constexpr float knobEnd = juce::MathConstants<float>::pi * 2.78f;

    const std::array<juce::String, 5> lengthNames { "1/1", "1/2", "1/4", "1/8", "1/16" };
}

JuiceRepeaterAudioProcessorEditor::RepeaterLookAndFeel::RepeaterLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, neon);
    setColour (juce::Slider::textBoxBackgroundColourId, panel);
    setColour (juce::Slider::textBoxOutlineColourId, panelEdge);
    setColour (juce::Slider::textBoxHighlightColourId, neon.withAlpha (0.25f));
}

void JuiceRepeaterAudioProcessorEditor::RepeaterLookAndFeel::drawRotarySlider (
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosition,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider&)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                          static_cast<float> (y),
                                          static_cast<float> (width),
                                          static_cast<float> (height))
                      .reduced (18.0f);
    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto dial = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());
    const auto centre = dial.getCentre();
    const float radius = diameter * 0.5f;
    const float angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillEllipse (dial.translated (0.0f, 7.0f));

    juce::ColourGradient face (juce::Colour::fromString ("ff303437"),
                               centre.x - radius * 0.45f,
                               dial.getY(),
                               juce::Colour::fromString ("ff090a0a"),
                               centre.x + radius * 0.45f,
                               dial.getBottom(),
                               false);
    g.setGradientFill (face);
    g.fillEllipse (dial);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 7.0f, radius - 7.0f,
                         0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (panelEdge);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius - 7.0f, radius - 7.0f,
                            0.0f, rotaryStartAngle, angle, true);
    g.setColour (neon.withAlpha (0.16f));
    g.strokePath (valueArc, juce::PathStrokeType (13.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    g.setColour (neon);
    g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    for (int index = 0; index < 5; ++index)
    {
        const float proportion = static_cast<float> (index) / 4.0f;
        const float tickAngle = rotaryStartAngle + proportion * (rotaryEndAngle - rotaryStartAngle);
        const float inner = radius - 22.0f;
        const float outer = radius - 12.0f;
        const juce::Point<float> start (centre.x + std::cos (tickAngle) * inner,
                                        centre.y + std::sin (tickAngle) * inner);
        const juce::Point<float> end (centre.x + std::cos (tickAngle) * outer,
                                      centre.y + std::sin (tickAngle) * outer);
        g.setColour (proportion <= sliderPosition ? neon : muted.withAlpha (0.45f));
        g.drawLine ({ start, end }, 2.0f);
    }

    juce::Path pointer;
    pointer.addRoundedRectangle (-2.5f, -radius + 29.0f, 5.0f, radius * 0.42f, 2.5f);
    pointer.applyTransform (
        juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (text);
    g.fillPath (pointer);

    g.setColour (neon.withAlpha (0.35f));
    g.drawEllipse (dial.reduced (1.0f), 1.0f);
}

void JuiceRepeaterAudioProcessorEditor::RepeaterLookAndFeel::drawToggleButton (
    juce::Graphics& g,
    juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    auto area = button.getLocalBounds().toFloat().reduced (5.0f);
    auto switchArea = area.removeFromTop (44.0f).withSizeKeepingCentre (86.0f, 36.0f);
    const bool enabled = button.getToggleState();

    g.setColour (enabled ? neon.withAlpha (0.22f) : panel);
    g.fillRoundedRectangle (switchArea, switchArea.getHeight() * 0.5f);
    g.setColour (enabled ? neon : panelEdge.brighter (0.18f));
    g.drawRoundedRectangle (switchArea.reduced (0.75f), switchArea.getHeight() * 0.5f,
                            shouldDrawButtonAsHighlighted ? 2.0f : 1.2f);

    const float knobSize = switchArea.getHeight() - 10.0f;
    const float knobX = enabled ? switchArea.getRight() - knobSize - 5.0f
                                : switchArea.getX() + 5.0f;
    g.setColour (shouldDrawButtonAsDown ? neon.darker (0.25f)
                                        : (enabled ? neon : muted));
    g.fillEllipse (knobX, switchArea.getY() + 5.0f, knobSize, knobSize);

    g.setColour (enabled ? neon : text);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText (button.getButtonText(), area, juce::Justification::centredTop);

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (enabled ? "5 ms S-CURVE" : "HARD WRAP",
                area.translated (0.0f, 23.0f),
                juce::Justification::centredTop);
}

JuiceRepeaterAudioProcessorEditor::JuiceRepeaterAudioProcessorEditor (
    JuiceRepeaterAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      audioProcessor (processor)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (560, 330, 900, 560);
    setSize (680, 410);

    lengthKnob.setName ("Length");
    lengthKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    lengthKnob.setRotaryParameters (knobStart, knobEnd, true);
    lengthKnob.setRange (0.0, 4.0, 1.0);
    lengthKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 28);
    lengthKnob.setDoubleClickReturnValue (true, 2.0);
    lengthKnob.textFromValueFunction = [] (double value)
    {
        return lengthNames[static_cast<size_t> (
            juce::jlimit (0, 4, static_cast<int> (std::round (value))))];
    };
    lengthKnob.valueFromTextFunction = [] (const juce::String& value)
    {
        for (int index = 0; index < static_cast<int> (lengthNames.size()); ++index)
            if (value.trim() == lengthNames[static_cast<size_t> (index)])
                return static_cast<double> (index);

        return 2.0;
    };
    addAndMakeVisible (lengthKnob);

    softButton.setButtonText ("SOFT");
    softButton.setClickingTogglesState (true);
    addAndMakeVisible (softButton);

    lengthAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts, JuiceRepeaterAudioProcessor::ParameterIDs::length, lengthKnob);
    softAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.apvts, JuiceRepeaterAudioProcessor::ParameterIDs::soft, softButton);
}

JuiceRepeaterAudioProcessorEditor::~JuiceRepeaterAudioProcessorEditor()
{
    softAttachment = nullptr;
    lengthAttachment = nullptr;
    setLookAndFeel (nullptr);
}

void JuiceRepeaterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    juce::ColourGradient glow (neon.withAlpha (0.12f),
                               static_cast<float> (getWidth()) * 0.28f,
                               80.0f,
                               juce::Colours::transparentBlack,
                               static_cast<float> (getWidth()) * 0.75f,
                               static_cast<float> (getHeight()),
                               true);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    auto frame = getLocalBounds().toFloat().reduced (14.0f);
    g.setColour (panel.withAlpha (0.72f));
    g.fillRoundedRectangle (frame, 12.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (frame, 12.0f, 1.0f);

    auto header = getLocalBounds().reduced (30).removeFromTop (62);
    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (25.0f, juce::Font::bold)));
    g.drawText ("JuiceRepeater", header, juce::Justification::centredLeft);

    g.setColour (neon);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText ("PPQ LOCKED  /  SAMPLE ACCURATE",
                header,
                juce::Justification::centredRight);

    const float gridTop = 94.0f;
    const float gridBottom = static_cast<float> (getHeight() - 38);
    g.setColour (neon.withAlpha (0.055f));
    for (float x = 28.0f; x < static_cast<float> (getWidth() - 28); x += 24.0f)
        g.drawVerticalLine (static_cast<int> (x), gridTop, gridBottom);
    for (float y = gridTop; y < gridBottom; y += 24.0f)
        g.drawHorizontalLine (static_cast<int> (y), 28.0f, static_cast<float> (getWidth() - 28));

    auto knobLabel = lengthKnob.getBounds().translated (0, -26);
    g.setColour (text);
    g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    g.drawText ("LENGTH", knobLabel, juce::Justification::centredTop);

    g.setColour (muted);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("HOST-SYNCED LOOP SIZE",
                knobLabel.translated (0, 21),
                juce::Justification::centredTop);

    g.setColour (muted.withAlpha (0.82f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("Made by Kino",
                getLocalBounds().reduced (25).removeFromBottom (18),
                juce::Justification::centredRight);
}

void JuiceRepeaterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28);
    area.removeFromTop (76);
    area.removeFromBottom (22);

    const int gap = juce::jlimit (34, 90, area.getWidth() / 9);
    auto left = area.removeFromLeft ((area.getWidth() - gap) * 2 / 3);
    area.removeFromLeft (gap);

    const int knobSize = juce::jmin (left.getWidth(), left.getHeight());
    lengthKnob.setBounds (left.withSizeKeepingCentre (knobSize, knobSize));
    softButton.setBounds (area.withSizeKeepingCentre (
        juce::jmin (150, area.getWidth()), juce::jmin (104, area.getHeight())));
}
