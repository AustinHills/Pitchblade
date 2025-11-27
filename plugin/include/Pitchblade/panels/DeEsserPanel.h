// Written by Austin Hills
// Refactored to use local ValueTree state

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/DeEsserProcessor.h"

class DeEsserPanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    explicit DeEsserPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state);
    ~DeEsserPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    //Reference to main processor
    AudioPluginAudioProcessor& processor;

    //Sliders
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider, frequencySlider;

    //Labels
    juce::Label deEsserLabel, thresholdLabel, ratioLabel, attackLabel, releaseLabel, frequencyLabel;

    juce::ValueTree localState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeEsserPanel)
};

//Visualizer Node for DeEsser
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

class DeEsserNode;

class DeEsserVisualizer : public FrequencyGraphVisualizer, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;
    DeEsserNode& deEsserNode;
    juce::ValueTree localState;

    // Helper to set threshold lines from state
    void updateThresholdLines(){
        const float freq = (float)localState.getProperty("DeEsserFrequency", 6000.0f);
        const float thresh = (float)localState.getProperty("DeEsserThreshold", 0.0f);
        setThreshold(freq, thresh);
    }

public:
    explicit DeEsserVisualizer(AudioPluginAudioProcessor& proc, DeEsserNode& node, juce::ValueTree& state)
        : FrequencyGraphVisualizer(proc.apvts, 5, 1),
            processor(proc),
            deEsserNode(node),
            localState(state)
    {
        localState.addListener(this);
        updateThresholdLines();
    }

    ~DeEsserVisualizer() override;

    void timerCallback() override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    void paint(juce::Graphics& g) override;
};

//Austin
class DeEsserNode : public EffectNode
{
public:
    //Create node with name and reference to main processor
    explicit DeEsserNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "DeEsserNode", "De-Esser"), processor(proc){
        //initialize default properties
        if (!getMutableNodeState().hasProperty("DeEsserThreshold"))
            getMutableNodeState().setProperty("DeEsserThreshold", 0.0f, nullptr);
        if (!getMutableNodeState().hasProperty("DeEsserRatio"))
            getMutableNodeState().setProperty("DeEsserRatio", 4.0f, nullptr);
        if (!getMutableNodeState().hasProperty("DeEsserAttack"))
            getMutableNodeState().setProperty("DeEsserAttack", 5.0f, nullptr);
        if (!getMutableNodeState().hasProperty("DeEsserRelease"))
            getMutableNodeState().setProperty("DeEsserRelease", 5.0f, nullptr);
        if (!getMutableNodeState().hasProperty("DeEsserFrequency"))
            getMutableNodeState().setProperty("DeEsserFrequency", 6000.0f, nullptr);
        
        //Ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        //Add this node to processor state tree
        processor.apvts.state.addChild(getMutableNodeState(), -1, nullptr);

        //Preparing the de-esser
        deEsserDSP.prepare(proc.getSampleRate(),proc.getCurrentBlockSize());
    }

    //dsp processing step for de-esser
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override{
        juce::ignoreUnused(proc);

        const float threshold = (float)getNodeState().getProperty("DeEsserThreshold", 0.0f);
        const float ratio = (float)getNodeState().getProperty("DeEsserRatio", 4.0f);
        const float attack = (float)getNodeState().getProperty("DeEsserAttack", 5.0f);
        const float release = (float)getNodeState().getProperty("DeEsserRelease", 5.0f);
        const float frequency = (float)getNodeState().getProperty("DeEsserFrequency", 6000.0f);

        deEsserDSP.setThreshold(threshold);
        deEsserDSP.setRatio(ratio);
        deEsserDSP.setAttack(attack);
        deEsserDSP.setRelease(release);
        deEsserDSP.setFrequency(frequency);

        deEsserDSP.process(buffer);
    }

    //return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override
    {
        return std::make_unique<DeEsserPanel>(proc, getMutableNodeState());
    }

    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override{
        return std::make_unique<DeEsserVisualizer>(proc,*this,getMutableNodeState());
    }

    ////////////////////////////////////////////////////////  reyna

    //clone node
    std::shared_ptr<EffectNode> clone() const override
    {
        auto copiedTree = getNodeState().createCopy();
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);

        auto* self = const_cast<DeEsserNode*>(this);
        auto clonePtr = std::make_shared<DeEsserNode>(self->processor);
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);

        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName);
        return clonePtr;
    }

    DeEsserProcessor& getDSP(){
        return deEsserDSP;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    //nodes own dsp processor + reference to main processor for param access
    AudioPluginAudioProcessor& processor;
    DeEsserProcessor deEsserDSP;
};