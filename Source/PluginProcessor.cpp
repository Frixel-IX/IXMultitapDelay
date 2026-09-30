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
    dryGainParameter = apvts.getRawParameterValue ("dryVolDB");
    wetGainParameter = apvts.getRawParameterValue ("wetVolDB");

    jassert (tapCountParameter != nullptr);
    jassert (feedbackParameter != nullptr);
    jassert (dryGainParameter != nullptr);
    jassert (wetGainParameter != nullptr);

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        tapTimeParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Time"));

        tapGainParameters[tapIndex] = apvts.getRawParameterValue (
            makeTapParameterId (tapIndex, "Gain"));

        jassert (tapTimeParameters[tapIndex] != nullptr);
        jassert (tapGainParameters[tapIndex] != nullptr);
    }
}
//=========Declare Value Tree State=========

IXMultitapDelayAudioProcessor::~IXMultitapDelayAudioProcessor()
{
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
        const auto tapNumber = juce::String (tapIndex + 1);
        const float defaultTimeMs = 250.0f * static_cast<float> (tapIndex + 1);
        const float defaultGainDb = -3.0f * static_cast<float> (tapIndex);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Time"), 1 },
            "Tap " + tapNumber + " Time",
            juce::NormalisableRange<float> { 1.0f, 2000.0f, 1.0f },
            defaultTimeMs,
            "ms"));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { makeTapParameterId (tapIndex, "Gain"), 1 },
            "Tap " + tapNumber + " Gain",
            juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
            defaultGainDb,
            "dB"));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feedback", 1 },
        "Feedback",
        juce::NormalisableRange<float> { 0.0f, 95.0f, 0.1f },
        30.0f,
        "%"));

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

    const int maximumDelaySamples =
        static_cast<int> (
            std::ceil (maxDelayTimeSeconds * sampleRate)
        ) + 1;

    delayBuffer.setSize (
        getTotalNumOutputChannels(),
        maximumDelaySamples
    );

    delayBuffer.clear();
    delayWritePosition = 0;

    constexpr double smoothingTimeSeconds = 0.05;

    for (int tapIndex = 0; tapIndex < maxNumTaps; ++tapIndex)
    {
        taps[tapIndex].prepare (
            sampleRate,
            smoothingTimeSeconds,
            tapTimeParameters[tapIndex]->load(),
            tapGainParameters[tapIndex]->load());
    }

    smoothedFeedback.reset (sampleRate, smoothingTimeSeconds);
    smoothedDryGain.reset (sampleRate, smoothingTimeSeconds);
    smoothedWetGain.reset (sampleRate, smoothingTimeSeconds);

    smoothedFeedback.setCurrentAndTargetValue (
        feedbackParameter->load() / 100.0f);

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
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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

void IXMultitapDelayAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
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
        taps[tapIndex].setTargets (
            tapTimeParameters[tapIndex]->load(),
            tapGainParameters[tapIndex]->load());
    }

    smoothedFeedback.setTargetValue (
        feedbackParameter->load() / 100.0f);

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
        }
        //actual processing

        for (int channel = 0;
             channel < totalNumInputChannels;
             ++channel)
        {
            const float inputSample =
                buffer.getSample (channel, sample);

            float wetSample = 0.0f;
            float feedbackSample = 0.0f;

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

                wetSample += delayedSample * tapGain[tapIndex];

                if (tapIndex == 0)
                    feedbackSample = delayedSample;
            }

            const float outputSample =
                currentDryGain * inputSample
                + currentWetGain * wetSample;

            buffer.setSample (
                channel,
                sample,
                outputSample
            );

            delayBuffer.setSample (
                channel,
                delayWritePosition,
                inputSample
                    + feedbackSample
                        * currentFeedback
            );
        }

        ++delayWritePosition;

        if (delayWritePosition >= delayBufferSize)
            delayWritePosition = 0;
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
