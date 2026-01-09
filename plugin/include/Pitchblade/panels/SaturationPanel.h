// Written by Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/LevelMeter.h"

class SaturationNode;

class SaturationPanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    ~SaturationPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    juce::String panelTitle;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    explicit SaturationPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, SaturationNode* nodePtr, const juce::String& nodeTitle);

private:
    AudioPluginAudioProcessor& processor;
    
    juce::Slider driveSlider, mixSlider;
    juce::Label saturationLabel, driveLabel, mixLabel;

    SaturationNode* node = nullptr;

    juce::ValueTree localState;

    // Helper for placement
    static void place(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SaturationPanel)
};

// Visualizer includes
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

// Forward decl
class SaturationNode;

class SaturationVisualizer : public RealTimeGraphVisualizer, public juce::ValueTree::Listener {
private:
    AudioPluginAudioProcessor& processor;
    SaturationNode& saturationNode;
    juce::ValueTree localState;
public:
    explicit SaturationVisualizer(AudioPluginAudioProcessor& proc, SaturationNode& node, juce::ValueTree& state)
        : RealTimeGraphVisualizer(proc.apvts, "dB", {-100.0f, 0.0f}, false, 6),
          processor(proc),
          saturationNode(node),
          localState(state)
    {
        // No specific threshold line for saturation, but maybe we can show something else?
        // Compressor shows threshold. Saturation doesn't really have a "threshold" line in the same way.
        // We will just listen for changes.
        localState.addListener(this);
    }

    ~SaturationVisualizer() override;

    void timerCallback() override;
    
    // We don't really have properties that change the graph lines (like threshold), but keeping listener for consistency
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override {}
};

#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/SaturationProcessor.h"

class SaturationNode : public EffectNode
{
public:
    explicit SaturationNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "SaturationNode", "Saturation"), processor(proc) {
        if (!getMutableNodeState().hasProperty("SatDrive"))
            getMutableNodeState().setProperty("SatDrive", 0.0f, nullptr);
        if (!getMutableNodeState().hasProperty("SatMix"))
            getMutableNodeState().setProperty("SatMix", 1.0f, nullptr);
        
        // Ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        saturationDSP.prepare(proc.getSampleRate());
    }

    SaturationNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
        : EffectNode(proc, existingState), processor(proc) {
            saturationDSP.prepare(proc.getSampleRate());
        }

    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
        juce::ignoreUnused(proc);

        const float drive = (float)getNodeState().getProperty("SatDrive", 0.0f);
        const float mix = (float)getNodeState().getProperty("SatMix", 1.0f);

        saturationDSP.setDrive(drive);
        saturationDSP.setMix(mix);

        saturationDSP.process(buffer);

        // Post-measure
        float peakAmplitude = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        float levelDb = juce::Decibels::gainToDecibels(peakAmplitude, -100.0f);
        saturationDSP.currentOutputLevelDb.store(levelDb);
    }

    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<SaturationPanel>(proc, getMutableNodeState(), this, effectName);
    }

    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<SaturationVisualizer>(proc, *this, getMutableNodeState());
    }

    std::atomic<float>& getOutputLevelAtomic() {
        return saturationDSP.currentOutputLevelDb;
    }

    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);

        auto* self = const_cast<SaturationNode*>(this);
        auto clonePtr = std::make_shared<SaturationNode>(self->processor);
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);

        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName);
        return clonePtr;
    }

    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    AudioPluginAudioProcessor& processor;
    SaturationProcessor saturationDSP;
};
