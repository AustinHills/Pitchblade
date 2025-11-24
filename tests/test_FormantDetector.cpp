//huda
#include <gtest/gtest.h>
#include <JuceHeader.h>
#include <array>
#include <cmath>

#include "Pitchblade/effects/FormantDetector.h"

class FormantDetectorTest : public ::testing::Test {
protected:
    FormantDetector detector;
    double sampleRate = 48000.0;
    const int fftOrder = 11;
    const int fftSize = 1 << fftOrder;

    void SetUp() override
    {
        detector.prepare(sampleRate);
    }

    juce::AudioBuffer<float> makeSineBuffer(float amplitude, double frequency)
    {
        juce::AudioBuffer<float> buffer(1, fftSize);
        double phase = 0.0;
        const double increment = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int i = 0; i < fftSize; ++i)
        {
            buffer.setSample(0, i, static_cast<float>(std::sin(phase) * amplitude));
            phase += increment;
        }

        return buffer;
    }
};

TEST_F(FormantDetectorTest, TC_46_PrepareClearsExistingFormants)
{
    auto buffer = makeSineBuffer(0.6f, 1000.0);
    detector.processBlock(buffer);
    ASSERT_FALSE(detector.getFormants().empty());

    detector.prepare(sampleRate);
    EXPECT_TRUE(detector.getFormants().empty());
}

TEST_F(FormantDetectorTest, TC_47_EmptyBufferClearsFormantsSafely)
{
    auto buffer = makeSineBuffer(0.6f, 900.0);
    detector.processBlock(buffer);
    ASSERT_FALSE(detector.getFormants().empty());

    juce::AudioBuffer<float> empty(1, 0);
    detector.processBlock(empty);

    EXPECT_TRUE(detector.getFormants().empty());
}

TEST_F(FormantDetectorTest, TC_48_LowLevelAudioProducesNoFormants)
{
    juce::AudioBuffer<float> low(1, fftSize);
    juce::FloatVectorOperations::fill(low.getWritePointer(0), 1e-5f, fftSize);

    detector.processBlock(low);

    EXPECT_TRUE(detector.getFormants().empty());
    EXPECT_TRUE(detector.getFormantFrequencies().empty());
}

TEST_F(FormantDetectorTest, TC_49_SingleToneDetectsAFormant)
{
    auto buffer = makeSineBuffer(0.8f, 1000.0);
    detector.processBlock(buffer);

    auto frequencies = detector.getFormantFrequencies();
    ASSERT_FALSE(frequencies.empty());

    const float nearest = frequencies.front();
    EXPECT_NEAR(nearest, 1000.0f, 60.0f);
}

TEST_F(FormantDetectorTest, TC_50_FormantCountLimitedForMultiToneSignal)
{
    juce::AudioBuffer<float> buffer(1, fftSize);
    juce::FloatVectorOperations::clear(buffer.getWritePointer(0), fftSize);

    const std::array<double, 5> freqs { 400.0, 900.0, 1500.0, 2500.0, 3500.0 };
    for (double freq : freqs)
    {
        auto tone = makeSineBuffer(0.5f / static_cast<float>(freqs.size()), freq);
        for (int i = 0; i < fftSize; ++i)
            buffer.addSample(0, i, tone.getSample(0, i));
    }

    detector.processBlock(buffer);
    auto formants = detector.getFormants();

    EXPECT_FALSE(formants.empty());
    EXPECT_LE(formants.size(), 3u);
}
