// Written by Austin Hills

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/SaturationProcessor.h"

class SaturationProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<SaturationProcessor> processor;

    void SetUp() override {
        processor = std::make_unique<SaturationProcessor>();
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

// Test Case 1: Unity Gain / Low Drive
// With Drive 0dB and low signal, output should be very close to input (linear region of tanh)
TEST_F(SaturationProcessorTest, UnityGainLowSignal) {
    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setDrive(0.0f);
    processor->setMix(1.0f);

    float inputLevel = juce::Decibels::decibelsToGain(-40.0f); // 0.01
    simulateConstantSignal(buffer, 50, inputLevel);

    // tanh(0.01) is approx 0.00999966, so very close to 0.01
    ASSERT_NEAR(buffer.getSample(0, 0), inputLevel, 0.0001f);
}

// Test Case 2: Saturation
// High Drive should cause clipping (compression)
TEST_F(SaturationProcessorTest, HighDriveSaturation) {
    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setDrive(20.0f); // +20dB gain (x10)
    processor->setMix(1.0f);

    // Input -20dB (0.1) -> Driven 1.0 -> tanh(1.0) = 0.76159
    float inputLevel = 0.1f;
    simulateConstantSignal(buffer, 50, inputLevel);

    // With Auto Gain Compensation:
    // Input = 0.5
    // Drive = 10.0 (+20dB) -> Gain = 10
    // Driven Signal = 0.5 * 10 = 5.0
    // Saturated (tanh(5.0)) ~= 0.9999
    // Compensated = 0.9999 / 10 = 0.09999
    
    // So the output should be much quieter than the raw input * gain,
    // essentially normalizing the volume.
    // We expect roughly 0.1f here.
    ASSERT_NEAR(buffer.getSample(0, 0), 0.1f, 0.05f);
}

// Test Case 3: Mix Control
// Even with high drive, if Mix is 0.0, output should match input
TEST_F(SaturationProcessorTest, MixBypass) {
    juce::AudioBuffer<float> buffer(1, 512);

    processor->prepare(44100);

    processor->setDrive(20.0f);
    processor->setMix(0.0f); // Fully Dry

    float inputLevel = 0.1f;
    simulateConstantSignal(buffer, 50, inputLevel);

    ASSERT_NEAR(buffer.getSample(0, 0), inputLevel, 0.001f);
}
