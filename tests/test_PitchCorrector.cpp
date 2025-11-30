/**
 * Author: Hayley Spellicy-Ryan
 */

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/effects/PitchCorrector.h"
#include "mocks.cpp"
#include <vector>

#define PI 3.141592653589793238


// Test setup; use TEST_F for tests that require auxilary classes
class PitchCorrectorTest : public ::testing::Test{
protected:
    void SetUp() override{
        thisSampleRate = 44100.f;
        blockSize = 1024;
        detector = std::make_unique<PitchDetector>();
        shifter = std::make_unique<PitchShifter>();
        corrector = std::make_unique<PitchCorrector>(*detector, *shifter);
        corrector->prepare(thisSampleRate, blockSize);
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

    double thisSampleRate;
    int blockSize;
    std::unique_ptr<PitchCorrector> corrector;
    std::unique_ptr<PitchDetector> detector;
    std::unique_ptr<PitchShifter> shifter;
};


// Unit Tests---------------------------------------------------------
TEST(PitchCorrectorTest, SetRetuneClamps)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setRetuneSpeed(1.1f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getRetuneSpeed(), 1.0f); // Does it clamp positive?
    // --- 2. ACT ---
    corrector.setRetuneSpeed(-0.1f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getRetuneSpeed(), 0.0f); // Does it clamp negative?
    // --- 2. ACT ---
    corrector.setRetuneSpeed(0.5f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getRetuneSpeed(), 0.5f); // Does it assign normal value?
}
TEST(PitchCorrectorTest, SetNoteTransitionClamps)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setNoteTransition(51.f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getNoteTransition(), 50.f); // Does it clamp positive?
    // --- 2. ACT ---
    corrector.setNoteTransition(-0.1f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getNoteTransition(), 0.0f); // Does it clamp negative?
    // --- 2. ACT ---
    corrector.setNoteTransition(20.f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getNoteTransition(), 20.f); // Does it assign normal value?
}
TEST(PitchCorrectorTest, SetCorrectionRatioClamps)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setCorrectionRatio(1.1f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getCorrectionRatio(), 1.0f); // Does it clamp positive?
    // --- 2. ACT ---
    corrector.setCorrectionRatio(0.0f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getCorrectionRatio(), 0.001f); // Does it clamp negative?
    // --- 2. ACT ---
    corrector.setCorrectionRatio(0.5f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getCorrectionRatio(), 0.5f); // Does it assign normal value?
}
TEST(PitchCorrectorTest, SetWaverClamps)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setWaver(21.f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getWaver(), 20.0f); // Does it clamp positive?
    // --- 2. ACT ---
    corrector.setWaver(-0.1f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getWaver(), 0.0f); // Does it clamp negative?
    // --- 2. ACT ---
    corrector.setWaver(10.f);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getWaver(), 10.f); // Does it assign normal value?
}
TEST(PitchCorrectorTest, SetScaleOffset)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setScaleOffset(-13);
    // --- 3. ASSERT ---
    ASSERT_EQ(corrector.getScaleOffset(), -12); // Does it clamp positive?
    // --- 2. ACT ---
    corrector.setScaleOffset(1);
    // --- 3. ASSERT ---
    ASSERT_EQ(corrector.getScaleOffset(), 0); // Does it clamp negative?
    // --- 2. ACT ---
    corrector.setScaleOffset(-6);
    // --- 3. ASSERT ---
    ASSERT_EQ(corrector.getScaleOffset(), -6); // Does it assign normal value?
}
TEST(PitchCorrectorTest, SetScaleType)
{
    // --- 1. ARRANGE ---
    MockPitchDetector detector;
    MockPitchShifter shifter;
    PitchCorrector corrector(detector, shifter);
    // --- 2. ACT ---
    corrector.setScaleType(scaleType::Major);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getScaleType(), 0); // Does it assign major?
    // --- 2. ACT ---
    corrector.setScaleType(scaleType::Minor);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getScaleType(), 1); // Does it assign minor?
    // --- 2. ACT ---
    corrector.setScaleType(-6);
    // --- 3. ASSERT ---
    ASSERT_FLOAT_EQ(corrector.getScaleType(), 1); // Does it default minor?
}