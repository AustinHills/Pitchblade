/**
 * Author: Hayley Spellicy-Ryan
 */

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/PitchCorrector.h"
#include <vector>

#define PI 3.141592653589793238


// Test setup; use TEST_F for tests that require auxilary classes
class PitchCorrectorIntegrationTest : public ::testing::Test{
protected:
    void SetUp() override{
        thisSampleRate = 44100.f;
        blockSize = 1024;
        detector = std::make_unique<PitchDetector>();
        shifter = std::make_unique<PitchShifter>();
        corrector = std::make_unique<PitchCorrector>(*detector, *shifter);
        corrector->prepare(thisSampleRate, blockSize);
    }
    
    //from austin: Helper to calculate blocks needed for a duration in ms
    int blocksForMS(float ms){
        return (int)(std::ceil((ms/1000) * 44100 / (double)512));
    }

    //from hayley: Helper to construct individual frame of sine wave
    std::vector<float> makeSineFrame(float frequency, int bufferSize, double thisSampleRate){
        std::vector<float> sineFrame(bufferSize);
        for(int i = 0; i < bufferSize; ++i){
            double timeIndex = double(i) / thisSampleRate;
            sineFrame[i] = float(std::sin(2.0 * PI * frequency * timeIndex));
        }
        return sineFrame;
    }

    //from austin: Helper to simulate a sine wave
    void simulateSineSignal(juce::AudioBuffer<float>& buffer, float msDuration, float frequency, float amplitude){
        int numBlocks = blocksForMS(msDuration);

        for(int i = 0; i < numBlocks; i++){

            auto sineData = makeSineFrame(frequency, blockSize, thisSampleRate);
        
            juce::FloatVectorOperations::multiply(sineData.data(), amplitude, blockSize);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel){
                juce::FloatVectorOperations::copy(buffer.getWritePointer(channel), sineData.data(), blockSize);
            }
            corrector->processBlock(buffer);
        }
    }

    double thisSampleRate;
    int blockSize;
    std::unique_ptr<PitchCorrector> corrector;
    std::unique_ptr<PitchDetector> detector;
    std::unique_ptr<PitchShifter> shifter;
};


// Integration Tests--------------------------------------------------
TEST_F(PitchCorrectorIntegrationTest, DetectSemitoneErrorPos)
{
    // --- 1. ARRANGE ---
    const float frequency = 450.f; // sharp of A4
    juce::AudioBuffer<float> buffer(1, blockSize);
    // --- 2. ACT ---
    simulateSineSignal(buffer, 500.f, frequency, juce::Decibels::decibelsToGain(-40.0f));
    // --- 3. ASSERT ---
    ASSERT_GT(detector->getCurrentPitch(), 440.f);
}

TEST_F(PitchCorrectorIntegrationTest, DetectSemitoneErrorNeg)
{
    // --- 1. ARRANGE ---
    const float frequency = 400.f; // flat of A4
    juce::AudioBuffer<float> buffer(1, blockSize);
    // --- 2. ACT ---
    simulateSineSignal(buffer, 500.f, frequency, juce::Decibels::decibelsToGain(-40.0f));
    // --- 3. ASSERT ---
    ASSERT_LT(detector->getCurrentPitch(), 440.f);
}

TEST_F(PitchCorrectorIntegrationTest, BypassZeroPitch)
{
    // --- 1. ARRANGE ---
    const float frequency = 0.f; // empty
    juce::AudioBuffer<float> buffer(1, blockSize);
    // --- 2. ACT ---
    simulateSineSignal(buffer, 500.f, frequency, juce::Decibels::decibelsToGain(-40.0f));
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(shifter->getPitchShiftRatio(), 1.f); // on bypass, ratio is set to 1
}

TEST_F(PitchCorrectorIntegrationTest, StablePitchCallsCorrection)
{
    // --- 1. ARRANGE ---
    const float frequency = 460.f; // sharp of A4
    const float permitted_error = 0.01f; //correction should be below 99%
    juce::AudioBuffer<float> buffer(1, blockSize);
    // --- 2. ACT ---
    for(unsigned int i = 0; i < 30; ++i)
        simulateSineSignal(buffer, 500.f, frequency, juce::Decibels::decibelsToGain(-40.0f));
    // --- 3. ASSERT ---
    EXPECT_TRUE(std::abs(shifter->getPitchShiftRatio() - 1.f) > permitted_error); //on sharp note, correction should be not bypassed
}