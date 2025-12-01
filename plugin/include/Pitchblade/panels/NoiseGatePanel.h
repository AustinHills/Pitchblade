// austin
#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"

//gain panel ui
class NoiseGatePanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    explicit NoiseGatePanel(AudioPluginAudioProcessor& proc);
    void resized() override;
    void paint(juce::Graphics&) override;

    // overloaded constructor with local state
    NoiseGatePanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);

    // destructor
    ~NoiseGatePanel() override;

    // valueTree listener callback
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    juce::String panelTitle;

private:
    // Reference back to main processor
    AudioPluginAudioProcessor& processor;

    // Sliders for noise gate
    juce::Slider thresholdSlider, attackSlider, releaseSlider;

    // Labels
    juce::Label noiseGateLabel, thresholdLabel, attackLabel, releaseLabel;

    juce::ValueTree localState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGatePanel)
};

//Visualizer - Austin
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

class NoiseGateNode;

class NoiseGateVisualizer : public RealTimeGraphVisualizer, public juce::ValueTree::Listener{
public:
    explicit NoiseGateVisualizer(AudioPluginAudioProcessor& proc, NoiseGateNode& node, juce::ValueTree& state)
        : RealTimeGraphVisualizer(proc.apvts, "dB", {-100.0f, 0.0f}, false, 6),
          processor(proc),
          noiseGateNode(node),
          localState(state)
    {
        // Set initial threshold line
        float initialThreshold = (float)localState.getProperty("GateThreshold", -100.0f);
        setThreshold(initialThreshold, true);

        // Listen for changes to the threshold
        localState.addListener(this);
    }

    ~NoiseGateVisualizer() override;

    // Update the graph by polling the node
    void timerCallback() override ;

    // Update threshold line if property changes
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    AudioPluginAudioProcessor& processor;
    NoiseGateNode& noiseGateNode;
    juce::ValueTree localState;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGateVisualizer)
};

////////////////////////////////////////////////////////////

//reynas changes > added dsp node defn to ui panel creation
// noisegate dsp node , processes audio + makes own panel
// inherits from EffectNode base class
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/NoiseGateProcessor.h"

class NoiseGateNode : public EffectNode {
public:
	//create node with name n reference to main processor
    explicit NoiseGateNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "NoiseGateNode", "Noise Gate"), processor(proc) {
        
        if (!processor.apvts.state.hasType("EffectNodes")) {    
			processor.apvts.state = juce::ValueTree("EffectNodes"); // ensure EffectNodes tree exists - reyna
        }

		auto& state = getMutableNodeState();    // get mutable state - reyna
        // initialize default properties
        if (!getMutableNodeState().hasProperty("GateThreshold"))
            getMutableNodeState().setProperty("GateThreshold", -100.0f, nullptr);
        if (!getMutableNodeState().hasProperty("GateAttack"))
            getMutableNodeState().setProperty("GateAttack", 25.0f, nullptr);
        if (!getMutableNodeState().hasProperty("GateRelease"))
            getMutableNodeState().setProperty("GateRelease", 100.0f, nullptr);
        // ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");
        // add this node to processor state tree
        processor.apvts.state.addChild(getMutableNodeState(), -1, nullptr);

        processor.apvts.state.addChild(state, -1, nullptr); // add to processor state tree

        //Preparing the gate
        gateDSP.prepare(proc.getSampleRate());
    }

	// dsp processing step for noise gate
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {

        juce::ignoreUnused(proc);

        const float threshold = (float)getNodeState().getProperty("GateThreshold", -100.0f);
        const float attack = (float)getNodeState().getProperty("GateAttack", 25.0f);
        const float release = (float)getNodeState().getProperty("GateRelease", 100.0f);

        gateDSP.setThreshold(threshold);
        gateDSP.setAttack(attack);
        gateDSP.setRelease(release);
        gateDSP.process(buffer);

        //Calculate output level for visualizer
        float peakAmplitude = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        float levelDb = juce::Decibels::gainToDecibels(peakAmplitude, -100.0f);
        gateDSP.currentOutputLevelDb.store(levelDb);
    }

    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<NoiseGatePanel>(proc, getMutableNodeState(), effectName);
    }

    // return visualizer 
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<NoiseGateVisualizer>(proc, *this, getMutableNodeState());
    }

    //Allows visualizer to get value
    std::atomic<float>& getOutputLevelAtomic() {
        return gateDSP.currentOutputLevelDb;
    }

    ////////////////////////////////////////////////////////////  reyna

    // clone node
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();                      // Copy ValueTree state
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);   // new uuid for clone

        auto* self = const_cast<NoiseGateNode*>(this);                                           // to access processor ref
        auto clonePtr = std::make_shared<NoiseGateNode>(self->processor);                        // create new GainNode
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr); // copy state

        // Keep clone in processor state tree
        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName); // name will be made unique in daisychian
        return clonePtr;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    //nodes own dsp processor + reference to main processor for param access
    AudioPluginAudioProcessor& processor;
    NoiseGateProcessor gateDSP;
};