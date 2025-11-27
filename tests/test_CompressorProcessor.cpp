//Austin 

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/CompressorProcessor.h"

class CompressorProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<CompressorProcessor> processor;

    void SetUp() override {
        processor = std::make_unique<CompressorProcessor>();
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

//Test for TC-09
TEST_F(CompressorProcessorTest, SignalBelowThreshold) {
    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);


    processor->setThreshold(-30.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-40.0f), 0.001f);
}

//Test for TC-10
TEST_F(CompressorProcessorTest, SignalAboveThreshold) {
    juce::AudioBuffer<float> buffer(1, 512);
    
    processor->prepare(44100);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-18.0f), 0.005f);
}

//Test for TC-11
TEST_F(CompressorProcessorTest, Limiter) {
    juce::AudioBuffer<float> buffer(1, 512);
    
    processor->prepare(44100);

    processor->setThreshold(-40.0f);
    processor->setRatio(20000.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-20.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-40.0f), 0.005f);
}

//Test for TC-012
TEST_F(CompressorProcessorTest, AttackTime){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(50.0f);
    processor->setRelease(10.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-40.0f));

    simulateConstantSignal(buffer, 50.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),juce::Decibels::decibelsToGain(-18.0f),0.005f);
}

//Test for TC-13
TEST_F(CompressorProcessorTest, ReleaseTime){

    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(50.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-12.0f));

    simulateConstantSignal(buffer, 50.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),juce::Decibels::decibelsToGain(-40.0f),0.001f);
}