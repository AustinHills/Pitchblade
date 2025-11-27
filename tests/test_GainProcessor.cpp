#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/GainProcessor.h"

TEST(GainProcessorTest, ApplyPositiveGain) 
{
    // --- 1. ARRANGE ---
    GainProcessor processor;
    juce::AudioBuffer<float> buffer(1, 512); // 1 channel, 512 samples

    // Set gain to +6.02dB (which is 2.0x linear gain)
    processor.setGain(juce::Decibels::gainToDecibels(2.0f));

    // Set buffer to a known value
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.5f, buffer.getNumSamples());
    
    processor.process(buffer);

    // --- 3. ASSERT ---
    // Check if the gain was applied
    // 0.5 (input) * 2.0 (gain) = 1.0 (output)
    ASSERT_FLOAT_EQ(buffer.getSample(0, 100), 1.0f);
    ASSERT_FLOAT_EQ(buffer.getSample(0, 200), 1.0f);
}

TEST(GainProcessorTest, ApplyNoGain) 
{
    // --- ARRANGE ---
    GainProcessor processor;
    juce::AudioBuffer<float> buffer(1, 512);
    
    // Set gain to 0.0dB (which is 1.0x linear gain)
    processor.setGain(0.0f); 
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.5f, buffer.getNumSamples());
    
    // --- ACT ---
    processor.process(buffer);

    // --- ASSERT ---
    // 0.5 (input) * 1.0 (gain) = 0.5 (output)
    ASSERT_FLOAT_EQ(buffer.getSample(0, 100), 0.5f);
}

TEST(GainProcessorTest, ApplyNegativeGain) 
{
    // --- ARRANGE ---
    GainProcessor processor;
    juce::AudioBuffer<float> buffer(1, 512);
    
    // Set gain to -6.02 dB (which is 0.5x linear gain)
    processor.setGain(juce::Decibels::gainToDecibels(0.5f));
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.5f, buffer.getNumSamples());
    
    // --- ACT ---
    processor.process(buffer);

    // --- ASSERT ---
    // 0.5 (input) * 0.5 (gain) = 0.25 (output)
    ASSERT_FLOAT_EQ(buffer.getSample(0, 100), 0.25f);
}

TEST(GainProcessorTest, ApplyNegativeInfinityGain) 
{
    // --- ARRANGE ---
    GainProcessor processor;
    juce::AudioBuffer<float> buffer(1, 512);
    
    // Set gain to -inf dB (which is 0.0x linear gain)
    processor.setGain(juce::Decibels::gainToDecibels(0.0f));
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), 0.5f, buffer.getNumSamples());
    
    // --- ACT ---
    processor.process(buffer);

    // --- ASSERT ---
    // 0.5 (input) * 0.0 (gain) = 0.0 (output)
    ASSERT_FLOAT_EQ(buffer.getSample(0, 100), 0.0f);
}