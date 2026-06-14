#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>

class JuiceRepeaterAudioProcessor final : public juce::AudioProcessor
{
public:
    JuiceRepeaterAudioProcessor();
    ~JuiceRepeaterAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    struct ParameterIDs
    {
        static constexpr auto length = "length";
        static constexpr auto soft = "soft";
    };

    juce::AudioProcessorValueTreeState apvts;

private:
    enum class LoopState
    {
        waitingForGrid,
        capturing,
        repeating
    };

    struct HostPosition
    {
        bool valid = false;
        bool playing = false;
        bool hasTimeInSamples = false;
        double bpm = 120.0;
        double ppq = 0.0;
        std::int64_t timeInSamples = 0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static double getQuarterNotesForChoice (int choice) noexcept;
    static double getBoundaryAtOrAfter (double ppq, double gridLength) noexcept;
    static float smoothstep (float x) noexcept;

    HostPosition getHostPosition() const noexcept;
    bool hostTimelineDiscontinuity (const HostPosition& position,
                                    int blockSize,
                                    double ppqPerSample) const noexcept;
    void resetLoopState (double blockPpq, double gridLength) noexcept;
    void beginCapture (double followingBoundaryPpq) noexcept;
    void finishCapture (double followingBoundaryPpq, bool softEnabled) noexcept;
    void captureSample (const juce::AudioBuffer<float>& buffer,
                        int sampleIndex,
                        int channelCount) noexcept;
    float getLoopSample (int channel, int logicalIndex) const noexcept;
    float renderLoopSample (int channel,
                            float drySample,
                            double samplePpq,
                            double ppqPerSample,
                            bool softEnabled) const noexcept;
    void advancePlayback() noexcept;
    void updateHostHistory (const HostPosition& position, int blockSize) noexcept;
    void clearHostHistory() noexcept;

    static constexpr int maximumChannels = 2;
    static constexpr double maximumLoopSeconds = 32.0;
    static constexpr double softCrossfadeSeconds = 0.005;

    juce::AudioBuffer<float> loopBuffer;

    std::atomic<float>* lengthParameter = nullptr;
    std::atomic<float>* softParameter = nullptr;

    double currentSampleRate = 44100.0;
    double nextBoundaryPpq = 0.0;
    double activeGridLength = 1.0;
    double previousBlockPpq = 0.0;
    double previousBpm = 120.0;

    std::int64_t previousTimeInSamples = 0;
    int previousBlockSize = 0;
    int maximumLoopSamples = 1;
    int captureWritePosition = 0;
    int capturedSampleCount = 0;
    int loopStartPosition = 0;
    int loopLengthSamples = 0;
    int playbackPosition = 0;
    int activationFadePosition = 0;
    int crossfadeSamples = 1;
    int activeLengthChoice = -1;

    bool previousPositionWasValid = false;
    bool previousPositionWasPlaying = false;
    bool previousPositionHadTimeInSamples = false;
    bool captureHasWrapped = false;
    LoopState loopState = LoopState::waitingForGrid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuiceRepeaterAudioProcessor)
};
