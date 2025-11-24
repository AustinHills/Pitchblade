//Austin

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/DeEsserProcessor.h"

class DeEsserProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<DeEsserProcessor> processor;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;

    //Keep track of phase across blocks
    double currentPhase = 0.0;

    void SetUp() override {
        processor = std::make_unique<DeEsserProcessor>();
        currentPhase = 0.0;
    }

    //Helper to calculate blocks needed for a duration in ms
    int blocksForMS(float ms){
        return (int)(std::ceil((ms/1000) * 44100 / (double)512));
    }

    std::vector<float> makeSineFrame(float frequency, int bufferSize){
        std::vector<float> sineFrame(bufferSize);

        double phaseIncrement = (2.0 * juce::MathConstants<double>::pi * frequency) / sampleRate;

        for(int i = 0; i < bufferSize; ++i){
            sineFrame[i] = float(std::sin(currentPhase));

            currentPhase += phaseIncrement;
            if (currentPhase >= 2.0 * juce::MathConstants<double>::pi) {
                currentPhase -= 2.0 * juce::MathConstants<double>::pi;
            }
        }
        return sineFrame;
    }

    //Helper to simulate a sine wave
    void simulateSineSignal(juce::AudioBuffer<float>& buffer, float msDuration, float frequency, float amplitude){
        int numBlocks = blocksForMS(msDuration);

        for(int i = 0; i < numBlocks; i++){

            auto sineData = makeSineFrame(frequency, samplesPerBlock);
        
            juce::FloatVectorOperations::multiply(sineData.data(), amplitude, samplesPerBlock);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel){
                juce::FloatVectorOperations::copy(buffer.getWritePointer(channel), sineData.data(), samplesPerBlock);
            }
            processor->process(buffer);
        }
    }
};

//Test for TC_14
TEST_F(DeEsserProcessorTest, TC_14) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate, samplesPerBlock);

    processor->setThreshold(-30.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);
    processor->setFrequency(6000.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-40.0f), 0.001f);
}

//Test for TC_15
TEST_F(DeEsserProcessorTest, TC_15) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate, samplesPerBlock);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);
    processor->setFrequency(6000.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-18.0f), 0.05f);
}

//Test for TC_16
TEST_F(DeEsserProcessorTest, TC_16) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate, samplesPerBlock);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(10.0f);
    processor->setFrequency(6000.0f);

    simulateSineSignal(buffer, 500.0f, 500.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-12.0f), 0.005f);
}

//Test for TC_17
TEST_F(DeEsserProcessorTest, TC_17) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate, samplesPerBlock);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(50.0f);
    processor->setRelease(10.0f);
    processor->setFrequency(6000.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-40.0f));

    simulateSineSignal(buffer, 50.0f, 6000.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock),juce::Decibels::decibelsToGain(-18.0f),0.001f);
}

//Test for TC_18
TEST_F(DeEsserProcessorTest, TC_18) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate, samplesPerBlock);

    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setAttack(10.0f);
    processor->setRelease(50.0f);
    processor->setFrequency(6000.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-12.0f));

    simulateSineSignal(buffer, 50.0f, 6000.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock),juce::Decibels::decibelsToGain(-40.0f),0.001f);
}