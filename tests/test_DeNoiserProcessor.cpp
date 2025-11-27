//Austin

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/DeNoiserProcessor.h"

class DeNoiserProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<DeNoiserProcessor> processor;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;

    //Keep track of phase across blocks
    double currentPhase = 0.0;

    void SetUp() override {
        processor = std::make_unique<DeNoiserProcessor>();
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

//Test for TC_19
TEST_F(DeNoiserProcessorTest, LearnAndReduce) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate);

    processor->setLearning(true);

    simulateSineSignal(buffer, 500.0f, 1000.0f, juce::Decibels::decibelsToGain(-40.0f));

    processor->setLearning(false);

    processor->setReduction(1.0f);

    simulateSineSignal(buffer, 500.0f, 1000.0f, juce::Decibels::decibelsToGain(-40.0f));

    float rms = buffer.getRMSLevel(0, 0, 512);
    ASSERT_LT(rms, 0.0001f);
}

//Test for TC_20
TEST_F(DeNoiserProcessorTest, ReLearnProfile) {
    juce::AudioBuffer<float> buffer(1, samplesPerBlock);

    processor->prepare(sampleRate);

    processor->setLearning(true);

    simulateSineSignal(buffer, 500.0f, 1000, juce::Decibels::decibelsToGain(-40.0f));

    processor->setLearning(false);

    simulateConstantSignal(buffer, 50.0f, 0.0f);

    processor->setLearning(true);

    simulateConstantSignal(buffer, 500.0f, 0.0f);

    processor->setLearning(false);

    processor->setReduction(1.0f);

    simulateSineSignal(buffer, 500.0f, 1000, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-40.0f), 0.001f);
}