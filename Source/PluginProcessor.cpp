#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr double boundaryEpsilon = 1.0e-9;
}

juce::AudioProcessorValueTreeState::ParameterLayout
JuiceRepeaterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::length, 1 },
        "Length",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16" },
        2));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::soft, 1 },
        "Soft",
        true));

    return { parameters.begin(), parameters.end() };
}

JuiceRepeaterAudioProcessor::JuiceRepeaterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    lengthParameter = apvts.getRawParameterValue (ParameterIDs::length);
    softParameter = apvts.getRawParameterValue (ParameterIDs::soft);
}

JuiceRepeaterAudioProcessor::~JuiceRepeaterAudioProcessor() = default;

void JuiceRepeaterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = juce::jmax (1.0, sampleRate);
    maximumLoopSamples = juce::jmax (
        1, static_cast<int> (std::ceil (currentSampleRate * maximumLoopSeconds)));
    crossfadeSamples = juce::jmax (
        1, static_cast<int> (std::round (currentSampleRate * softCrossfadeSeconds)));

    loopBuffer.setSize (maximumChannels, maximumLoopSamples, false, true, false);
    loopBuffer.clear();

    activeLengthChoice = -1;
    resetLoopState (0.0, 1.0);
    clearHostHistory();
}

void JuiceRepeaterAudioProcessor::releaseResources()
{
    loopBuffer.setSize (0, 0);
    resetLoopState (0.0, 1.0);
    clearHostHistory();
}

bool JuiceRepeaterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    return input == output
           && (output == juce::AudioChannelSet::mono()
               || output == juce::AudioChannelSet::stereo());
}

void JuiceRepeaterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();
    const int processChannels = juce::jmin (
        maximumChannels,
        juce::jmin (inputChannels, juce::jmin (outputChannels, buffer.getNumChannels())));
    const int sampleCount = buffer.getNumSamples();

    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear (channel, 0, sampleCount);

    if (sampleCount == 0 || processChannels == 0)
        return;

    const auto host = getHostPosition();
    const int requestedLengthChoice = juce::jlimit (
        0, 4, static_cast<int> (std::round (lengthParameter->load())));
    const bool softEnabled = softParameter->load() >= 0.5f;
    const double requestedGridLength = getQuarterNotesForChoice (requestedLengthChoice);

    if (! host.valid || ! host.playing)
    {
        activeLengthChoice = requestedLengthChoice;
        resetLoopState (host.ppq, requestedGridLength);
        clearHostHistory();
        return;
    }

    const double ppqPerSample = host.bpm / (60.0 * currentSampleRate);
    const bool lengthChanged = requestedLengthChoice != activeLengthChoice;
    const bool timelineMoved = hostTimelineDiscontinuity (host, sampleCount, ppqPerSample);

    if (lengthChanged || timelineMoved)
    {
        activeLengthChoice = requestedLengthChoice;
        resetLoopState (host.ppq, requestedGridLength);
    }

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const double samplePpq = host.ppq + static_cast<double> (sample) * ppqPerSample;

        while (samplePpq + boundaryEpsilon >= nextBoundaryPpq)
        {
            const double followingBoundary = nextBoundaryPpq + activeGridLength;

            if (loopState == LoopState::waitingForGrid)
                beginCapture (followingBoundary);
            else if (loopState == LoopState::capturing)
                finishCapture (followingBoundary, softEnabled);
            else
            {
                const int fadeLength = juce::jmin (
                    crossfadeSamples, juce::jmax (1, loopLengthSamples / 2));
                playbackPosition = softEnabled ? juce::jmin (fadeLength, loopLengthSamples - 1) : 0;
                nextBoundaryPpq = followingBoundary;
            }
        }

        if (loopState == LoopState::capturing)
        {
            captureSample (buffer, sample, processChannels);
            continue;
        }

        if (loopState != LoopState::repeating || loopLengthSamples <= 0)
            continue;

        for (int channel = 0; channel < processChannels; ++channel)
        {
            const float dry = buffer.getSample (channel, sample);
            buffer.setSample (
                channel,
                sample,
                renderLoopSample (channel, dry, samplePpq, ppqPerSample, softEnabled));
        }

        advancePlayback();
    }

    updateHostHistory (host, sampleCount);
}

void JuiceRepeaterAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void JuiceRepeaterAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

double JuiceRepeaterAudioProcessor::getQuarterNotesForChoice (int choice) noexcept
{
    constexpr double lengths[] { 4.0, 2.0, 1.0, 0.5, 0.25 };
    return lengths[juce::jlimit (0, 4, choice)];
}

double JuiceRepeaterAudioProcessor::getBoundaryAtOrAfter (double ppq,
                                                           double gridLength) noexcept
{
    if (! std::isfinite (ppq) || ! std::isfinite (gridLength) || gridLength <= 0.0)
        return 0.0;

    return std::ceil ((ppq - boundaryEpsilon) / gridLength) * gridLength;
}

float JuiceRepeaterAudioProcessor::smoothstep (float x) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}

JuiceRepeaterAudioProcessor::HostPosition JuiceRepeaterAudioProcessor::getHostPosition() const noexcept
{
    HostPosition result;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            const auto bpm = position->getBpm();
            const auto ppq = position->getPpqPosition();

            if (bpm && ppq
                && std::isfinite (*bpm) && std::isfinite (*ppq) && *bpm > 0.0)
            {
                result.valid = true;
                result.playing = position->getIsPlaying();
                result.bpm = *bpm;
                result.ppq = *ppq;
            }

            if (const auto time = position->getTimeInSamples())
            {
                result.hasTimeInSamples = true;
                result.timeInSamples = *time;
            }
        }
    }

    return result;
}

bool JuiceRepeaterAudioProcessor::hostTimelineDiscontinuity (
    const HostPosition& position,
    int blockSize,
    double ppqPerSample) const noexcept
{
    juce::ignoreUnused (blockSize);

    if (! previousPositionWasValid || ! previousPositionWasPlaying)
        return true;

    if (std::abs (position.bpm - previousBpm) > 1.0e-4)
        return true;

    if (position.hasTimeInSamples && previousPositionHadTimeInSamples)
    {
        const auto expected = previousTimeInSamples + previousBlockSize;
        if (std::llabs (position.timeInSamples - expected) > 1)
            return true;
    }

    const double expectedPpq = previousBlockPpq
                               + static_cast<double> (previousBlockSize)
                                     * previousBpm / (60.0 * currentSampleRate);
    const double tolerance = juce::jmax (1.0e-7, std::abs (ppqPerSample) * 2.0);
    return std::abs (position.ppq - expectedPpq) > tolerance;
}

void JuiceRepeaterAudioProcessor::resetLoopState (double blockPpq,
                                                   double gridLength) noexcept
{
    activeGridLength = juce::jmax (0.25, gridLength);
    nextBoundaryPpq = getBoundaryAtOrAfter (blockPpq, activeGridLength);
    captureWritePosition = 0;
    capturedSampleCount = 0;
    captureHasWrapped = false;
    loopStartPosition = 0;
    loopLengthSamples = 0;
    playbackPosition = 0;
    activationFadePosition = crossfadeSamples;
    loopState = LoopState::waitingForGrid;
}

void JuiceRepeaterAudioProcessor::beginCapture (double followingBoundaryPpq) noexcept
{
    captureWritePosition = 0;
    capturedSampleCount = 0;
    captureHasWrapped = false;
    loopStartPosition = 0;
    loopLengthSamples = 0;
    playbackPosition = 0;
    nextBoundaryPpq = followingBoundaryPpq;
    loopState = LoopState::capturing;
}

void JuiceRepeaterAudioProcessor::finishCapture (double followingBoundaryPpq,
                                                  bool softEnabled) noexcept
{
    loopLengthSamples = capturedSampleCount;
    loopStartPosition = captureHasWrapped ? captureWritePosition : 0;
    playbackPosition = 0;
    activationFadePosition = softEnabled ? 0 : crossfadeSamples;
    nextBoundaryPpq = followingBoundaryPpq;
    loopState = loopLengthSamples > 0 ? LoopState::repeating : LoopState::waitingForGrid;
}

void JuiceRepeaterAudioProcessor::captureSample (const juce::AudioBuffer<float>& buffer,
                                                  int sampleIndex,
                                                  int channelCount) noexcept
{
    if (maximumLoopSamples <= 0)
        return;

    for (int channel = 0; channel < channelCount; ++channel)
        loopBuffer.setSample (channel, captureWritePosition, buffer.getSample (channel, sampleIndex));

    captureWritePosition = (captureWritePosition + 1) % maximumLoopSamples;

    if (capturedSampleCount < maximumLoopSamples)
        ++capturedSampleCount;
    else
        captureHasWrapped = true;
}

float JuiceRepeaterAudioProcessor::getLoopSample (int channel, int logicalIndex) const noexcept
{
    if (loopLengthSamples <= 0 || maximumLoopSamples <= 0)
        return 0.0f;

    logicalIndex %= loopLengthSamples;
    if (logicalIndex < 0)
        logicalIndex += loopLengthSamples;

    const int physicalIndex = (loopStartPosition + logicalIndex) % maximumLoopSamples;
    return loopBuffer.getSample (channel, physicalIndex);
}

float JuiceRepeaterAudioProcessor::renderLoopSample (int channel,
                                                      float drySample,
                                                      double samplePpq,
                                                      double ppqPerSample,
                                                      bool softEnabled) const noexcept
{
    const float loopSample = getLoopSample (channel, playbackPosition);

    if (! softEnabled)
        return loopSample;

    float result = loopSample;
    const int fadeLength = juce::jmin (crossfadeSamples, juce::jmax (1, loopLengthSamples / 2));

    if (activationFadePosition < fadeLength)
    {
        const float x = static_cast<float> (activationFadePosition + 1)
                        / static_cast<float> (fadeLength);
        const float mix = smoothstep (x);
        result = drySample + (loopSample - drySample) * mix;
    }

    if (ppqPerSample > 0.0)
    {
        const double samplesUntilBoundary = (nextBoundaryPpq - samplePpq) / ppqPerSample;

        if (samplesUntilBoundary > 0.0
            && samplesUntilBoundary <= static_cast<double> (fadeLength))
        {
            const float x = static_cast<float> (
                1.0 - samplesUntilBoundary / static_cast<double> (fadeLength));
            const float mix = smoothstep (x);
            const int headIndex = juce::jlimit (
                0, fadeLength - 1, static_cast<int> (std::floor (x * fadeLength)));
            const float loopHead = getLoopSample (channel, headIndex);
            result += (loopHead - result) * mix;
        }
    }

    return result;
}

void JuiceRepeaterAudioProcessor::advancePlayback() noexcept
{
    if (loopLengthSamples <= 0)
        return;

    playbackPosition = juce::jmin (playbackPosition + 1, loopLengthSamples - 1);

    if (activationFadePosition < crossfadeSamples)
        ++activationFadePosition;
}

void JuiceRepeaterAudioProcessor::updateHostHistory (const HostPosition& position,
                                                      int blockSize) noexcept
{
    previousPositionWasValid = position.valid;
    previousPositionWasPlaying = position.playing;
    previousPositionHadTimeInSamples = position.hasTimeInSamples;
    previousBlockPpq = position.ppq;
    previousBpm = position.bpm;
    previousTimeInSamples = position.timeInSamples;
    previousBlockSize = blockSize;
}

void JuiceRepeaterAudioProcessor::clearHostHistory() noexcept
{
    previousPositionWasValid = false;
    previousPositionWasPlaying = false;
    previousPositionHadTimeInSamples = false;
    previousBlockPpq = 0.0;
    previousBpm = 120.0;
    previousTimeInSamples = 0;
    previousBlockSize = 0;
}

juce::AudioProcessorEditor* JuiceRepeaterAudioProcessor::createEditor()
{
    return new JuiceRepeaterAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuiceRepeaterAudioProcessor();
}
