/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

//==============================================================================
/**
*/
class IXMultitapDelayAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    IXMultitapDelayAudioProcessor();
    ~IXMultitapDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept
    {
        return apvts;
    }


private:
    //==============================================================================

    class DelayTap
    {
    public:
        void prepare (double sampleRate,
                      double smoothingTimeSeconds,
                      float initialDelayTimeMs,
                      float initialGainDb)
        {
            currentSampleRate = sampleRate;

            smoothedDelaySamples.reset (sampleRate, smoothingTimeSeconds);
            smoothedGain.reset (sampleRate, smoothingTimeSeconds);

            smoothedDelaySamples.setCurrentAndTargetValue (
                millisecondsToSamples (initialDelayTimeMs));

            smoothedGain.setCurrentAndTargetValue (
                juce::Decibels::decibelsToGain (initialGainDb, -60.0f));

            advance();
        }

        void setTargets (float delayTimeMs, float gainDb)
        {
            smoothedDelaySamples.setTargetValue (
                millisecondsToSamples (delayTimeMs));

            smoothedGain.setTargetValue (
                juce::Decibels::decibelsToGain (gainDb, -60.0f));
        }

        void advance() noexcept
        {
            currentDelaySamples = smoothedDelaySamples.getNextValue();
            currentGain = smoothedGain.getNextValue();
        }

        float getDelaySamples() const noexcept
        {
            return currentDelaySamples;
        }

        float getGain() const noexcept
        {
            return currentGain;
        }

    private:
        float millisecondsToSamples (float delayTimeMs) const noexcept
        {
            return delayTimeMs * 0.001f
                * static_cast<float> (currentSampleRate);
        }

        double currentSampleRate = 44100.0;
        juce::SmoothedValue<float> smoothedDelaySamples;
        juce::SmoothedValue<float> smoothedGain;
        float currentDelaySamples = 1.0f;
        float currentGain = 0.0f;
    };

    static constexpr int maxNumTaps = 8;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout 
    createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    std::array<DelayTap, maxNumTaps> taps;

    std::atomic<float>* tapCountParameter = nullptr;
    std::array<std::atomic<float>*, maxNumTaps> tapTimeParameters {};
    std::array<std::atomic<float>*, maxNumTaps> tapGainParameters {};
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* dryGainParameter = nullptr;
    std::atomic<float>* wetGainParameter = nullptr;

    static constexpr double maxDelayTimeSeconds = 2.0;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;

    juce::SmoothedValue<float> smoothedFeedback;
    juce::SmoothedValue<float> smoothedDryGain;
    juce::SmoothedValue<float> smoothedWetGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IXMultitapDelayAudioProcessor)




    

};
