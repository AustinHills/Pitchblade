// austins code
#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"

//gain panel ui
// creates slider n attaches to apvts parameter (in pluginprocessor.cpp)
class GainPanel : public juce::Component, public juce::ValueTree::Listener {
public:
    explicit GainPanel(AudioPluginAudioProcessor& proc);

	// overloaded constructor with local state
    GainPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);

	// destructor
    ~GainPanel() override;

	// valueTree listener callback
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    void resized() override;
    void paint(juce::Graphics&) override; 
    juce::String panelTitle;

private:
    //declaring slider n processor
    AudioPluginAudioProcessor& processor;
    //ui
    juce::Slider gainSlider;
    juce::Label gainLabel;

	juce::ValueTree localState; // local state for this panel

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainPanel)
};

//Visualizer stuff - Austin
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

class GainNode;

class GainVisualizer : public RealTimeGraphVisualizer
{
public:
    explicit GainVisualizer(AudioPluginAudioProcessor& proc, GainNode& node)
        : RealTimeGraphVisualizer(proc.apvts, "dB", {-100.0f, 0.0f}, false, 6),
          processor(proc),
          gainNode(node)
    {    }
    ~GainVisualizer() {}
    // Update the graph
    void timerCallback() override;
private:
    AudioPluginAudioProcessor& processor;
    GainNode& gainNode;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainVisualizer)
};


////////////////////////////////////////////////////////////

//reynas changes > added dsp node defn to ui panel creation
// gain dsp node , processes audio + makes own panel
// inherits from EffectNode base class
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/GainProcessor.h"

class GainNode : public EffectNode {
public:
    explicit GainNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "GainNode", "Gain"), processor(proc) {   
        // initialize default properties
        if (!getMutableNodeState().hasProperty("Gain"))
            getMutableNodeState().setProperty("Gain", 0.0f, nullptr);
		// ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");
		// add this node to processor state tree
        processor.apvts.state.addChild(getMutableNodeState(), -1, nullptr);
    }

	// dsp read from local state instead of apvts
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(proc);
        const float gainDb = (float)getNodeState().getProperty("Gain", 0.0f);
        gainDSP.setGain(gainDb);
        gainDSP.process(buffer);

        //Calculate output level for visualizer
        float peakAmplitude = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        float levelDb = juce::Decibels::gainToDecibels(peakAmplitude, -100.0f);
        gainDSP.currentOutputLevelDb.store(levelDb);
    }

    // return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
		return std::make_unique<GainPanel>(proc, getMutableNodeState(), effectName); // pass local state
    }

    // return visualizer 
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<GainVisualizer>(proc,*this);
    }

    //Allows visualizer to get value
    std::atomic<float>& getOutputLevelAtomic(){
        return gainDSP.currentOutputLevelDb;
    }

    //////////////////////////////////////////////  reyna

	// clone node with copied state
    std::shared_ptr<EffectNode> clone() const override
	{   
        auto copiedTree = getNodeState().createCopy();                      // Copy ValueTree state
		copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);   // new uuid for clone

		auto* self = const_cast<GainNode*>(this);                                           // to access processor ref
		auto clonePtr = std::make_shared<GainNode>(self->processor);                        // create new GainNode
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
    GainProcessor gainDSP;
};