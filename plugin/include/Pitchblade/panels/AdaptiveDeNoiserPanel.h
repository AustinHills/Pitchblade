//Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/effects/AdaptiveDeNoiserProcessor.h"

//UI panel class
//Time is included to manage the 2 second learning countdown
class AdaptiveDeNoiserPanel : public juce::Component, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;

    //Slider
    juce::Slider reductionSlider;
    juce::Slider thresholdSlider;

    //Labels
    juce::Label deNoiserLabel;

    juce::ValueTree localState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdaptiveDeNoiserPanel)
public:
    explicit AdaptiveDeNoiserPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);
    ~AdaptiveDeNoiserPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    juce::String panelTitle;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};

//Visualizer
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

class AdaptiveDeNoiserNode;
//Not fully implemented. Just shows a placeholder
class AdaptiveDeNoiserVisualizer : public FrequencyGraphVisualizer, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;
    AdaptiveDeNoiserNode& deNoiserNode;
    juce::ValueTree localState;
public:
    explicit AdaptiveDeNoiserVisualizer(AudioPluginAudioProcessor& proc, AdaptiveDeNoiserNode& node, juce::ValueTree& state)
        : FrequencyGraphVisualizer(proc.apvts, 5,2),
            processor(proc),
            deNoiserNode(node),
            localState(state)
    {
        //Nothing yet
    }

    ~AdaptiveDeNoiserVisualizer() override;

    void timerCallback() override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    void paint(juce::Graphics& g) override;
};


//Node
#include "Pitchblade/panels/EffectNode.h"

class AdaptiveDeNoiserNode : public EffectNode{
public:
    //Create node with name and reference to main processor
    explicit AdaptiveDeNoiserNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "DeNoiserNode", "De-Noiser"), processor(proc) {
        if(!getMutableNodeState().hasProperty("DenoiserReduction"))
            getMutableNodeState().setProperty("DenoiserReduction",0.5f,nullptr);
        if(!getMutableNodeState().hasProperty("DenoiserThreshold"))
            getMutableNodeState().setProperty("DenoiserThreshold",-60.0f,nullptr);

        //Ensure EffectNodes tree exists
        if(!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        deNoiserDSP.prepare(proc.getSampleRate());
    }

    AdaptiveDeNoiserNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
        : EffectNode(proc, existingState), processor(proc) {}

    //DSP processing step for denoiser
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(proc);

        const float reduction = (float)getNodeState().getProperty("DenoiserReduction", 0.5f);
        const float threshold = (float)getNodeState().getProperty("DenoiserThreshold", -60.0f);

        deNoiserDSP.setReduction(reduction);
        deNoiserDSP.setThreshold(threshold);

        deNoiserDSP.process(buffer);
    }

    //return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<AdaptiveDeNoiserPanel>(proc,getMutableNodeState(), effectName);
    }

    //return visualizer
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<AdaptiveDeNoiserVisualizer>(proc, *this, getMutableNodeState());
    }

    //clone node
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();
        copiedTree.setProperty("uuid",juce::Uuid().toString(),nullptr);

        auto* self = const_cast<AdaptiveDeNoiserNode*>(this);
        auto clonePtr = std::make_shared<AdaptiveDeNoiserNode>(self->processor);
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree,nullptr);

        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(),-1,nullptr);
        clonePtr->setDisplayName(effectName);
        return clonePtr;
    }

    AdaptiveDeNoiserProcessor& getDSP(){
        return deNoiserDSP;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;
private:
    AudioPluginAudioProcessor& processor;
    AdaptiveDeNoiserProcessor deNoiserDSP;
};