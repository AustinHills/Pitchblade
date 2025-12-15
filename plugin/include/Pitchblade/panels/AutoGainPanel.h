// Written by Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/effects/AutoGainProcessor.h"

// Forward declaration
class AutoGainNode;

// Defining the UI panel /////////////////////////////////////////////
class AutoGainPanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    explicit AutoGainPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);
    ~AutoGainPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    juce::String panelTitle;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    // Reference back to main processor
    AudioPluginAudioProcessor& processor;

    // Sliders
    juce::Slider targetSlider, thresholdSlider, attackSlider, releaseSlider;

    // Labels
    juce::Label targetLabel, thresholdLabel, attackLabel, releaseLabel;

    juce::ValueTree localState;

    // Update visibility function (if needed)
    // void updateSliderVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoGainPanel)
};

// Creating visualizer node for Auto Gain
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

class AutoGainVisualizer : public RealTimeGraphVisualizer, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;
    AutoGainNode& autoGainNode;
    juce::ValueTree localState;
public:
    explicit AutoGainVisualizer(AudioPluginAudioProcessor& proc, AutoGainNode& node, juce::ValueTree& state)
        : RealTimeGraphVisualizer(proc.apvts, "dB", {-100.0f, 0.0f}, false, 6),
            processor(proc),
            autoGainNode(node),
            localState(state)
    {
        // Set initial threshold line
        float initialThreshold = (float)localState.getProperty("AutoGainThreshold", -40.0f);
        setThreshold(initialThreshold, true);

        // Listen for changes
        localState.addListener(this);
    }

    ~AutoGainVisualizer() override;

    // Update the graph
    void timerCallback() override;

    void paint(juce::Graphics& g) override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};

// Node Definition
#include "Pitchblade/panels/EffectNode.h"

class AutoGainNode : public EffectNode 
{
public:
    // Create node with name and reference to main processor
    explicit AutoGainNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "AutoGainNode", "Auto Gain"), processor(proc) { 
        // initialize default properties
        if (!getMutableNodeState().hasProperty("AutoGainTarget"))
            getMutableNodeState().setProperty("AutoGainTarget", -6.0f, nullptr);
        if (!getMutableNodeState().hasProperty("AutoGainThreshold"))
            getMutableNodeState().setProperty("AutoGainThreshold", -40.0f, nullptr);
        if (!getMutableNodeState().hasProperty("AutoGainAttack"))
            getMutableNodeState().setProperty("AutoGainAttack", 20.0f, nullptr);
        if (!getMutableNodeState().hasProperty("AutoGainRelease"))
            getMutableNodeState().setProperty("AutoGainRelease", 200.0f, nullptr);

        // ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        autoGainDSP.prepare(proc.getSampleRate());
    }

    AutoGainNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
        : EffectNode(proc, existingState), processor(proc) {}

    // dsp processing step
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {

        juce::ignoreUnused(proc);

        const float target = (float)getNodeState().getProperty("AutoGainTarget", -6.0f);
        const float threshold = (float)getNodeState().getProperty("AutoGainThreshold", -40.0f);
        const float attack = (float)getNodeState().getProperty("AutoGainAttack", 20.0f);
        const float release = (float)getNodeState().getProperty("AutoGainRelease", 200.0f);
        
        autoGainDSP.setTargetThroughput(target);
        autoGainDSP.setThreshold(threshold);
        autoGainDSP.setAttack(attack);
        autoGainDSP.setRelease(release);

        // Process the audio buffer
        autoGainDSP.process(buffer);

        // Calculate output level (post-process) for visualizer
        // The processor stores it internally too, but let's be consistent with other nodes 
        // that seem to push it to the DSP object or pull it.
        // EffectNode usually grabs it from buffer here.
        // Let's use the one in the DSP if it updates it.
        // Looking at GainNode, it calculates it here.
        // Looking at CompressorNode, it calculates Pre and Post.
        // AutoGainDSP updates currentOutputLevelDb inside its process method.
        // So we don't strictly need to recalculate it if we trust the DSP object.
        // But for consistency:
        // float peakAmplitude = buffer.getMagnitude(0,0,buffer.getNumSamples());
        // float levelDb = juce::Decibels::gainToDecibels(peakAmplitude,-100.0f);
        // autoGainDSP.currentOutputLevelDb.store(levelDb);
        // (The DSP already does this at the end of its process block)
    }

    // return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<AutoGainPanel>(proc, getMutableNodeState(), effectName);
    }

    // return visualizer 
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<AutoGainVisualizer>(proc, *this, getMutableNodeState());
    }

    // Allows visualizer to get the value
    std::atomic<float>& getOutputLevelAtomic(){
        return autoGainDSP.currentOutputLevelDb;
    }

    ////////////////////////////////////////////////////////////

    // clone node
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();                    // Copy ValueTree state
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr); // new uuid for clone

        auto* self = const_cast<AutoGainNode*>(this);                     // to access processor ref
        auto clonePtr = std::make_shared<AutoGainNode>(self->processor);  // create new AutoGainNode
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);    // copy state

        // Keep clone in processor state tree
        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName); 
        return clonePtr;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    AudioPluginAudioProcessor& processor;
    AutoGainProcessor autoGainDSP;
};
