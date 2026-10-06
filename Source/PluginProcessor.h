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

    double getCurrentBpmForUI() const noexcept
    {
        return currentBpmForUI.load();
    }

    float getMaximumDelayTimeMsForUI() const noexcept
    {
        return static_cast<float> (maxDelayTimeSeconds * 1000.0);
    }


private:
    //==============================================================================

    class DelayTap
    {
    public:
        void prepare (double sampleRate,
                      double smoothingTimeSeconds,
                      float initialDelayTimeMs,
                      float initialGainDb,
                      float initialHumanizeRangeMs,
                      float initialPan)
        {
            currentSampleRate = sampleRate;

            setHumanizeRange (initialHumanizeRangeMs);
            randomizeHumanizeOffset();

            smoothedDelaySamples.reset (sampleRate, smoothingTimeSeconds);
            smoothedGain.reset (sampleRate, smoothingTimeSeconds);
            smoothedPan.reset (sampleRate, smoothingTimeSeconds);

            smoothedDelaySamples.setCurrentAndTargetValue (
                millisecondsToSamples (
                    juce::jmax (1.0f,
                                initialDelayTimeMs + humanizeOffsetMs)));

            smoothedGain.setCurrentAndTargetValue (
                juce::Decibels::decibelsToGain (initialGainDb, -60.0f));

            smoothedPan.setCurrentAndTargetValue (
                juce::jlimit (-1.0f, 1.0f, initialPan));

            advance();
        }

        void setHumanizeRange (float newRangeMs) noexcept
        {
            humanizeRangeMs = juce::jlimit (0.0f, 100.0f, newRangeMs);
            humanizeOffsetMs = humanizeRandomUnit * humanizeRangeMs;
        }

        void randomizeHumanizeOffset() noexcept
        {
            humanizeRandomUnit = random.nextFloat() * 2.0f - 1.0f;
            humanizeOffsetMs = humanizeRandomUnit * humanizeRangeMs;
        }

        float getHumanizeOffsetMs() const noexcept
        {
            return humanizeOffsetMs;
        }

        void setTargets (float delayTimeMs, float gainDb, float pan)
        {
            smoothedDelaySamples.setTargetValue (
                millisecondsToSamples (delayTimeMs));

            smoothedGain.setTargetValue (
                juce::Decibels::decibelsToGain (gainDb, -60.0f));

            smoothedPan.setTargetValue (
                juce::jlimit (-1.0f, 1.0f, pan));
        }

        void advance() noexcept
        {
            currentDelaySamples = smoothedDelaySamples.getNextValue();
            currentGain = smoothedGain.getNextValue();
            currentPan = smoothedPan.getNextValue();
        }

        float getDelaySamples() const noexcept
        {
            return currentDelaySamples;
        }

        float getGain() const noexcept
        {
            return currentGain;
        }

        float getPan() const noexcept
        {
            return currentPan;
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
        juce::SmoothedValue<float> smoothedPan;
        juce::Random random;
        float currentDelaySamples = 1.0f;
        float currentGain = 0.0f;
        float currentPan = 0.0f;
        float humanizeRangeMs = 0.0f;
        float humanizeRandomUnit = 0.0f;
        float humanizeOffsetMs = 0.0f;
    };

    static constexpr int maxNumTaps = 8;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout 
    createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    std::array<DelayTap, maxNumTaps> taps;

    std::atomic<float>* tapCountParameter = nullptr;
    std::array<std::atomic<float>*, maxNumTaps> tapTimeParameters {};
    std::array<std::atomic<float>*, maxNumTaps> tapGainParameters {};
    std::array<std::atomic<float>*, maxNumTaps> tapDivisionParameters {};
    std::array<std::atomic<float>*, maxNumTaps> tapHumanizeParameters {};
    std::array<std::atomic<float>*, maxNumTaps> tapPanParameters {};
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* feedbackHighCutParameter = nullptr;
    std::atomic<float>* dryGainParameter = nullptr;
    std::atomic<float>* wetGainParameter = nullptr;
    std::atomic<float>* timeModeParameter = nullptr;

    static constexpr double maxDelayTimeSeconds = 2.0;
    static constexpr double maxHumanizeTimeSeconds = 0.1;

    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
    bool wasTransportPlaying = false;
    double currentSampleRate = 44100.0;
    std::atomic<double> currentBpmForUI { 120.0 };
    std::array<float, 2> feedbackFilterStates {};

    juce::SmoothedValue<float> smoothedFeedback;
    juce::SmoothedValue<float> smoothedFeedbackHighCut;
    juce::SmoothedValue<float> smoothedDryGain;
    juce::SmoothedValue<float> smoothedWetGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IXMultitapDelayAudioProcessor)




    

};
