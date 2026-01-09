#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/BandPassCompressorProcessor.h"
#include <cmath>
#include <iostream>
#include <vector>

// Helper class for testing
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
        buffer.clear();
        currentPhase = 0.0;
        
        // Ensure default frequencies are reasonable
        processor->setMinFrequency(200.0f);
        processor->setMaxFrequency(2000.0f);
        processor->setAttack(10.0f);
        processor->setRelease(100.0f);
        processor->setRatio(4.0f);
        processor->setThreshold(0.0f); // Default no comp
    }
    
    // Fill buffer with continuous phase sine
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
    
    // Fill buffer with random noise for spectrum checks
    void fillNoise(float levelDb) {
        auto* w = buffer.getWritePointer(0);
        float gain = juce::Decibels::decibelsToGain(levelDb);
        juce::Random rng;
        for (int i=0; i<blockSize; ++i) {
            w[i] = (rng.nextFloat() * 2.0f - 1.0f) * gain;
        }
    }

    void resetPhase() { currentPhase = 0.0; }

    // Helper to settle filters and get peak
    // Must refill buffer every block to simulate stream!
    float getPeakAfterProcessing(float freqHz, float levelDb, int numBlocks = 10) {
        float peak = 0.0f;
        for (int i=0; i<numBlocks; ++i) {
            fillSine(freqHz, levelDb);
            processor->process(buffer);
            peak = buffer.getMagnitude(0, 0, blockSize);
        }
        return peak;
    }
};

// 1. Passthrough Flatness
// Ensure that with Threshold=0, the sum of bands recreates the input signal reasonably well.
TEST_F(BandPassCompressorProcessorTest, PassthroughFlatness) {
    processor->setThreshold(0.0f); 
    // Test 3 frequencies: Low, Mid, High
    float freqs[] = { 100.0f, 600.0f, 5000.0f };
    float inputDb = -10.0f;
    float expectedGain = juce::Decibels::decibelsToGain(inputDb);

    for (float f : freqs) {
        resetPhase();
        // fillSine(f, inputDb); // Removed manual fill
        float peak = getPeakAfterProcessing(f, inputDb, 50);
        
        // Expect pass-through gain ~ 1.0 (Output Level = Input Level)
        // Note: Linkwitz-Riley sums perfectly flat. Butterworth pairs sum with small bumps/dips (+3dB at crossover).
        // Since we are checking frequencies FAR from crossover (200, 2000), ideally they should be flat.
        // 100Hz (Low) < 200Hz.
        // 600Hz (Mid) > 200, < 2000.
        // 5000Hz (High) > 2000.
        
        std::cout << "Flatness Freq=" << f << " Peak=" << peak << " Expected=" << expectedGain << std::endl;
        EXPECT_NEAR(peak, expectedGain, expectedGain * 0.1f) << "Failed at freq " << f;
    }
}

// 2. Low Band Isolation
// Input Low Freq. Ensure it passes.
TEST_F(BandPassCompressorProcessorTest, LowBandIsolation) {
    processor->setMinFrequency(500.0f);
    float peak = getPeakAfterProcessing(100.0f, -6.0f, 30);
    EXPECT_NEAR(peak, juce::Decibels::decibelsToGain(-6.0f), 0.05f);
}

// 3. Mid Band Isolation
TEST_F(BandPassCompressorProcessorTest, MidBandIsolation) {
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    float peak = getPeakAfterProcessing(600.0f, -6.0f, 30);
    EXPECT_NEAR(peak, juce::Decibels::decibelsToGain(-6.0f), 0.05f);
}

// 4. High Band Isolation
TEST_F(BandPassCompressorProcessorTest, HighBandIsolation) {
    processor->setMaxFrequency(2000.0f);
    float peak = getPeakAfterProcessing(5000.0f, -6.0f, 30);
    EXPECT_NEAR(peak, juce::Decibels::decibelsToGain(-6.0f), 0.05f);
}

// 5. Compression Action (Mid Band)
TEST_F(BandPassCompressorProcessorTest, CompressionAction) {
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    processor->setRatio(4.0f);

    float inputDb = -10.0f;
    // Uncompressed (Thresh 0)
    processor->setThreshold(0.0f);
    float peakUn = getPeakAfterProcessing(600.0f, inputDb, 20);

    // Compressed (Thresh -20)
    resetPhase(); 
    processor->setThreshold(-20.0f);
    float peakComp = getPeakAfterProcessing(600.0f, inputDb, 50); 

    std::cout << "CompAction: Un=" << peakUn << " Comp=" << peakComp << std::endl;
    EXPECT_LT(peakComp, peakUn);
}

// 6. Compression Ratio Check
TEST_F(BandPassCompressorProcessorTest, CompressionRatioCheck) {
    // Ideally, for Input= -10, Thresh= -20, Ratio= 2:1
    // Over = 10dB. Red = 5dB. Output = -15dB.
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    processor->setRatio(2.0f);
    processor->setThreshold(-20.0f);
    processor->setThreshold(-20.0f);
    processor->setAttack(1.0f); // Fast attack for measurement

    float peak = getPeakAfterProcessing(600.0f, -10.0f, 100);
    float gain = juce::Decibels::decibelsToGain(-15.0f); // Target

    std::cout << "RatioCheck: Peak=" << peak << " Target=" << gain << std::endl;
    // Allow slight tolerance for knee/detector behavior
    EXPECT_NEAR(peak, gain, 0.05f);
}

// 7. Listen Mode
TEST_F(BandPassCompressorProcessorTest, ListenMode) {
    processor->setListen(true);
    processor->setMinFrequency(500.0f);

    // Input LOW freq (should be silent in Listen Mode because Listen = Mid Only)
    float peakLow = getPeakAfterProcessing(100.0f, -10.0f, 20);
    EXPECT_LT(peakLow, 0.01f); // Silence

    // Input MID freq (should pass)
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    float peakMid = getPeakAfterProcessing(600.0f, -10.0f, 20);
    EXPECT_GT(peakMid, 0.1f);
}

// 8. Parameter Updates (Freq Shift)
TEST_F(BandPassCompressorProcessorTest, ParameterUpdates) {
    // Start with 600Hz in Mid Band (Min 200)
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);
    float peak1 = getPeakAfterProcessing(600.0f, -10.0f, 20);
    EXPECT_GT(peak1, 0.1f);

    // Move Min Freq to 1000Hz (600Hz is now Low Band)
    // In Listen Mode, this should become silent?
    processor->setListen(true);
    processor->setMinFrequency(1000.0f);
    // Need to re-process to flush filters?
    float peak2 = getPeakAfterProcessing(600.0f, -10.0f, 50);
    EXPECT_LT(peak2, 0.05f); // Should be rejected by Mid Band HP(1000)
}

// 9. Silence Handling
TEST_F(BandPassCompressorProcessorTest, SilenceHandling) {
    buffer.clear(); // 0.0
    processor->process(buffer);
    float peak = buffer.getMagnitude(0, 0, blockSize);
    EXPECT_EQ(peak, 0.0f);
}

// 10. Stereo Independence (Basic Check)
// Note: our processor sums envelopes or links them?
// Implementation: "envelopeInput = max(absMid)" across channels. Stereo linked detection.
// "processedMid = midSample[ch] * gain".
// If CH1 is silent, CH2 is Loud. CH1 Mid Gain should be reduced?
// Yes, linked compression.
TEST_F(BandPassCompressorProcessorTest, StereoLinking) {
    buffer.setSize(2, blockSize);
    buffer.clear();
    
    // Set Thresh -20.
    processor->setThreshold(-20.0f);
    processor->setRatio(4.0f);
    processor->setMinFrequency(200.0f);
    processor->setMaxFrequency(2000.0f);

    // CH0: Silence. CH1: Loud Mid (-10dB).
    auto* w0 = buffer.getWritePointer(0);
    auto* w1 = buffer.getWritePointer(1);
    float gain = juce::Decibels::decibelsToGain(-10.0f);
    float midFreq = 600.0f;
    float phaseInc = 2.0f * juce::MathConstants<float>::pi * midFreq / 44100.0f;
    
    // Process loop with refill
    for(int i=0; i<50; ++i) {
        // Refill to simulate stream
        auto* w0 = buffer.getWritePointer(0);
        auto* w1 = buffer.getWritePointer(1);
        for (int s=0; s<blockSize; ++s) {
            w0[s] = 0.0f;
            w1[s] = gain * std::sin((float)s * phaseInc); // Phase reset irrelevant for this check
        }
        processor->process(buffer);
    }
    
    // Check CH1 gain reduction.
    float peak1 = buffer.getMagnitude(1, 0, blockSize);
    float expected = juce::Decibels::decibelsToGain(-17.5f); // -7.5dB reduction
    EXPECT_NEAR(peak1, expected, 0.05f);
    
    // Check CH0 should still be 0
    float peak0 = buffer.getMagnitude(0, 0, blockSize);
    EXPECT_EQ(peak0, 0.0f);
}

// 11. Sample Rate Change
TEST_F(BandPassCompressorProcessorTest, SampleRateChange) {
    // Simulate Host changing SR
    processor->prepare(48000.0, 512); // Change to 48k
    // processor->updateFilters() is called in prepare.
    
    sampleRate = 48000.0; // Update test helper SR
    
    float peak = getPeakAfterProcessing(1000.0f, -10.0f, 20);
    EXPECT_NEAR(peak, juce::Decibels::decibelsToGain(-10.0f), 0.05f);
}

// 12. Extreme Values
TEST_F(BandPassCompressorProcessorTest, ExtremeFrequencies) {
    // Min=20, Max=20000. Essentially Full Band.
    processor->setMinFrequency(20.0f);
    processor->setMaxFrequency(20000.0f);
    
    float peak = getPeakAfterProcessing(1000.0f, -10.0f, 20);
    
    // Should pass perfectly (Mid Band covers everything)
    EXPECT_NEAR(peak, juce::Decibels::decibelsToGain(-10.0f), 0.05f);
}
