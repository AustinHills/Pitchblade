//huda
#include <gtest/gtest.h>
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

#include "Pitchblade/effects/Equalizer.h"

class EqualizerTest : public ::testing::Test {
protected:
    Equalizer eq;
    double sampleRate = 48000.0;
    int blockSize = 256;
    int numChannels = 2;

    void SetUp() override
    {
        eq.prepare(sampleRate, blockSize, numChannels);
    }

    juce::AudioBuffer<float> makeSineBuffer(float amplitude, double frequency)
    {
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        double phase = 0.0;
        const double increment = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int i = 0; i < blockSize; ++i)
        {
            float sample = static_cast<float>(std::sin(phase) * amplitude);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, sample);
            phase += increment;
        }

        return buffer;
    }

    float rms(const juce::AudioBuffer<float>& buffer)
    {
        return buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    }

    juce::AudioBuffer<float> processRepeated(float shiftGainDbSetterValue,
                                             void (Equalizer::*gainSetter)(float),
                                             double frequency,
                                             int blocks)
    {
        juce::AudioBuffer<float> buffer = makeSineBuffer(0.25f, frequency);
        for (int i = 0; i < blocks; ++i)
        {
            if (i == 0 && gainSetter != nullptr)
                (eq.*gainSetter)(shiftGainDbSetterValue);

            juce::AudioBuffer<float> temp(buffer);
            eq.processBlock(temp);
            buffer.makeCopyOf(temp);
        }
        return buffer;
    }
};

TEST_F(EqualizerTest, TC_65_PrepareAndProcessRunSafely)
{
    juce::AudioBuffer<float> buffer = makeSineBuffer(0.3f, 440.0);
    EXPECT_NO_THROW(eq.processBlock(buffer));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            ASSERT_TRUE(std::isfinite(buffer.getSample(ch, i)));
}

TEST_F(EqualizerTest, TC_66_LowShelfBoostsAtConfiguredFrequency)
{
    eq.setLowFreq(100.0f);
    eq.setLowGainDb(6.0f);
    eq.setMidGainDb(0.0f);
    eq.setHighGainDb(0.0f);

    auto input = makeSineBuffer(0.25f, 100.0);
    const float inLevel = rms(input);

    juce::AudioBuffer<float> output(input);
    const int blocksToSettle = 8;
    for (int i = 0; i < blocksToSettle; ++i)
    {
        juce::AudioBuffer<float> temp(input);
        eq.processBlock(temp);
        output.makeCopyOf(temp);
    }

    EXPECT_NEAR(eq.getLowFreq(), 100.0f, 1e-3f);
    EXPECT_GT(rms(output), inLevel);
}

TEST_F(EqualizerTest, TC_67_MidPeakBoostsAtConfiguredFrequency)
{
    eq.setMidFreq(1000.0f);
    eq.setMidGainDb(6.0f);
    eq.setLowGainDb(0.0f);
    eq.setHighGainDb(0.0f);

    auto input = makeSineBuffer(0.25f, 1000.0);
    const float inLevel = rms(input);

    juce::AudioBuffer<float> output(input);
    const int blocksToSettle = 8;
    for (int i = 0; i < blocksToSettle; ++i)
    {
        juce::AudioBuffer<float> temp(input);
        eq.processBlock(temp);
        output.makeCopyOf(temp);
    }

    EXPECT_NEAR(eq.getMidFreq(), 1000.0f, 1e-3f);
    EXPECT_GT(rms(output), inLevel);
}

TEST_F(EqualizerTest, TC_68_HighShelfBoostsAtConfiguredFrequency)
{
    eq.setHighFreq(8000.0f);
    eq.setHighGainDb(6.0f);
    eq.setLowGainDb(0.0f);
    eq.setMidGainDb(0.0f);

    auto input = makeSineBuffer(0.25f, 8000.0);
    const float inLevel = rms(input);

    juce::AudioBuffer<float> output(input);
    const int blocksToSettle = 8;
    for (int i = 0; i < blocksToSettle; ++i)
    {
        juce::AudioBuffer<float> temp(input);
        eq.processBlock(temp);
        output.makeCopyOf(temp);
    }

    EXPECT_NEAR(eq.getHighFreq(), 8000.0f, 1e-2f);
    EXPECT_GT(rms(output), inLevel);
}

TEST_F(EqualizerTest, TC_69_LowGainIsClamped)
{
    eq.setLowGainDb(-40.0f);
    float g1 = eq.getLowGainDb();

    eq.setLowGainDb(6.0f);
    float g2 = eq.getLowGainDb();

    eq.setLowGainDb(40.0f);
    float g3 = eq.getLowGainDb();

    EXPECT_FLOAT_EQ(g1, -24.0f);
    EXPECT_FLOAT_EQ(g2, 6.0f);
    EXPECT_FLOAT_EQ(g3, 24.0f);
}

TEST_F(EqualizerTest, TC_70_MidGainIsClamped)
{
    eq.setMidGainDb(-40.0f);
    float g1 = eq.getMidGainDb();

    eq.setMidGainDb(10.0f);
    float g2 = eq.getMidGainDb();

    eq.setMidGainDb(40.0f);
    float g3 = eq.getMidGainDb();

    EXPECT_FLOAT_EQ(g1, -24.0f);
    EXPECT_FLOAT_EQ(g2, 10.0f);
    EXPECT_FLOAT_EQ(g3, 24.0f);
}

TEST_F(EqualizerTest, TC_71_HighGainIsClamped)
{
    eq.setHighGainDb(-40.0f);
    float g1 = eq.getHighGainDb();

    eq.setHighGainDb(10.0f);
    float g2 = eq.getHighGainDb();

    eq.setHighGainDb(40.0f);
    float g3 = eq.getHighGainDb();

    EXPECT_FLOAT_EQ(g1, -24.0f);
    EXPECT_FLOAT_EQ(g2, 10.0f);
    EXPECT_FLOAT_EQ(g3, 24.0f);
}

TEST_F(EqualizerTest, TC_72_UnityGainBypassesSignal)
{
    eq.setLowGainDb(0.0f);
    eq.setMidGainDb(0.0f);
    eq.setHighGainDb(0.0f);

    auto input = makeSineBuffer(0.4f, 440.0);
    juce::AudioBuffer<float> output(input);

    eq.processBlock(output);

    EXPECT_NEAR(rms(output), rms(input), 1e-4f);
}

TEST_F(EqualizerTest, TC_73_GainSmoothingGraduallyChangesLevel)
{
    eq.setLowGainDb(0.0f);
    eq.setMidGainDb(0.0f);
    eq.setHighGainDb(0.0f);

    auto input = makeSineBuffer(0.3f, 1000.0);

    juce::AudioBuffer<float> baseline(input);
    eq.processBlock(baseline);
    const float baselineRms = rms(baseline);

    juce::AudioBuffer<float> first(input);
    eq.setMidGainDb(12.0f);
    eq.processBlock(first);
    const float out1 = rms(first);

    juce::AudioBuffer<float> last(first);
    const int extraBlocks = 10;
    for (int i = 0; i < extraBlocks; ++i)
    {
        juce::AudioBuffer<float> temp(input);
        eq.processBlock(temp);
        last.makeCopyOf(temp);
    }
    const float outN = rms(last);

    EXPECT_GT(outN, baselineRms);
    EXPECT_GT(outN, out1);
}

TEST_F(EqualizerTest, TC_74_ResetClearsState)
{
    auto input = makeSineBuffer(0.4f, 600.0);
    eq.setLowGainDb(6.0f);
    eq.processBlock(input);

    eq.reset();

    juce::AudioBuffer<float> silence(numChannels, blockSize);
    silence.clear();
    eq.processBlock(silence);

    EXPECT_LT(rms(silence), 1e-5f);
}

TEST_F(EqualizerTest, TC_75_ExtraChannelsAreCleared)
{
    eq.prepare(sampleRate, blockSize, 2);
    eq.setHighGainDb(6.0f);

    // Warm up smoothing so gain exceeds bypass threshold
    for (int i = 0; i < 8; ++i)
    {
        juce::AudioBuffer<float> warmup(2, blockSize);
        for (int ch = 0; ch < warmup.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill(warmup.getWritePointer(ch), 0.5f, blockSize);
        eq.processBlock(warmup);
    }

    juce::AudioBuffer<float> buffer(4, blockSize);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        buffer.clear(ch, 0, blockSize);
        juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.5f, blockSize);
    }

    eq.processBlock(buffer);

    for (int sample = 0; sample < blockSize; ++sample)
    {
        EXPECT_FLOAT_EQ(buffer.getSample(2, sample), 0.0f);
        EXPECT_FLOAT_EQ(buffer.getSample(3, sample), 0.0f);
    }
}