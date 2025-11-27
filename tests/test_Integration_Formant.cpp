// huda - integration tests for FormantNode + FormantPanel
#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/FormantPanel.h"

// Helpers to line up with the processor's public API.
static juce::AudioProcessorValueTreeState& getApvts(AudioPluginAudioProcessor& processor)
{
    return processor.apvts;
}

static std::vector<float> getLatestFormantFrequencies(AudioPluginAudioProcessor& processor)
{
    return processor.getLatestFormants();
}

// Find a node by display name
static std::shared_ptr<EffectNode> findNode(AudioPluginAudioProcessor& processor,
                                            const juce::String& name)
{
    for (auto& node : processor.getEffectNodes())
        if (node && node->effectName == name)
            return node;
    return {};
}

//=========== helpers ============

static float computeRms(const juce::AudioBuffer<float>& buffer)
{
    const int channels = buffer.getNumChannels();
    const int samples  = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return 0.0f;

    double sumSquares = 0.0;
    int count = 0;

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* ptr = buffer.getReadPointer(ch);
        for (int i = 0; i < samples; ++i)
        {
            const float v = ptr[i];
            sumSquares += static_cast<double>(v) * static_cast<double>(v);
            ++count;
        }
    }

    if (count == 0)
        return 0.0f;

    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
}

static float bufferDifferenceRms(const juce::AudioBuffer<float>& a,
                                 const juce::AudioBuffer<float>& b)
{
    const int channels = juce::jmin(a.getNumChannels(), b.getNumChannels());
    const int samples  = juce::jmin(a.getNumSamples(),  b.getNumSamples());

    double sumSquares = 0.0;
    int count = 0;

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* pa = a.getReadPointer(ch);
        const float* pb = b.getReadPointer(ch);

        for (int i = 0; i < samples; ++i)
        {
            const float diff = pa[i] - pb[i];
            sumSquares += static_cast<double>(diff) * static_cast<double>(diff);
            ++count;
        }
    }

    if (count == 0)
        return 0.0f;

    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(count)));
}

// Very small utility: dominant frequency estimate via JUCE FFT.
// Only needs to be approximate to distinguish 800 Hz as same vs different.
static float estimateDominantFrequency(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate)
{
    const int availableSamples = buffer.getNumSamples();
    if (availableSamples < 16)
        return 0.0f;

    // Choose largest power-of-two <= availableSamples (but not too tiny)
    int order = 1;
    while ((1 << (order + 1)) <= availableSamples && order < 14)
        ++order;

    const int fftSize = 1 << order;
    juce::dsp::FFT fft(order);

    juce::HeapBlock<float> fftData;
    fftData.allocate(2 * fftSize, true);

    // Use channel 0 for frequency estimate
    const float* src = buffer.getReadPointer(0);
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = src[i];

    // Rest is already zeroed (imag part)
    fft.performRealOnlyForwardTransform(fftData.getData());

    int bestBin = 0;
    float bestMag = 0.0f;

    // Only search up to Nyquist
    const int maxBin = fftSize / 2;
    for (int bin = 1; bin < maxBin; ++bin)
    {
        const float re = fftData[2 * bin];
        const float im = fftData[2 * bin + 1];
        const float mag = std::sqrt(re * re + im * im);

        if (mag > bestMag)
        {
            bestMag = mag;
            bestBin = bin;
        }
    }

    if (bestBin == 0)
        return 0.0f;

    return static_cast<float>(bestBin * sampleRate / fftSize);
}

// ======= processor integration fixture ============

class FormantNodeIntegrationTest : public ::testing::Test
{
protected:
    AudioPluginAudioProcessor processor;
    double sampleRate = 48000.0;
    int blockSize = 256;
    int numChannels = 2;

    void SetUp() override
    {
        processor.prepareToPlay(sampleRate, blockSize);
    }

    // Generate one block of a continuous sine by reference phase.
    juce::AudioBuffer<float> makeSineBlock(float amplitude,
                                           double frequency,
                                           double& phase)
    {
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        const double increment = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int i = 0; i < blockSize; ++i)
        {
            const float value = static_cast<float>(std::sin(phase) * amplitude);
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, value);
            phase += increment;
        }

        return buffer;
    }

    // Richer signal (adds first 3 harmonics) so formant shifting has audible effect.
    juce::AudioBuffer<float> makeHarmonicBlock(float amplitude,
                                               double frequency,
                                               double& phaseFundamental)
    {
        juce::AudioBuffer<float> buffer(numChannels, blockSize);
        const double increment = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

        for (int i = 0; i < blockSize; ++i)
        {
            float sample = 0.0f;
            for (int h = 1; h <= 3; ++h)
            {
                const double phaseH = phaseFundamental * static_cast<double>(h);
                const float partialAmp = amplitude / static_cast<float>(h);
                sample += static_cast<float>(std::sin(phaseH) * partialAmp);
            }

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, sample);

            phaseFundamental += increment;
        }

        return buffer;
    }

    //insert a single-row layout with a Formant node.
    void insertSingleFormantNode()
    {
        std::vector<AudioPluginAudioProcessor::Row> layout;
        layout.push_back({ "Formant", "" });
        processor.requestLayout(layout);
    }
};

// ========TC-86 ================
// FormantNode - Neutral shift with fully wet mix.
// Output stays neutral (same amp/freq) but formant detection runs.

TEST_F(FormantNodeIntegrationTest,
       TC_86_FormantNodeNeutralShiftFullyWet_PassesAudioAndDetectsFormants)
{
    insertSingleFormantNode();

    auto& apvts = getApvts(processor);
    auto* shiftParam = apvts.getRawParameterValue("FORMANT_SHIFT");
    auto* mixParam   = apvts.getRawParameterValue("FORMANT_MIX");

    ASSERT_NE(shiftParam, nullptr);
    ASSERT_NE(mixParam, nullptr);

    shiftParam->store(0.0f); // neutral shift
    mixParam->store(1.0f); // fully wet

    const float amplitude  = 0.18f;
    const double frequency = 800.0; // mid-range test tone

    const int minBlocks = 64;
    // Allow latency + some extra to fully flush RubberBand
    const int blocksNeeded =
        std::max(minBlocks, (processor.getLatencySamples() / blockSize) + 16);

    double phase = 0.0;
    juce::MidiBuffer midi;

    juce::AudioBuffer<float> lastInput (numChannels, blockSize);
    juce::AudioBuffer<float> lastOutput(numChannels, blockSize);

    for (int block = 0; block < blocksNeeded; ++block)
    {
        auto inBlock = makeSineBlock(amplitude, frequency, phase);
        lastInput.makeCopyOf(inBlock);

        juce::AudioBuffer<float> procBlock;
        procBlock.makeCopyOf(inBlock);

        processor.processBlock(procBlock, midi);
        lastOutput.makeCopyOf(procBlock);
    }

    const float inRms  = computeRms(lastInput);
    const float outRms = computeRms(lastOutput);

    //Amplitude roughly preserved (within 10%)
    ASSERT_GT(inRms, 1e-4f);
    EXPECT_NEAR(outRms, inRms, inRms * 0.1f);

    //Dominant frequency unchanged (should still be ~800 Hz)
    const float inFreq  = estimateDominantFrequency(lastInput,  sampleRate);
    const float outFreq = estimateDominantFrequency(lastOutput, sampleRate);

    EXPECT_NEAR(outFreq, inFreq, 30.0f); // loose; we just need "same band"

    //Formant detection produced non-empty list in 300-5000 Hz.
    auto formants = getLatestFormantFrequencies(processor);
    ASSERT_FALSE(formants.empty());

    for (float f : formants)
    {
        EXPECT_GE(f, 300.0f);
        EXPECT_LE(f, 5000.0f);
    }
}

// ======== TC-87 =========
// FormantNode - Positive shift with fully wet mix.
// Compare against neutral baseline: output & formants differ.

//Altered by Austin to account for the changes to the formant panel
TEST_F(FormantNodeIntegrationTest,
       TC_87_FormantNodePositiveShift_FullyWet_ChangesOutputAndFormants)
{
    insertSingleFormantNode();

    auto node = findNode(processor, "Formant");
    ASSERT_NE(node, nullptr);

    const float amplitude  = 0.18f;
    const double frequency = 800.0;
    juce::MidiBuffer midi;

    //Baseline
    node->getMutableNodeState().setProperty("FORMANT_SHIFT", 0.0f, nullptr);
    node->getMutableNodeState().setProperty("FORMANT_MIX", 1.0f, nullptr);

    const int baselineBlocks =
        std::max(48, (processor.getLatencySamples() / blockSize) + 16);

    double phaseBaseline = 0.0;
    juce::AudioBuffer<float> neutralOutput(numChannels, blockSize);

    for (int i = 0; i < baselineBlocks; ++i)
    {
        auto inBlock = makeHarmonicBlock(amplitude, frequency, phaseBaseline);
        juce::AudioBuffer<float> procBlock;
        procBlock.makeCopyOf(inBlock);

        processor.processBlock(procBlock, midi);
        neutralOutput.makeCopyOf(procBlock);
    }

    auto formantsNeutral = getLatestFormantFrequencies(processor);
    ASSERT_FALSE(formantsNeutral.empty());

    //Shifted run
    node->getMutableNodeState().setProperty("FORMANT_SHIFT", 40.0f, nullptr);
    node->getMutableNodeState().setProperty("FORMANT_MIX", 1.0f, nullptr);

    const double durationSeconds = 0.5;
    const int totalSamplesNeeded = static_cast<int>(durationSeconds * sampleRate);
    const int shiftedBlocks = std::max(
        totalSamplesNeeded / blockSize,
        (processor.getLatencySamples() / blockSize) + 16);

    double phaseShifted = 0.0;
    juce::AudioBuffer<float> outShifted(numChannels, blockSize);

    for (int i = 0; i < shiftedBlocks; ++i)
    {
        auto inBlock = makeHarmonicBlock(amplitude, frequency, phaseShifted);
        juce::AudioBuffer<float> procBlock;
        procBlock.makeCopyOf(inBlock);

        processor.processBlock(procBlock, midi);
        outShifted.makeCopyOf(procBlock);
    }

    auto formantsShifted = getLatestFormantFrequencies(processor);
    ASSERT_FALSE(formantsShifted.empty());

    //Output numerically different from neutral.
    const float diffRms = bufferDifferenceRms(neutralOutput, outShifted);
    EXPECT_GT(diffRms, 1e-3f); // clearly different beyond tiny tolerance

    //Formant list differs, but still in 300-5000 Hz.
    bool anyDifferent = false;
    const size_t n = std::min(formantsNeutral.size(), formantsShifted.size());

    for (size_t i = 0; i < n; ++i)
    {
        if (std::fabs(formantsNeutral[i] - formantsShifted[i]) > 5.0f)
        {
            anyDifferent = true;
            break;
        }
    }

    // If sizes differ they're certainly not identical.
    if (formantsNeutral.size() != formantsShifted.size())
        anyDifferent = true;

    EXPECT_TRUE(anyDifferent);

    for (float f : formantsShifted)
    {
        EXPECT_GE(f, 300.0f);
        EXPECT_LE(f, 5000.0f);
    }
}

// ======== TC-88 ===========
// FormantNode - Bypass behavior.
// Active: shifted output.
// Bypassed: dry/neutral signal regardless of shift/mix settings.

//Altered by Austin to account for the changes to the formant panel
TEST_F(FormantNodeIntegrationTest,
       TC_88_FormantNodeBypassProducesDrySignal)
{
    insertSingleFormantNode();

    auto node = findNode(processor, "Formant");
    ASSERT_NE(node, nullptr);

    node->getMutableNodeState().setProperty("FORMANT_SHIFT", 40.0f, nullptr);
    node->getMutableNodeState().setProperty("FORMANT_MIX", 1.0f, nullptr);

    const float amplitude= 0.2f;
    const double frequency = 800.0;
    juce::MidiBuffer midi;

    const int blocksNeeded = std::max(64, (processor.getLatencySamples() / blockSize) + 16);
    double phase = 0.0;
    juce::AudioBuffer<float> inputRef  (numChannels, blockSize);
    juce::AudioBuffer<float> outActive (numChannels, blockSize);
    juce::AudioBuffer<float> outBypass(numChannels, blockSize);

    //Active (not bypassed)
    for (int i = 0; i < blocksNeeded; ++i)
    {
        auto inBlock = makeHarmonicBlock(amplitude, frequency, phase);
        juce::AudioBuffer<float> procBlock;
        procBlock.makeCopyOf(inBlock);

        processor.processBlock(procBlock, midi);

        if (i == blocksNeeded - 1)
        {
            inputRef.makeCopyOf(inBlock);
            outActive.makeCopyOf(procBlock);
        }
    }

    // outActive should differ from input (shifted formants)
    const float activeDiff = bufferDifferenceRms(inputRef, outActive);
    EXPECT_GT(activeDiff, 1e-3f);

    //Bypass the Formant node
    auto formantNode = findNode(processor, "Formant");
    ASSERT_TRUE(formantNode);
    formantNode->bypassed = true;

    // Process again with the same tone; capture bypassed output.
    phase = 0.0; // restart test tone to compare easily
    for (int i = 0; i < blocksNeeded; ++i)
    {
        auto inBlock = makeHarmonicBlock(amplitude, frequency, phase);
        juce::AudioBuffer<float> procBlock;
        procBlock.makeCopyOf(inBlock);

        processor.processBlock(procBlock, midi);

        if (i == blocksNeeded - 1)
        {
            inputRef.makeCopyOf(inBlock);
            outBypass.makeCopyOf(procBlock);
        }
    }

    const float bypassDiff = bufferDifferenceRms(inputRef, outBypass);
    const float inRms = computeRms(inputRef);
    const float outRms = computeRms(outBypass);

    // Bypassed output should be very close to dry input.
    EXPECT_LT(bypassDiff, 1e-3f);
    EXPECT_NEAR(outRms, inRms, inRms * 0.05f);
}

// ======== TC-89 ===========
// FormantPanel + APVTS - Slider to parameter wiring.
class FormantPanelApvtsTest : public ::testing::Test
{
protected:
    AudioPluginAudioProcessor processor;
    double sampleRate = 48000.0;
    int blockSize     = 256;

    void SetUp() override
    {
        processor.prepareToPlay(sampleRate, blockSize);
    }
};

//Altered by Austin to make sure this can run with the current changes that Reyna included with her unit test fixes to allow save and load
TEST_F(FormantPanelApvtsTest,
       TC_89_FormantPanelSlidersUpdateApvtsParameters)
{

    juce::ValueTree testState("FormantNode");
    
    // Initialize properties (mimicking FormantNode constructor)
    testState.setProperty("FORMANT_SHIFT", 0.0f, nullptr);
    testState.setProperty("FORMANT_MIX", 1.0f, nullptr);

    // Construct panel with the processor
    FormantPanel panel(processor,testState);

    // Make sure panel is properly initialized
    panel.setSize(400, 200);

    auto& shiftSlider = panel.getShiftSlider();
    auto& mixSlider = panel.getMixSlider();

    // Move sliders using the UI API (this should drive attachments).
    const float targetShift = 25.0f;
    const float targetMix = 0.5f;

    shiftSlider.setValue(targetShift, juce::NotificationType::sendNotificationSync);
    mixSlider.setValue(targetMix, juce::NotificationType::sendNotificationSync);

    const float shiftVal = (float)testState.getProperty("FORMANT_SHIFT");
    const float mixVal  = (float)testState.getProperty("FORMANT_MIX");

    EXPECT_NEAR(shiftVal, targetShift, 0.5f);
    EXPECT_NEAR(mixVal, targetMix, 0.05f);
}