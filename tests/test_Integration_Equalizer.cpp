// huda
// Equalizer integration tests (node, panel wiring, visualizer, unity behavior).
#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EqualizerPanel.h"
#include "Pitchblade/ui/EqualizerVisualizer.h"
#include <limits>

// Helper: find a node by display name
static std::shared_ptr<EffectNode> findNode(AudioPluginAudioProcessor& processor,
                                            const juce::String& name)
{
    for (auto& node : processor.getEffectNodes())
        if (node && node->effectName == name)
            return node;
    return {};
}

//RMS helper for buffers.
static float computeRms(const juce::AudioBuffer<float>& buffer)
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return 0.0f;

    double sum = 0.0;
    int count = 0;
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* ptr = buffer.getReadPointer(ch);
        for (int i = 0; i < samples; ++i)
        {
            sum += static_cast<double>(ptr[i]) * static_cast<double>(ptr[i]);
            ++count;
        }
    }
    return count == 0 ? 0.0f : static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

// ======== Fixture =========

class EqualizerIntegrationTest : public ::testing::Test
{
protected:
    AudioPluginAudioProcessor processor;
    double sampleRate = 48000.0;
    int blockSize = 512;
    int numChannels = 2;

    void SetUp() override
    {
        processor.prepareToPlay(sampleRate, blockSize);
    }

    juce::AudioBuffer<float> makeSine(float amplitude, double freq, double& phase)
    {
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        const double inc = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;

        for (int i = 0; i < blockSize; ++i)
        {
            const float v = static_cast<float>(std::sin(phase) * amplitude);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, v);
            phase += inc;
        }
        return buffer;
    }

    // Wideband-ish noise for unity checks.
    juce::AudioBuffer<float> makeNoise(float amplitude, int seed = 1234)
    {
        juce::Random rng(seed);
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                buffer.setSample(ch, i, amplitude * (2.0f * rng.nextFloat() - 1.0f));
        return buffer;
    }

    void insertEqualizerNode()
    {
        std::vector<AudioPluginAudioProcessor::Row> layout;
        layout.push_back({ "Equalizer", "" });
        processor.requestLayout(layout);
    }
};

// =====TC-95 =======
TEST_F(EqualizerIntegrationTest, TC_95_EqualizerMidBoostRaisesRms)
{
    insertEqualizerNode();

    // Configure EQ
    auto& eq = processor.getEqualizer();
    eq.setMidFreq(1000.0f);
    eq.setMidGainDb(6.0f);
    eq.setLowGainDb(0.0f);
    eq.setHighGainDb(0.0f);
    double phase = 0.0;
    auto in = makeSine(0.2f, 1000.0, phase);
    auto processed = in;
    juce::MidiBuffer midi;
    // allow smoothing to ramp by running a few blocks
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            in = makeSine(0.2f, 1000.0, phase);
        processed = in;
        processor.processBlock(processed, midi);
    }
    const float inRms = computeRms(in);
    const float outRms = computeRms(processed);
    ASSERT_GT(inRms, 0.0f);
    EXPECT_GT(outRms, inRms); // boost should raise level
}

// ===== TC-96 =======
TEST_F(EqualizerIntegrationTest, TC_96_EqualizerPanelLowBandUpdatesDsp)
{
    insertEqualizerNode();

    auto node = findNode(processor, "Equalizer");
    ASSERT_TRUE(node);

    // Use the shared ValueTree backing this node to drive the panel/listener.
    auto& state = node->getMutableNodeState();

    // Construct panel with the shared state.
    EqualizerPanel panel(processor, state);
    panel.setSize(400, 200);

    // Drive parameters via ValueTree; listener will push to DSP.
    state.setProperty("LowFreq", 120.0f, nullptr);
    state.setProperty("LowGain", 6.0f, nullptr);

    // Read back DSP values.
    const float fLow = processor.getEqualizer().getLowFreq();
    const float gLow = processor.getEqualizer().getLowGainDb();

    EXPECT_NEAR(fLow, 120.0f, 1.0f);
    EXPECT_NEAR(gLow, 6.0f, 0.25f);
}

// ======= TC-97 =========
TEST_F(EqualizerIntegrationTest, TC_97_VisualizerResponseReflectsEqSettings)
{
    // Configure EQ with a low boost, flat mid, high cut.
    auto& eq = processor.getEqualizer();
    eq.setLowFreq(100.0f);
    eq.setLowGainDb(6.0f);
    eq.setMidFreq(1000.0f);
    eq.setMidGainDb(0.0f);
    eq.setHighFreq(8000.0f);
    eq.setHighGainDb(-6.0f);

    EqualizerVisualizer viz(processor);
    viz.setSize(400, 200);

    // Force one update of the response curve.
    viz.forceUpdateForTest();
    auto points = viz.getLastResponsePoints();

    ASSERT_FALSE(points.empty());

    auto findNear = [&](float targetHz) -> float
    {
        float bestDb = 0.0f;
        float bestDiff = std::numeric_limits<float>::max();
        for (auto& p : points)
        {
            float diff = std::abs(p.x - targetHz);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestDb = p.y;
            }
        }
        return bestDb;
    };

    const float dbLow  = findNear(100.0f);
    const float dbMid  = findNear(1000.0f);
    const float dbHigh = findNear(8000.0f);

    // Display dB is mapped to [-100..0]; more positive (toward 0) means boost.
    EXPECT_GT(dbLow, dbMid);   // low boosted relative to mid
    EXPECT_LT(dbHigh, dbMid);  // high cut relative to mid
}

// ======= TC-98 =========
TEST_F(EqualizerIntegrationTest, TC_98_UnityGainBehavesAsBypass)
{
    insertEqualizerNode();
    auto& eq = processor.getEqualizer();
    eq.setLowGainDb(0.0f);
    eq.setMidGainDb(0.0f);
    eq.setHighGainDb(0.0f);
    auto input = makeNoise(0.3f, 42);
    auto reference = input;
    juce::MidiBuffer midi;

    processor.processBlock(input, midi);

    // Compare RMS and allow tiny numerical wiggle.
    const float inRms = computeRms(reference);
    const float outRms = computeRms(input);
    EXPECT_NEAR(outRms, inRms, inRms * 0.02f);
}