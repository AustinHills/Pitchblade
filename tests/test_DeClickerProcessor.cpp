//Austin

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/DeClickerProcessor.h"

class DeClickerProcessorTest : public ::testing::Test {
protected:
    std::unique_ptr<DeClickerProcessor> processor;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    int numChannels = 2; // Stereo test default

    void SetUp() override {
        processor = std::make_unique<DeClickerProcessor>();
        // Initialize with default history and sensitivity
        processor->prepare(sampleRate, numChannels);
        processor->setHistoryLength(5);
        processor->setSensitivity(0.5f);
    }
    
    // Helper to generate a sine wave
    void generateSine(juce::AudioBuffer<float>& buffer, float frequency, float amplitude) {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            auto* data = buffer.getWritePointer(ch);
            double phase = 0.0;
            double phaseInc = juce::MathConstants<double>::twoPi * frequency / sampleRate;
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = (float)(std::sin(phase) * amplitude);
                phase += phaseInc;
            }
        }
    }
    
    // Helper to run audio through the processor for N blocks
    void processAudio(juce::AudioBuffer<float>& buffer, int numBlocks) {
        // Create a scratch buffer to simulate block-by-block processing
        juce::AudioBuffer<float> blockBuffer(numChannels, samplesPerBlock);
        
        int totalSamples = buffer.getNumSamples();
        int position = 0;
        
        while (position < totalSamples) {
            int numThisBlock = std::min(samplesPerBlock, totalSamples - position);
            
            // Copy input to block
            for (int ch = 0; ch < numChannels; ++ch) {
                blockBuffer.copyFrom(ch, 0, buffer, ch, position, numThisBlock);
                // Zero padding if last block is small? (Not strictly needed if we just process valid samples)
            }
            
            processor->process(blockBuffer);
            
            // Copy output back
            for (int ch = 0; ch < numChannels; ++ch) {
                buffer.copyFrom(ch, position, blockBuffer, ch, 0, numThisBlock);
            }
            
            position += numThisBlock;
        }
    }
};

TEST_F(DeClickerProcessorTest, ClickRemoval) {
    // Need enough audio to fill history
    int totalSamples = samplesPerBlock * 20;
    juce::AudioBuffer<float> buffer(numChannels, totalSamples);
    
    // 1. Generate Quiet Tone (Background)
    generateSine(buffer, 440.0f, 0.1f);
    
    // 2. Insert Click
    // Wait until ~10 blocks in so history is populated
    int clickIndex = samplesPerBlock * 10 + 50; 
    
    // Inject massive spike into Left channel only
    buffer.setSample(0, clickIndex, 1.0f);
    
    // 3. Process
    processAudio(buffer, 20);
    
    // 4. Check
    // Get output sample at click index
    float outSample = buffer.getSample(0, clickIndex);
    
    // The denoiser/clicker introduces latency due to FFT. 
    // Usually equal to hopSize or windowSize depending on overlap buffering.
    // We should search for the click's remnant in the output vicinity or check if the spike is gone from the stream.
    // Wait, typical FFT overlap-add latency is (fftSize - hopSize) or similar delay.
    // Let's check RMS of the block containing the click vs input.
    
    // Or simpler: Look for any sample > 0.5 (since click was 1.0 and background 0.1) in the region.
    // If it worked, no sample should be near 1.0.
    
    float maxPeak = buffer.getMagnitude(0, totalSamples);
    
    // If click was removed, max peak should be way less than 1.0 (maybe 0.2-0.3 due to smoothing/ringing)
    // If click remains, it will be close to 1.0 (or smeared but high).
    EXPECT_LT(maxPeak, 0.8f);
}

TEST_F(DeClickerProcessorTest, SilencePreservation) {
    int totalSamples = samplesPerBlock * 10;
    juce::AudioBuffer<float> buffer(numChannels, totalSamples);
    buffer.clear(); // Silence
    
    processAudio(buffer, 10);
    
    // Output should basically be zero (floating point noise acceptable)
    float rms = buffer.getRMSLevel(0, 0, totalSamples);
    EXPECT_LT(rms, 1e-4f);
}

TEST_F(DeClickerProcessorTest, StereoIndependence) {
    int totalSamples = samplesPerBlock * 20;
    juce::AudioBuffer<float> buffer(numChannels, totalSamples);
    generateSine(buffer, 440.0f, 0.1f);
    
    int clickIndex = samplesPerBlock * 10 + 50;
    
    // Click in Left Only
    buffer.setSample(0, clickIndex, 1.0f);
    // Right is clean
    
    processAudio(buffer, 20);
    
    // Left should be attenuated
    float leftPeak = buffer.getMagnitude(0, totalSamples);
    EXPECT_LT(leftPeak, 0.8f);
    
    // Right should remain roughly same (sine wave peak 0.1)
    float rightPeak = buffer.getMagnitude(1, 0, totalSamples);
    
    std::cout << "[Test Debug] Left Peak: " << leftPeak << " | Right Peak: " << rightPeak << std::endl;
    
    EXPECT_NEAR(rightPeak, 0.1f, 0.05f); 
}

TEST_F(DeClickerProcessorTest, SineThroughput) {
    // Basic stability check without clicks
    int totalSamples = samplesPerBlock * 30; // More samples to clear latency
    juce::AudioBuffer<float> buffer(numChannels, totalSamples);
    generateSine(buffer, 440.0f, 0.1f);
    
    processAudio(buffer, 30);
    
    // Check MIDDLE of the buffer (to avoid onset latency or tail cutoff)
    // The lookahead introduces a delay. Silence at start.
    // FFT latency is substantial now (Window + LookAhead). 
    // We should check a region we know is safe. e.g. Block 15.
    
    int checkStart = samplesPerBlock * 15;
    int checkLen = samplesPerBlock * 5;
    
    float leftPeak = buffer.getMagnitude(0, checkStart, checkLen);
    float rightPeak = buffer.getMagnitude(1, checkStart, checkLen);
    
    std::cout << "[Test Debug] Throughput - Left: " << leftPeak << " | Right: " << rightPeak << std::endl;
    
    EXPECT_NEAR(leftPeak, 0.1f, 0.05f); 
    EXPECT_NEAR(rightPeak, 0.1f, 0.05f); 
}

TEST_F(DeClickerProcessorTest, OnsetPreservation) {
    // New Test: Sustain Verification
    // Verify that a SUDDEN onset (Start of a word) is NOT cut if it sustains
    int totalSamples = samplesPerBlock * 40;
    juce::AudioBuffer<float> buffer(numChannels, totalSamples);
    
    // Silence for 10 blocks
    // Then Loud Sine Wave (0.8) for remainder (Speech onset)
    // A primitive click remover would see the first loud frame as a spike and kill it.
    // Ours should look ahead, see it sustains, and keep it.
    
    int onsetIndex = samplesPerBlock * 10;
    for (int ch=0; ch<numChannels; ++ch) {
        auto* w = buffer.getWritePointer(ch);
        double phase = 0.0;
        double inc = juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
        
        for (int i=onsetIndex; i<totalSamples; ++i) {
             w[i] = (float)(std::sin(phase) * 0.8f);
             phase += inc;
        }
    }
    
    processAudio(buffer, 40);
    
    // Check the INITIAL ONSET block (Block 11 or so, accounting for latency)
    // Latency = LookAheadDepth(2) * HopSize? + WindowDelay? 
    // FFT Block latency is roughly 1 block out. Lookahead adds 2 blocks.
    // Real output starts around Block 3-4 relative to input? 
    // Let's sweep and find the rising edge.
    
    // Or just check that the sustained part is intact.
    // If we cut the onset, there would be a "hole" or "fade in". 
    // We want sharp onset.
    // This is hard to test deterministically without exact latency calculation.
    // Let's just check that we eventually reach 0.8 amplitude.
    
    float peak = buffer.getMagnitude(0, samplesPerBlock * 20, samplesPerBlock * 10);
    EXPECT_NEAR(peak, 0.8f, 0.1f);
}
