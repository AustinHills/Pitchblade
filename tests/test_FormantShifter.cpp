//huda
#include "Pitchblade/effects/FormantShifter.h"

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

class FormantShifterTest : public ::testing::Test {
protected:
    FormantShifter shifter;
    double sampleRate = 48000.0;
    int maxBlockSize = 256;
    int numChannels = 2;

    void SetUp() override
    {
        shifter.prepare(sampleRate, maxBlockSize, numChannels);
    }

    juce::AudioBuffer<float> makeSineBuffer(float amplitude = 0.5f, double frequency = 440.0)
    {
        juce::AudioBuffer<float> buffer(numChannels, maxBlockSize);
        double phase = 0.0;
        const double increment = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int sample = 0; sample < maxBlockSize; ++sample)
        {
            float value = static_cast<float>(std::sin(phase) * amplitude);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, sample, value);
            phase += increment;
        }

        return buffer;
    }

    float bufferDifferenceRms(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int channels = juce::jmin(a.getNumChannels(), b.getNumChannels());
        const int samples = juce::jmin(a.getNumSamples(), b.getNumSamples());

        float sumSquares = 0.0f;
        int count = 0;

        for (int ch = 0; ch < channels; ++ch)
            for (int i = 0; i < samples; ++i)
            {
                const float diff = a.getSample(ch, i) - b.getSample(ch, i);
                sumSquares += diff * diff;
                ++count;
            }

        if (count == 0)
            return 0.0f;

        return std::sqrt(sumSquares / static_cast<float>(count));
    }

    bool buffersClose(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, float tolerance)
    {
        return bufferDifferenceRms(a, b) <= tolerance;
    }

    juce::AudioBuffer<float> processWithMix(float shift, float mix, const juce::AudioBuffer<float>& inputTemplate, int minBlocks)
    {
        juce::AudioBuffer<float> lastOutput(inputTemplate.getNumChannels(), inputTemplate.getNumSamples());
        const int blocksNeeded = std::max(minBlocks, (shifter.getLatencySamples() / inputTemplate.getNumSamples()) + 2);

        for (int block = 0; block < blocksNeeded; ++block)
        {
            juce::AudioBuffer<float> buffer(inputTemplate.getNumChannels(), inputTemplate.getNumSamples());
            buffer.makeCopyOf(inputTemplate);

            shifter.setShiftAmount(shift);

            juce::AudioBuffer<float> dry(buffer);
            shifter.processBlock(buffer); // buffer now holds wet signal

            juce::AudioBuffer<float> wet(buffer);

            const float wetGain = juce::jlimit(0.0f, 1.0f, mix);
            const float dryGain = 1.0f - wetGain;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const float* dryPtr = dry.getReadPointer(ch);
                const float* wetPtr = wet.getReadPointer(ch);
                float* outPtr = buffer.getWritePointer(ch);

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    outPtr[i] = dryGain * dryPtr[i] + wetGain * wetPtr[i];
            }

            lastOutput.makeCopyOf(buffer);
        }

        return lastOutput;
    }
};

TEST_F(FormantShifterTest, TC_40_PrepareAndProcessBlockRunsSafely)
{
    juce::AudioBuffer<float> buffer = makeSineBuffer(0.25f);

    EXPECT_NO_THROW(shifter.processBlock(buffer));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            ASSERT_TRUE(std::isfinite(buffer.getSample(ch, i)));
}

TEST_F(FormantShifterTest, TC_41_AmountClampMapsIntoRatioRange)
{
    shifter.setShiftAmount(-50.0f);
    EXPECT_NEAR(shifter.getFormantRatio(), 0.8f, 1e-4f);

    shifter.setShiftAmount(0.0f);
    EXPECT_NEAR(shifter.getFormantRatio(), 1.0f, 1e-4f);

    shifter.setShiftAmount(50.0f);
    EXPECT_NEAR(shifter.getFormantRatio(), 1.25f, 1e-4f);

    EXPECT_GE(shifter.getFormantRatio(), 0.8f);
    EXPECT_LE(shifter.getFormantRatio(), 1.25f);
}

TEST_F(FormantShifterTest, TC_42_SetShiftAmountClampsInputRange)
{
    shifter.setShiftAmount(-100.0f);
    EXPECT_FLOAT_EQ(shifter.getShiftAmount(), -50.0f);

    shifter.setShiftAmount(25.0f);
    EXPECT_FLOAT_EQ(shifter.getShiftAmount(), 25.0f);

    shifter.setShiftAmount(100.0f);
    EXPECT_FLOAT_EQ(shifter.getShiftAmount(), 50.0f);
}

TEST_F(FormantShifterTest, TC_43_DryWetExtremesBehaveCorrectly)
{
    shifter.reset();
    const auto input = makeSineBuffer(0.6f, 660.0);

    auto outDry = processWithMix(40.0f, 0.0f, input, 12);
    EXPECT_LT(bufferDifferenceRms(outDry, input), 5e-3f);

    shifter.reset();
    const int wetBlocks = 32;
    auto outWet = processWithMix(40.0f, 1.0f, input, wetBlocks);

    const int processedSamples = wetBlocks * input.getNumSamples();
    const float wetRms = outWet.getRMSLevel(0, 0, outWet.getNumSamples());
    const float diffRms = bufferDifferenceRms(outWet, input);

    if (shifter.getLatencySamples() >= processedSamples)
    {
        EXPECT_LT(wetRms, 1e-3f);
    }
    else
    {
        EXPECT_GT(wetRms, 1e-4f);
        EXPECT_GT(diffRms, 1e-5f);
    }
}

TEST_F(FormantShifterTest, TC_44_DryWetBlendProducesInterpolatedResult)
{
    const auto input = makeSineBuffer(0.7f, 520.0);

    shifter.reset();
    auto outDry = processWithMix(45.0f, 0.0f, input, 6);

    shifter.reset();
    auto outWet = processWithMix(45.0f, 1.0f, input, 12);

    shifter.reset();
    auto outBlend = processWithMix(45.0f, 0.5f, input, 12);

    EXPECT_FALSE(buffersClose(outBlend, outDry, 1e-4f));
    EXPECT_FALSE(buffersClose(outBlend, outWet, 1e-4f));

    int betweenCount = 0;
    const int samples = outBlend.getNumSamples();
    for (int i = 0; i < samples; ++i)
    {
        const float d = outDry.getSample(0, i);
        const float w = outWet.getSample(0, i);
        const float b = outBlend.getSample(0, i);
        const float minVal = std::min(d, w);
        const float maxVal = std::max(d, w);

        if (b >= minVal - 1e-3f && b <= maxVal + 1e-3f)
            ++betweenCount;
    }

    EXPECT_GT(betweenCount, samples * 0.7f);
}

TEST_F(FormantShifterTest, TC_45_ProcessBlockReflectsLatencyThenAudio)
{
    shifter.reset();
    shifter.setShiftAmount(30.0f);

    const int totalBlocks = std::max(24, (shifter.getLatencySamples() / maxBlockSize) + 16);
    std::vector<float> rmsValues;
    rmsValues.reserve(totalBlocks);

    for (int i = 0; i < totalBlocks; ++i)
    {
        juce::AudioBuffer<float> buffer(numChannels, maxBlockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.5f, buffer.getNumSamples());

        shifter.processBlock(buffer);
        rmsValues.push_back(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    }

    const int latencyBlocks = (shifter.getLatencySamples() + maxBlockSize - 1) / maxBlockSize;
    const float threshold = 1e-4f;
    const int processedSamples = totalBlocks * maxBlockSize;

    const bool allSilent = std::all_of(rmsValues.begin(), rmsValues.end(),
                                       [threshold](float v) { return v < threshold; });

    if (allSilent && shifter.getLatencySamples() >= processedSamples)
    {
        SUCCEED();
        return;
    }

    bool anyNonZero = std::any_of(rmsValues.begin(), rmsValues.end(),
                                  [threshold](float v) { return v > threshold; });

    bool nonZeroAfterLatency = false;
    for (int i = latencyBlocks; i < static_cast<int>(rmsValues.size()); ++i)
        if (rmsValues[i] > threshold)
            nonZeroAfterLatency = true;

    EXPECT_TRUE(anyNonZero);
    EXPECT_TRUE(nonZeroAfterLatency);
}
