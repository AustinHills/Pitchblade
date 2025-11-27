//Austin

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/NoiseGateProcessor.h"

// Helper class
class NoiseGateProcessorTest : public::testing::Test{
protected:
    std::unique_ptr<NoiseGateProcessor> processor;

    void SetUp() override {
        processor = std::make_unique<NoiseGateProcessor>();
    }

    //Helper to calculate blocks needed for a duration in ms
    int blocksForMS(float ms){
        return (int)(std::ceil((ms/1000) * 44100 / (double)512));
    }

    //Helper to simulate a constant signal
    void simulateConstantSignal(juce::AudioBuffer<float>& buffer, float msDuration, float signalValue){
        int numBlocks = blocksForMS(msDuration);
        for(int i = 0; i < numBlocks; i++){
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel){
                juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), signalValue, buffer.getNumSamples());
            }
            processor->process(buffer);
        }
    }
};

//Test for TC-05
TEST_F(NoiseGateProcessorTest, SignalBelowThreshold){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-30.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),0.0f,0.001f);
}

//Test for TC-06
TEST_F(NoiseGateProcessorTest, SignalAboveThreshold){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-30.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-20.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),juce::Decibels::decibelsToGain(-20.0f),0.001f);
}

//Test for TC-07
TEST_F(NoiseGateProcessorTest, AttackTime){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-30.0f);
    processor->setAttack(50.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-40.0f));

    simulateConstantSignal(buffer, 50.0f, juce::Decibels::decibelsToGain(-20.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),juce::Decibels::decibelsToGain(-20.0f),0.0001f);
}

//Test for TC-08
TEST_F(NoiseGateProcessorTest, ReleaseTime){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-30.0f);
    processor->setAttack(10.0f);
    processor->setRelease(50.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-20.0f));

    simulateConstantSignal(buffer, 59.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),0.0,0.0001f);
}