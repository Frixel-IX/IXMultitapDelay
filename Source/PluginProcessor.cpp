/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace std;

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
}
//=========Declare Value Tree State=========

IXMultitapDelayAudioProcessor::~IXMultitapDelayAudioProcessor()
{
}
juce::AudioProcessorValueTreeState::ParameterLayout

IXMultitapDelayAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "delayTime", 1 },
        "Delay Time",
        juce::NormalisableRange<float> { 1.0f, 2000.0f, 1.0f },
        400.0f,
        "ms"
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "feedback", 1 },
        "Feedback",
        juce::NormalisableRange<float> { 0.0f, 95.0f, 0.1f },
        30.0f,
        "%"
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "dryVolDB", 1 },
        "Dry Volume",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
        0.0f,
        "dB"
    ));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "wetVolDB", 1 },
        "Wet Volume",
        juce::NormalisableRange<float> { -60.0f, 6.0f, 0.1f },
        -6.0f,
        "dB"
    ));


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

    smoothedDelaySamples.reset (
    sampleRate,
    smoothingTimeSeconds
    );

    smoothedFeedback.reset (
    sampleRate,
    smoothingTimeSeconds
    );

    smoothedDryGain.reset (
    sampleRate,
    smoothingTimeSeconds
    );

    smoothedWetGain.reset (
    sampleRate,
    smoothingTimeSeconds
    );

    const float initialDelayTimeMs =
        apvts.getRawParameterValue ("delayTime")->load();

    const float initialFeedback =
        apvts.getRawParameterValue ("feedback")->load() / 100.0f;

    const float initialDryDb =
        apvts.getRawParameterValue ("dryVolDB")->load();

    const float initialWetDb =
        apvts.getRawParameterValue ("wetVolDB")->load();

        smoothedDelaySamples.setCurrentAndTargetValue (
            initialDelayTimeMs * 0.001f
            * static_cast<float> (sampleRate)
        );

    smoothedFeedback.setCurrentAndTargetValue (
        initialFeedback
    );

    smoothedDryGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            initialDryDb,
            -60.0f
        )
    );

    smoothedWetGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (
            initialWetDb,
            -60.0f
    )
);

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

    // Read the latest parameter values once per block.
    const float delayTimeMs =
        apvts.getRawParameterValue ("delayTime")->load();

    const float feedback =
        apvts.getRawParameterValue ("feedback")->load()
        / 100.0f;

    const float dryDb =
        apvts.getRawParameterValue ("dryVolDB")->load();

    const float wetDb =
        apvts.getRawParameterValue ("wetVolDB")->load();

    const float targetDelaySamples = juce::jlimit (
        1.0f,
        static_cast<float> (delayBufferSize - 1),
        delayTimeMs * 0.001f
            * static_cast<float> (currentSampleRate)
    );

    const float targetDryGain =
        juce::Decibels::decibelsToGain (
            dryDb,
            -60.0f
        );

    const float targetWetGain =
        juce::Decibels::decibelsToGain (
            wetDb,
            -60.0f
        );

    // Tell each smoother where it should gradually move.
    smoothedDelaySamples.setTargetValue (
        targetDelaySamples
    );

    smoothedFeedback.setTargetValue (
        feedback
    );

    smoothedDryGain.setTargetValue (
        targetDryGain
    );

    smoothedWetGain.setTargetValue (
        targetWetGain
    );

    for (int sample = 0;
         sample < buffer.getNumSamples();
         ++sample)
    {
        // Advance each smoother exactly once per sample.
        const float currentDelaySamples =
            smoothedDelaySamples.getNextValue();

        const float currentFeedback =
            smoothedFeedback.getNextValue();

        const float currentDryGain =
            smoothedDryGain.getNextValue();

        const float currentWetGain =
            smoothedWetGain.getNextValue();

        float readPosition =
            static_cast<float> (delayWritePosition)
            - currentDelaySamples;

        if (readPosition < 0.0f)
            readPosition +=
                static_cast<float> (delayBufferSize);

        const int readIndexA =
            static_cast<int> (floor (readPosition));

        const int readIndexB =
            (readIndexA + 1) % delayBufferSize;

        const float fraction =
            readPosition
            - static_cast<float> (readIndexA);

        for (int channel = 0;
             channel < totalNumInputChannels;
             ++channel)
        {
            const float inputSample =
                buffer.getSample (channel, sample);

            const float delayedSampleA =
                delayBuffer.getSample (
                    channel,
                    readIndexA
                );

            const float delayedSampleB =
                delayBuffer.getSample (
                    channel,
                    readIndexB
                );

            const float delayedSample =
                delayedSampleA
                + fraction
                    * (delayedSampleB
                       - delayedSampleA);

            const float outputSample =
                currentDryGain * inputSample
                + currentWetGain * delayedSample;

            buffer.setSample (
                channel,
                sample,
                outputSample
            );

            delayBuffer.setSample (
                channel,
                delayWritePosition,
                inputSample
                    + delayedSample
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
