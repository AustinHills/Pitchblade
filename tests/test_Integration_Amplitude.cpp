//Austin

#include <gtest/gtest.h>
#include <JuceHeader.h>
#include <cmath>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"

class IntegrationAmplitudeTest : public ::testing::Test {
protected:
    juce::ScopedJuceInitialiser_GUI guiInitialiser;

    std::unique_ptr<AudioPluginAudioProcessor> plugin;
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midiBuffer;
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;

    //Keep track of phase across blocks
    double currentPhase = 0.0;

    void SetUp() override {
        plugin = std::make_unique<AudioPluginAudioProcessor>();
        plugin->prepareToPlay(sampleRate, samplesPerBlock);
        
        // Ensure we start with a clean slate
        plugin->clearAllNodes();

        buffer.setSize(2, samplesPerBlock);
        buffer.clear();
    }

    // NEW Helper: Add a single node to the chain via APVTS
    void addNodeToChain(const juce::String& type, const juce::String& name) {
        auto chain = plugin->apvts.state.getChildWithName("Chain");
        
        juce::ValueTree newNode(type);
        newNode.setProperty("name", name, nullptr);
        newNode.setProperty("uuid", juce::Uuid().toString(), nullptr);
        
        // This triggers the listener in PluginProcessor, which calls syncChainFromState()
        chain.addChild(newNode, -1, &plugin->undoManager);
        
        // Force synchronous update just in case listener is async (it shouldn't be for standard ValueTree, but safe)
        plugin->syncChainFromState();
    }

    // Helper: Find node by type string
    std::shared_ptr<EffectNode> getNodeByType(const juce::String& typeName) {
        auto& nodes = plugin->getEffectNodes();
        for (auto& node : nodes) {
            if (node && node->getNodeType() == typeName)
                return node;
        }
        return nullptr;
    }

    // Helper: Set generic property on node
    void setNodeProperty(std::shared_ptr<EffectNode> node, const juce::String& propertyId, const juce::var& value) {
        if (node) {
            node->getMutableNodeState().setProperty(propertyId, value, nullptr);
        }
    }

    //Helper to calculate blocks needed for a duration in ms
    int blocksForMS(float ms){
        return (int)(std::ceil((ms/1000) * 44100 / (double)512));
    }

    std::vector<float> makeSineFrame(float frequency, int bufferSize){
        std::vector<float> sineFrame(bufferSize);

        double phaseIncrement = (2.0 * juce::MathConstants<double>::pi * frequency) / sampleRate;

        for(int i = 0; i < bufferSize; ++i){
            sineFrame[i] = float(std::sin(currentPhase));

            currentPhase += phaseIncrement;
            if (currentPhase >= 2.0 * juce::MathConstants<double>::pi) {
                currentPhase -= 2.0 * juce::MathConstants<double>::pi;
            }
        }
        return sineFrame;
    }

    //Helper to simulate a constant signal
    void simulateConstantSignal(juce::AudioBuffer<float>& buffer, float msDuration, float signalValue){
        int numBlocks = blocksForMS(msDuration);
        for(int i = 0; i < numBlocks; i++){
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel){
                juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), signalValue, buffer.getNumSamples());
            }
            plugin->processBlock(buffer, midiBuffer);
        }
    }

    //Helper to simulate a sine wave
    void simulateSineSignal(juce::AudioBuffer<float>& buffer, float msDuration, float frequency, float amplitude){
        int numBlocks = blocksForMS(msDuration);

        for(int i = 0; i < numBlocks; i++){

            auto sineData = makeSineFrame(frequency, samplesPerBlock);
        
            juce::FloatVectorOperations::multiply(sineData.data(), amplitude, samplesPerBlock);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel){
                juce::FloatVectorOperations::copy(buffer.getWritePointer(channel), sineData.data(), samplesPerBlock);
            }

            plugin->processBlock(buffer, midiBuffer);
        }
    }
};


//Test for TC-76
TEST_F(IntegrationAmplitudeTest, GainNode_ApplyPositiveGain)
{
    addNodeToChain("GainNode", "Gain");
    auto gainNode = getNodeByType("GainNode");
    ASSERT_NE(gainNode, nullptr); // Ensure node was created

    setNodeProperty(gainNode, "Gain", juce::Decibels::gainToDecibels(2.0f));

    simulateConstantSignal(buffer, 20.0f, 0.5f);

    ASSERT_FLOAT_EQ(buffer.getSample(0, 100), 1.0f);
}

//Test for TC-77
TEST_F(IntegrationAmplitudeTest, NoiseGateNode_SignalBelowThreshold)
{
    addNodeToChain("NoiseGateNode", "Noise Gate");
    auto gateNode = getNodeByType("NoiseGateNode");
    ASSERT_NE(gateNode, nullptr);

    setNodeProperty(gateNode, "GateThreshold", -10.0f);
    setNodeProperty(gateNode, "GateAttack", 10.0f);
    setNodeProperty(gateNode, "GateRelease", 10.0f);

    simulateConstantSignal(buffer, 500.0f, juce::Decibels::decibelsToGain(-20.0f));

    ASSERT_NEAR(buffer.getSample(0, 100),0.0f,0.001f);
}

//Test for TC-78
TEST_F(IntegrationAmplitudeTest, NoiseGateNode_SignalAboveThreshold)
{
    addNodeToChain("NoiseGateNode", "Noise Gate");
    auto gateNode = getNodeByType("NoiseGateNode");
    ASSERT_NE(gateNode, nullptr);

    setNodeProperty(gateNode, "GateThreshold", -10.0f);
    setNodeProperty(gateNode, "GateAttack", 10.0f);
    setNodeProperty(gateNode, "GateRelease", 10.0f);

    simulateConstantSignal(buffer, 500.0f, 0.5f);

    ASSERT_NEAR(buffer.getSample(0, 100),0.5f,0.001f);
}

//Test for TC-79
TEST_F(IntegrationAmplitudeTest, CompressorNode_SignalBelowThreshold)
{
    addNodeToChain("CompressorNode", "Compressor");
    auto compNode = getNodeByType("CompressorNode");
    ASSERT_NE(compNode, nullptr);

    setNodeProperty(compNode, "CompThreshold", -30.0f);
    setNodeProperty(compNode, "CompRatio", 4.0f);
    setNodeProperty(compNode, "CompAttack", 10.0f);
    setNodeProperty(compNode, "CompRelease", 10.0f);

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-40.0f), 0.001f);
}

//Test for TC-80
TEST_F(IntegrationAmplitudeTest, CompressorNode_SignalAboveThreshold)
{
    addNodeToChain("CompressorNode", "Compressor");
    auto compNode = getNodeByType("CompressorNode");
    ASSERT_NE(compNode, nullptr);

    setNodeProperty(compNode, "CompThreshold", -20.0f);
    setNodeProperty(compNode, "CompRatio", 4.0f);
    setNodeProperty(compNode, "CompAttack", 10.0f);
    setNodeProperty(compNode, "CompRelease", 10.0f);

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-18.0f), 0.005f);
}

//Test for TC-81
TEST_F(IntegrationAmplitudeTest, CompressorNode_LimiterMode)
{
    addNodeToChain("CompressorNode", "Compressor");
    auto compNode = getNodeByType("CompressorNode");
    ASSERT_NE(compNode, nullptr);

    setNodeProperty(compNode, "CompThreshold", -40.0f);
    setNodeProperty(compNode, "CompRatio", 2.0f);
    setNodeProperty(compNode, "CompLimiterMode", true); 

    simulateConstantSignal(buffer, 500, juce::Decibels::decibelsToGain(-20.0f));

    ASSERT_NEAR(buffer.getSample(0, 0), juce::Decibels::decibelsToGain(-40.0f), 0.005f);
}

//Test for TC-82
TEST_F(IntegrationAmplitudeTest, DeEsserNode_SignalInsideFrequencyAndAboveThreshold)
{
    addNodeToChain("DeEsserNode", "De-Esser");
    auto deesserNode = getNodeByType("DeEsserNode");
    ASSERT_NE(deesserNode, nullptr);

    setNodeProperty(deesserNode, "DeEsserThreshold", -20.0f);
    setNodeProperty(deesserNode, "DeEsserRatio", 4.0f);
    setNodeProperty(deesserNode, "DeEsserFrequency", 6000.0f);
    setNodeProperty(deesserNode, "DeEsserAttack", 10.0f);
    setNodeProperty(deesserNode, "DeEsserRelease", 10.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-18.0f), 0.05f);
}

//Test for TC-83
TEST_F(IntegrationAmplitudeTest, DeEsserNode_SignalOutsideFrequencyAndAboveThreshold)
{
    addNodeToChain("DeEsserNode", "De-Esser");
    auto deesserNode = getNodeByType("DeEsserNode");
    ASSERT_NE(deesserNode, nullptr);

    setNodeProperty(deesserNode, "DeEsserThreshold", -20.0f);
    setNodeProperty(deesserNode, "DeEsserRatio", 2.0f);
    setNodeProperty(deesserNode, "DeEsserFrequency", 6000.0f);
    setNodeProperty(deesserNode, "DeEsserAttack", 10.0f);
    setNodeProperty(deesserNode, "DeEsserRelease", 10.0f);

    simulateSineSignal(buffer, 500.0f, 500.0f, juce::Decibels::decibelsToGain(-12.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-12.0f), 0.005f);
}

//Test for TC-84
TEST_F(IntegrationAmplitudeTest, DeEsserNode_SignalInsideFrequencyAndBelowThreshold)
{
    addNodeToChain("DeEsserNode", "De-Esser");
    auto deesserNode = getNodeByType("DeEsserNode");
    ASSERT_NE(deesserNode, nullptr);

    setNodeProperty(deesserNode, "DeEsserThreshold", -20.0f);
    setNodeProperty(deesserNode, "DeEsserRatio", 2.0f);
    setNodeProperty(deesserNode, "DeEsserFrequency", 6000.0f);
    setNodeProperty(deesserNode, "DeEsserAttack", 10.0f);
    setNodeProperty(deesserNode, "DeEsserRelease", 10.0f);

    simulateSineSignal(buffer, 500.0f, 6000.0f, juce::Decibels::decibelsToGain(-40.0f));

    ASSERT_NEAR(buffer.getMagnitude(0, samplesPerBlock), juce::Decibels::decibelsToGain(-40.0f), 0.001f);
}

//Test for TC-85
TEST_F(IntegrationAmplitudeTest, DeNoiserNode_LearnAndReduce)
{
    addNodeToChain("DeNoiserNode", "De-Noiser");
    auto denoiserNode = getNodeByType("DeNoiserNode");
    ASSERT_NE(denoiserNode, nullptr);

    setNodeProperty(denoiserNode, "DenoiserLearn", true); 
    simulateSineSignal(buffer, 500.0f, 1000.0f, juce::Decibels::decibelsToGain(-40.0f));

    setNodeProperty(denoiserNode, "DenoiserLearn", false);
    setNodeProperty(denoiserNode, "DenoiserReduction", 1.0f);

    simulateSineSignal(buffer, 500.0f, 1000.0f, juce::Decibels::decibelsToGain(-40.0f));

    float rms = buffer.getRMSLevel(0, 0, 512);
    ASSERT_LT(rms, 0.0001f);
}