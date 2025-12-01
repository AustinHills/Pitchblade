//Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/effects/DeNoiserProcessor.h"

//UI panel class
//Time is included to manage the 2 second learning countdown
class DeNoiserPanel : public juce::Component, public juce::ValueTree::Listener, public juce::Timer{
private:
    AudioPluginAudioProcessor& processor;

    //Slider
    juce::Slider reductionSlider;

    //Button for learning
    juce::TextButton learnButton {"Learn Noise Profile"};

    //Labels
    juce::Label deNoiserLabel, reductionLabel, statusLabel;

    juce::ValueTree localState;

    //This is used to indicate to the statusLabel what the state of the noise profile is
    int currentState = 0;

    //Handling learning duration stuff
    float countdownTimeRemaining = 2.0f;
    const float learnDuration = 2.0f;
    const float messageDuration = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeNoiserPanel)
public:
    explicit DeNoiserPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);
    ~DeNoiserPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    juce::String panelTitle;

    //Callback for countdown timer
    void timerCallback() override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};

//Visualizer
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

class DeNoiserNode;
//Not fully implemented. Just shows a placeholder
class DeNoiserVisualizer : public FrequencyGraphVisualizer, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;
    DeNoiserNode& deNoiserNode;
    juce::ValueTree localState;
public:
    explicit DeNoiserVisualizer(AudioPluginAudioProcessor& proc, DeNoiserNode& node, juce::ValueTree& state)
        : FrequencyGraphVisualizer(proc.apvts, 5,2),
            processor(proc),
            deNoiserNode(node),
            localState(state)
    {
        //Nothing yet
    }

    ~DeNoiserVisualizer() override;

    void timerCallback() override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    void paint(juce::Graphics& g) override;
};


//Node
#include "Pitchblade/panels/EffectNode.h"

class DeNoiserNode : public EffectNode{
public:
    //Create node with name and reference to main processor
    explicit DeNoiserNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "DeNoiserNode", "De-Noiser"), processor(proc) {
        if(!getMutableNodeState().hasProperty("DenoiserReduction"))
            getMutableNodeState().setProperty("DenoiserReduction",0.5f,nullptr);
        if(!getMutableNodeState().hasProperty("DenoiserLearn"))
            getMutableNodeState().setProperty("DenoiserLearn",false,nullptr);

        //Ensure EffectNodes tree exists
        if(!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        //Add this node to processor state tree
        processor.apvts.state.addChild(getMutableNodeState(),-1,nullptr);

        deNoiserDSP.prepare(proc.getSampleRate());
    }

    //DSP processing step for denoiser
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(proc);

        const float reduction = (float)getNodeState().getProperty("DenoiserReduction", 0.5f);
        const bool isLearning = (bool)getNodeState().getProperty("DenoiserLearn", false);

        deNoiserDSP.setReduction(reduction);
        deNoiserDSP.setLearning(isLearning);

        deNoiserDSP.process(buffer);
    }

    //return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<DeNoiserPanel>(proc,getMutableNodeState(), effectName);
    }

    //return visualizer
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<DeNoiserVisualizer>(proc, *this, getMutableNodeState());
    }

    //clone node
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();
        copiedTree.setProperty("uuid",juce::Uuid().toString(),nullptr);

        auto* self = const_cast<DeNoiserNode*>(this);
        auto clonePtr = std::make_shared<DeNoiserNode>(self->processor);
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree,nullptr);

        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(),-1,nullptr);
        clonePtr->setDisplayName(effectName);
        return clonePtr;
    }

    DeNoiserProcessor& getDSP(){
        return deNoiserDSP;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;
private:
    AudioPluginAudioProcessor& processor;
    DeNoiserProcessor deNoiserDSP;
};