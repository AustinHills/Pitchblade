#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "Pitchblade/effects/AutoGainProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>

TEST(AutoGainProcessor, BelowThresholdDoesNotApplyGain) {
    AutoGainProcessor processor;
    processor.prepare(44100.0);
    processor.setThreshold(-30.0f);
    processor.setTargetThroughput(-6.0f);
    
    // Input signal at -40dB (below -30dB threshold)
    juce::AudioBuffer<float> buffer(1, 100);
    float inputLevel = juce::Decibels::decibelsToGain(-40.0f);
    for(int i=0; i<100; ++i) buffer.setSample(0, i, inputLevel);
    
    // Process
    processor.process(buffer);
    
    // Expect output to be close to input (Gain ~ 1.0)
    // There might be some release smoothing if defaults were different, but it starts at 1.0
    float outputLevel = buffer.getRMSLevel(0, 0, 100);
    float outputDB = juce::Decibels::gainToDecibels(outputLevel);
    
    EXPECT_NEAR(outputDB, -40.0f, 0.1f);
}

TEST(AutoGainProcessor, AboveThresholdAppliesGain) {
    AutoGainProcessor processor;
    processor.prepare(44100.0);
    processor.setThreshold(-30.0f);
    processor.setTargetThroughput(-6.0f); // Target output
    processor.setAttack(1.0f); // Fast attack for test
    processor.setRelease(100.0f);
    
    // Input signal at -10dB (Above -30dB). 
    // Needed gain = -6dB - (-10dB) = +4dB.
    // However, the logic implemented: if input > threshold, target = desiredOutput.
    
    juce::AudioBuffer<float> buffer(1, 44100); // 1 second buffer
    float inputLevel = juce::Decibels::decibelsToGain(-10.0f);
    for(int i=0; i<44100; ++i) buffer.setSample(0, i, inputLevel);
    
    // Process
    processor.process(buffer);
    
    // Check end of buffer (should have settled)
    // The last sample should be close to -6dB
    float lastSample = std::abs(buffer.getSample(0, 44099));
    float lastSampleDB = juce::Decibels::gainToDecibels(lastSample);
    
    EXPECT_NEAR(lastSampleDB, -6.0f, 0.5f);
}
