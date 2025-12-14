#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/BandPassCompressorProcessor.h"
#include <cmath>
#include <iostream>

class BandPassCompressorProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<BandPassCompressorProcessor> processor;
    double sampleRate = 44100.0;
    int blockSize = 512;
    juce::AudioBuffer<float> buffer;
    double currentPhase = 0.0;

    void SetUp() override {
        processor = std::make_unique<BandPassCompressorProcessor>();
        processor->prepare(sampleRate, blockSize);
        buffer.setSize(1, blockSize);
        currentPhase = 0.0;
    }

    void fillSine(float freqHz, float levelDb) {
        auto* w = buffer.getWritePointer(0);
        float gain = juce::Decibels::decibelsToGain(levelDb);
        float phaseInc = 2.0f * juce::MathConstants<float>::pi * freqHz / (float)sampleRate;
        for (int i = 0; i < blockSize; ++i) {
            w[i] = gain * std::sin((float)currentPhase);
            currentPhase += phaseInc;
            if (currentPhase >= 2.0 * juce::MathConstants<float>::pi)
                currentPhase -= 2.0 * juce::MathConstants<float>::pi;
        }
    }
    
    void resetPhase() { currentPhase = 0.0; }

    void fillDC(float levelDb) {
        float gain = juce::Decibels::decibelsToGain(levelDb);
        juce::FloatVectorOperations::fill(buffer.getWritePointer(0), gain, blockSize);
    }
};

TEST_F(BandPassCompressorProcessorTest, LowEndPassThrough) {
    processor->setThreshold(0.0f);
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    resetPhase();
    fillDC(-6.0f);
    
    for (int i=0; i<10; ++i) processor->process(buffer);

    float rms = buffer.getRMSLevel(0, 0, blockSize);
    float target = juce::Decibels::decibelsToGain(-6.0f);
    std::cout << "LowEndPassThrough: RMS=" << rms << std::endl;
    EXPECT_NEAR(rms, target, 0.05f);
}

TEST_F(BandPassCompressorProcessorTest, MidBandSignalFlow) {
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    processor->setThreshold(0.0f); 
    resetPhase();
    
    // Process many blocks to be sure
    for (int i=0; i<50; ++i) {
        fillSine(600.0f, -10.0f);
        processor->process(buffer); 
    }

    float peak = buffer.getMagnitude(0, 0, blockSize);
    std::cout << "MidBandSignalFlow: Peak=" << peak << std::endl;

    // Should be near -10dB (0.316)
    EXPECT_GT(peak, 0.2f); 
}

TEST_F(BandPassCompressorProcessorTest, MidBandCompression) {
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    
    // 1. Uncompressed
    processor->setThreshold(0.0f); 
    processor->setRatio(4.0f);
    resetPhase();
    for (int i=0; i<50; ++i) {
        fillSine(600.0f, -10.0f);
        processor->process(buffer);
    }
    float peakUncompressed = buffer.getMagnitude(0, 0, blockSize);

    // 2. Compressed
    // Note: We need to reset processor state or allow release? 
    // Setting threshold lower should trigger attack.
    processor->setThreshold(-20.0f);
    // Continue phase
    for (int i=0; i<50; ++i) {
        fillSine(600.0f, -10.0f);
        processor->process(buffer);
    }
    float peakCompressed = buffer.getMagnitude(0, 0, blockSize);

    std::cout << "MidBandCompression: Un=" << peakUncompressed << " Comp=" << peakCompressed << std::endl;

    if (peakUncompressed > 0.1f) {
        EXPECT_LT(peakCompressed, peakUncompressed); 
    } else {
        // If uncompressed is low, we have a bigger problem (SignalFlow test should catch it)
        // But let's fail here too
        FAIL() << "Uncompressed signal too low: " << peakUncompressed;
    }
}

TEST_F(BandPassCompressorProcessorTest, LowBandNoCompression) {
    processor->setMinFrequency(2000.0f);
    processor->setMaxFrequency(20000.0f);
    
    // 1. Uncompressed
    processor->setThreshold(0.0f);
    processor->setRatio(10.0f);
    resetPhase();
    for (int i=0; i<30; ++i) {
        fillSine(100.0f, -10.0f);
        processor->process(buffer);
    }
    float peakUncompressed = buffer.getMagnitude(0, 0, blockSize);

    // 2. Compressed
    processor->setThreshold(-40.0f);
    for (int i=0; i<30; ++i) {
        fillSine(100.0f, -10.0f);
        processor->process(buffer);
    }
    float peakCompressed = buffer.getMagnitude(0, 0, blockSize);

    std::cout << "LowBandNoCompression: Un=" << peakUncompressed << " Comp=" << peakCompressed << std::endl;

    EXPECT_NEAR(peakCompressed, peakUncompressed, 0.05f);
}

// Test 5: NaN Check
TEST_F(BandPassCompressorProcessorTest, NoNaNs) {
    resetPhase();
    fillSine(1000.0f, 0.0f);
    processor->process(buffer);
    
    float val = buffer.getSample(0, 0);
    EXPECT_FALSE(std::isnan(val));
    EXPECT_FALSE(std::isinf(val));
}
