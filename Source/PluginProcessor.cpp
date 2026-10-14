/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String makeTapParameterId (int tapIndex, const char* suffix)
{
    return juce::String ("tap")
        + juce::String (tapIndex + 1)
        + suffix;
}
}

//==============================================================================
IXMultitapDelayAudioProcessor::IXMultitapDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (
        BusesProperties()
       #if ! JucePlugin_IsMidiEffect
        #if ! JucePlugin_IsSynth
         .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        #endif
         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
       #endif
      ),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#else
    : apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    tapCountParameter = apvts.getRawParameterValue ("tapCount");
    feedbackParameter = apvts.getRawParameterValue ("feedback");
    feedbackHighCutParameter = apvts.getRawParameterValue ("feedbackHighCut");
    dryGainParameter = apvts.getRawParameterValue ("dryVolDB");
    wetGainParameter = apvts.getRawParameterValue ("wetVolDB");
    timeModeParameter = apvts.getRawParameterValue ("timeMode");

    jassert (tapCountParameter != nullptr);
    jassert (feedbackParameter != nullptr);
    jassert (feedbackHighCutParameter != nullptr);
    jassert (dryGainParameter != nullptr);
    jassert (wetGainParameter != nullptr);
    jassert (timeModeParameter != nullptr);

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        tapTimeParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Time"));

        tapGainParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Gain"));

        tapDivisionParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Division"));

        tapHumanizeParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Humanize"));

        tapPanParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Pan"));

        jassert (tapTimeParameters[tapIndex] != nullptr);
        jassert (tapGainParameters[tapIndex] != nullptr);
        jassert (tapDivisionParameters[tapIndex] != nullptr);
        jassert (tapHumanizeParameters[tapIndex] != nullptr);
        jassert (tapPanParameters[tapIndex] != nullptr);
    }
}
//=========Declare Value Tree State=========

IXMultitapDelayAudioProcessor::~IXMultitapDelayAudioProcessor()
{
}

float IXMultitapDelayAudioProcessor::consumeTapVisualLevel (
    int tapIndex, int channel) noexcept
{
    if (! juce::isPositiveAndBelow (tapIndex, maxNumTaps)
        || ! juce::isPositiveAndBelow (channel, 2))
        return 0.0f;

    return tapVisualLevels[static_cast<size_t> (channel)]
                          [static_cast<size_t> (tapIndex)]
                              .exchange (0.0f, std::memory_order_relaxed);
}

int IXMultitapDelayAudioProcessor::pullWetWaveformSamples (
    float* destination, int maximumSamples) noexcept
{
    if (destination == nullptr || maximumSamples <= 0)
        return 0;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    wetWaveformFifo.prepareToRead (maximumSamples,
                                   start1, size1, start2, size2);
    std::copy_n (wetWaveformFifoBuffer.begin() + start1,
                 size1, destination);
    std::copy_n (wetWaveformFifoBuffer.begin() + start2,
                 size2, destination + size1);
    wetWaveformFifo.finishedRead (size1 + size2);
    return size1 + size2;
}
juce::AudioProcessorValueTreeState::ParameterLayout

IXMultitapDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "tapCount", 1 },
        "Tap Count",
        1,
        maxNumTaps,
        2));

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        static constexpr std::array<int, maxNumTaps> defaultDivisionIndices {
            4, 3, 9, 2, 8, 1, 7, 0
        };

        const auto tapNumber = juce::String (tapIndex + 1);
        const float defaultTimeMs = 250.0f * static_cast<float> (tapIndex + 1);
        const float defaultGainDb = juce::jmax (
            -24.0f,
            -6.0f - 3.0f * static_cast<float> (tapIndex));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Time"), 1 },
            "Tap " + tapNumber + " Time",
            juce::NormalisableRange<float> { 1.0f, 2000.0f, 1.0f },
            defaultTimeMs,
            "ms"));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Gain"), 1 },
            "Tap " + tapNumber + " Gain",
            juce::NormalisableRange<float> { -24.0f, 0.0f, 0.1f },
            defaultGainDb,
            "dB"));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Division"), 1 },
            "Tap " + tapNumber + " Division",
            juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
                                "1/2.", "1/4.", "1/8.", "1/16.", "1/32.",
                                "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" },
            defaultDivisionIndices[static_cast<size_t> (tapIndex)]));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Humanize"), 1 },
            "Tap " + tapNumber + " Humanize Range",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
            0.0f,
            "ms"));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Pan"), 1 },
            "Tap " + tapNumber + " Pan",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 1.0f },
            0.0f,
            "%"));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feedback", 1 },
        "Feedback",
        juce::NormalisableRange<float> { 0.0f, 95.0f, 0.1f },
        0.0f,
        "%"));

    auto feedbackHighCutRange = juce::NormalisableRange<float> {
        500.0f, 20000.0f, 1.0f
    };
    feedbackHighCutRange.setSkewForCentre (5000.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feedbackHighCut", 1 },
        "Feedback High Cut",
        feedbackHighCutRange,
        8000.0f,
        "Hz"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dryVolDB", 1 },
        "Dry Volume",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
        0.0f,
        "dB"));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "wetVolDB", 1 },
        "Wet Volume",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
        -6.0f,
        "dB"));
    
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "timeMode", 1 },
        "Time Mode",
        juce::StringArray { "Milliseconds", "Beats" },
        0));

    return layout;
}

//==============================================================================
const juce::String IXMultitapDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool IXMultitapDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool IXMultitapDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool IXMultitapDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double IXMultitapDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int IXMultitapDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int IXMultitapDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void IXMultitapDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String IXMultitapDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void IXMultitapDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void IXMultitapDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;
    feedbackFilterStates.fill (0.0f);

    const int maximumDelaySamples =
        static_cast<int> (
            std::ceil ((maxDelayTimeSeconds + maxHumanizeTimeSeconds)
                       * sampleRate)
        ) + 1;

    delayBuffer.setSize (
        getTotalNumOutputChannels(),
        maximumDelaySamples
    );

    delayBuffer.clear();
    delayWritePosition = 0;
    wasTransportPlaying = false;
    wetWaveformFifo.reset();
    wetWaveformDecimationCounter = 0;
    for (auto& channelLevels : tapVisualLevels)
        for (auto& level : channelLevels)
            level.store (0.0f, std::memory_order_relaxed);

    constexpr double smoothingTimeSeconds = 0.05;

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        taps[tapIndex].prepare (
            sampleRate,
            smoothingTimeSeconds,
            tapTimeParameters[tapIndex]->load(),
            tapGainParameters[tapIndex]->load(),
            tapHumanizeParameters[tapIndex]->load(),
            tapPanParameters[tapIndex]->load() / 100.0f);
    }

    smoothedFeedback.reset (sampleRate, smoothingTimeSeconds);
    smoothedFeedbackHighCut.reset (sampleRate, smoothingTimeSeconds);
    smoothedDryGain.reset (sampleRate, smoothingTimeSeconds);
    smoothedWetGain.reset (sampleRate, smoothingTimeSeconds);

    smoothedFeedback.setCurrentAndTargetValue (
        feedbackParameter->load() / 100.0f);

    smoothedFeedbackHighCut.setCurrentAndTargetValue (
        feedbackHighCutParameter->load());

    smoothedDryGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            dryGainParameter->load(),
            -60.0f));

    smoothedWetGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            wetGainParameter->load(),
            -60.0f));

}

void IXMultitapDelayAudioProcessor::releaseResources()
{
    delayBuffer.clear();
    delayWritePosition =0;
        feedbackFilterStates.fill (0.0f);
    wetWaveformFifo.reset();

    for (auto& channelLevels : tapVisualLevels)
        for (auto& level : channelLevels)
            level.store (0.0f, std::memory_order_relaxed);

    
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool IXMultitapDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif


//helper function to convert division index to quarter notes
static float divisionIndexToQuarterNotes (int divisionIndex)
{
    switch (divisionIndex)
    {
        case 0: return 4.0f;   // 1/1
        case 1: return 2.0f;   // 1/2
        case 2: return 1.0f;   // 1/4
        case 3: return 0.5f;   // 1/8
        case 4: return 0.25f;  // 1/16
        case 5: return 0.125f; // 1/32
        case 6: return 0.0625f;// 1/64
        case 7: return 3.0f;   // dotted 1/2
        case 8: return 1.5f;   // dotted 1/4
        case 9: return 0.75f;  // dotted 1/8
        case 10: return 0.375f;// dotted 1/16
        case 11: return 0.1875f;// dotted 1/32
        case 12: return 4.0f / 3.0f;  // 1/2 triplet
        case 13: return 2.0f / 3.0f;  // 1/4 triplet
        case 14: return 1.0f / 3.0f;  // 1/8 triplet
        case 15: return 1.0f / 6.0f;  // 1/16 triplet
        case 16: return 1.0f / 12.0f; // 1/32 triplet
        default: return 1.0f;  // Default to 1/4
    }
}

static float applySafetySoftClip (float input) noexcept
{
    constexpr float knee = 0.8f;
    const float magnitude = std::abs (input);

    if (magnitude <= knee)
        return input;

    const float protectedMagnitude = knee
        + (1.0f - knee)
            * std::tanh ((magnitude - knee) / (1.0f - knee));

    return std::copysign (protectedMagnitude, input);
}


void IXMultitapDelayAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    double bpm = 120.0;
    bool transportIsPlaying = false;
    bool hostProvidedTransport = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            hostProvidedTransport = true;
            transportIsPlaying = position->getIsPlaying();

            if (auto hostBpm = position->getBpm())
            {
                if (*hostBpm > 0.0)
                    bpm = *hostBpm;
            }
        }
    }

    const bool transportJustStarted =
        hostProvidedTransport
        && transportIsPlaying
        && ! wasTransportPlaying;

    wasTransportPlaying =
        hostProvidedTransport && transportIsPlaying;

    const float msPerQuarterNote =
        60000.0f / static_cast<float> (bpm);

    currentBpmForUI.store (bpm);

    const bool tempoSyncEnabled =
        juce::roundToInt (timeModeParameter->load()) == 1;

    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels =
        getTotalNumInputChannels();
 
    const int totalNumOutputChannels =
        getTotalNumOutputChannels();

    for (int channel = totalNumInputChannels;
         channel < totalNumOutputChannels;
         ++channel)
    {
        buffer.clear (
            channel,
            0,
            buffer.getNumSamples()
        );
    }

    const int delayBufferSize =
        delayBuffer.getNumSamples();

    if (delayBufferSize == 0)
        return;

    const int activeTapCount = juce::jlimit (
        1,
        maxNumTaps,
        juce::roundToInt (tapCountParameter->load()));

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        taps[tapIndex].setHumanizeRange (
            tapHumanizeParameters[tapIndex]->load());

        if (transportJustStarted)
            taps[tapIndex].randomizeHumanizeOffset();

        float effectiveDelayTimeMs =
            tapTimeParameters[tapIndex]->load();

        if (tempoSyncEnabled)
        {
            const int divisionIndex = juce::roundToInt (
                tapDivisionParameters[tapIndex]->load());

            effectiveDelayTimeMs =
                divisionIndexToQuarterNotes (divisionIndex)
                * msPerQuarterNote;
        }

        effectiveDelayTimeMs = juce::jlimit (
            1.0f,
            static_cast<float> (maxDelayTimeSeconds * 1000.0),
            effectiveDelayTimeMs);

        effectiveDelayTimeMs +=
            taps[tapIndex].getHumanizeOffsetMs();

        effectiveDelayTimeMs = juce::jlimit (
            1.0f,
            static_cast<float> (
                (maxDelayTimeSeconds + maxHumanizeTimeSeconds) * 1000.0),
            effectiveDelayTimeMs);

        taps[tapIndex].setTargets (
            effectiveDelayTimeMs,
            tapGainParameters[tapIndex]->load(),
            tapPanParameters[tapIndex]->load() / 100.0f);
    }

    smoothedFeedback.setTargetValue (
        feedbackParameter->load() / 100.0f);

    smoothedFeedbackHighCut.setTargetValue (
        feedbackHighCutParameter->load());

    smoothedDryGain.setTargetValue (
        juce::Decibels::decibelsToGain (
            dryGainParameter->load(),
            -60.0f));

    smoothedWetGain.setTargetValue (
        juce::Decibels::decibelsToGain (
            wetGainParameter->load(),
            -60.0f));

    std::array<int, maxNumTaps> readIndexA {};
    std::array<int, maxNumTaps> readIndexB {};
    std::array<float, maxNumTaps> readFraction {};
    std::array<float, maxNumTaps> tapGain {};
    std::array<float, maxNumTaps> tapPan {};
    std::array<std::array<float, maxNumTaps>, 2> tapBlockPeaks {};

    for (int sample = 0;
         sample < buffer.getNumSamples();
         ++sample)
    {
        const float currentFeedback =
            smoothedFeedback.getNextValue();

        const float currentDryGain =
            smoothedDryGain.getNextValue();

        const float currentWetGain =
            smoothedWetGain.getNextValue();

        const float currentHighCutHz = juce::jlimit (
            20.0f,
            static_cast<float> (currentSampleRate * 0.45),
            smoothedFeedbackHighCut.getNextValue());

        const float feedbackFilterCoefficient =
            1.0f - std::exp (
                -juce::MathConstants<float>::twoPi
                * currentHighCutHz
                / static_cast<float> (currentSampleRate));
    //getting read & write pos for each tap

        for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
            taps[tapIndex].advance();

        for (int tapIndex = 0; tapIndex < activeTapCount; ++tapIndex)
        {
            const float delaySamples = juce::jlimit (
                1.0f,
                static_cast<float> (delayBufferSize - 1),
                taps[tapIndex].getDelaySamples());

            float readPosition =
                static_cast<float> (delayWritePosition) - delaySamples;

            if (readPosition < 0.0f)
                readPosition += static_cast<float> (delayBufferSize);

            readIndexA[tapIndex] =
                static_cast<int> (std::floor (readPosition));

            readIndexB[tapIndex] =
                (readIndexA[tapIndex] + 1) % delayBufferSize;

            readFraction[tapIndex] =
                readPosition - static_cast<float> (readIndexA[tapIndex]);

            tapGain[tapIndex] = taps[tapIndex].getGain();
            tapPan[tapIndex] = taps[tapIndex].getPan();
        }

        std::array<std::array<float, maxNumTaps>, 2> delayedSamples {};

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            for (int tapIndex = 0; tapIndex < activeTapCount; ++tapIndex)
            {
                const float delayedSampleA = delayBuffer.getSample (
                    channel,
                    readIndexA[tapIndex]);

                const float delayedSampleB = delayBuffer.getSample (
                    channel,
                    readIndexB[tapIndex]);

                const float delayedSample = delayedSampleA
                    + readFraction[tapIndex]
                        * (delayedSampleB - delayedSampleA);

                delayedSamples[static_cast<size_t> (channel)]
                              [static_cast<size_t> (tapIndex)] = delayedSample;
            }
        }

        std::array<float, 2> wetSamples {};

        for (int tapIndex = 0; tapIndex < activeTapCount; ++tapIndex)
        {
            if (totalNumInputChannels == 1)
            {
                const float contribution = delayedSamples[0][tapIndex]
                    * tapGain[tapIndex];
                wetSamples[0] += contribution;
                tapBlockPeaks[0][tapIndex] = juce::jmax (
                    tapBlockPeaks[0][tapIndex],
                    std::abs (contribution * currentWetGain));
                continue;
            }

            const float monoDelayedSample = 0.5f
                * (delayedSamples[0][tapIndex]
                   + delayedSamples[1][tapIndex]);

            const float pan = tapPan[tapIndex];
            const float leftPanGain = pan <= 0.0f ? 1.0f : 1.0f - pan;
            const float rightPanGain = pan >= 0.0f ? 1.0f : 1.0f + pan;

            const float leftContribution = monoDelayedSample
                * tapGain[tapIndex]
                * leftPanGain;
            const float rightContribution = monoDelayedSample
                * tapGain[tapIndex]
                * rightPanGain;
            wetSamples[0] += leftContribution;
            wetSamples[1] += rightContribution;
            tapBlockPeaks[0][tapIndex] = juce::jmax (
                tapBlockPeaks[0][tapIndex],
                std::abs (leftContribution * currentWetGain));
            tapBlockPeaks[1][tapIndex] = juce::jmax (
                tapBlockPeaks[1][tapIndex],
                std::abs (rightContribution * currentWetGain));
        }

        if (++wetWaveformDecimationCounter >= 32)
        {
            wetWaveformDecimationCounter = 0;
            const float wetWaveformSample = currentWetGain
                * (totalNumInputChannels > 1
                    ? 0.5f * (wetSamples[0] + wetSamples[1])
                    : wetSamples[0]);
            int start1 = 0;
            int size1 = 0;
            int start2 = 0;
            int size2 = 0;
            wetWaveformFifo.prepareToWrite (1,
                                            start1, size1, start2, size2);
            if (size1 > 0)
                wetWaveformFifoBuffer[static_cast<size_t> (start1)]
                    = wetWaveformSample;
            else if (size2 > 0)
                wetWaveformFifoBuffer[static_cast<size_t> (start2)]
                    = wetWaveformSample;
            wetWaveformFifo.finishedWrite (size1 + size2);
        }

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const float inputSample = buffer.getSample (channel, sample);

            const float feedbackSample =
                delayedSamples[static_cast<size_t> (channel)][0];

            auto& feedbackFilterState =
                feedbackFilterStates[static_cast<size_t> (channel)];

            feedbackFilterState += feedbackFilterCoefficient
                * (feedbackSample - feedbackFilterState);

            const float outputSample = applySafetySoftClip (
                currentDryGain * inputSample
                + currentWetGain
                    * wetSamples[static_cast<size_t> (channel)]);

            buffer.setSample (
                channel,
                sample,
                outputSample
            );

            delayBuffer.setSample (
                channel,
                delayWritePosition,
                inputSample
                    + feedbackFilterState
                        * currentFeedback
            );
        }

        ++delayWritePosition;

        if (delayWritePosition >= delayBufferSize)
            delayWritePosition = 0;
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int tapIndex = 0; tapIndex < activeTapCount; ++tapIndex)
        {
            auto& destination = tapVisualLevels[static_cast<size_t> (channel)]
                                               [static_cast<size_t> (tapIndex)];
            const float newPeak = tapBlockPeaks[static_cast<size_t> (channel)]
                                               [static_cast<size_t> (tapIndex)];
            float existing = destination.load (std::memory_order_relaxed);
            while (newPeak > existing
                   && ! destination.compare_exchange_weak (
                       existing, newPeak,
                       std::memory_order_relaxed,
                       std::memory_order_relaxed))
            {
            }
        }
    }
}

//==============================================================================
bool IXMultitapDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* IXMultitapDelayAudioProcessor::createEditor()
{
    return new IXMultitapDelayAudioProcessorEditor (*this);
}

//==============================================================================
void IXMultitapDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();

    copyXmlToBinary (*xml, destData);
}

void IXMultitapDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr
        && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (
            juce::ValueTree::fromXml (*xml)
        );
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IXMultitapDelayAudioProcessor();
}
