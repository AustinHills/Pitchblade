//Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/effects/DeClickerProcessor.h"

// 1. Panel Class
class DeClickerPanel : public juce::Component, public juce::ValueTree::Listener {
private:
    AudioPluginAudioProcessor& processor;
    juce::ValueTree localState;
    juce::String panelTitle;

    // UI Elements
    juce::Label titleLabel;

    juce::Slider sensitivitySlider;
    juce::Slider historySlider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeClickerPanel)

public:
    explicit DeClickerPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);
    ~DeClickerPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};


// 2. Node Class
#include "Pitchblade/panels/EffectNode.h"

class DeClickerNode : public EffectNode {
public:
    explicit DeClickerNode(AudioPluginAudioProcessor& proc);
    DeClickerNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState);

    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override;

    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override;
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override;
    std::shared_ptr<EffectNode> clone() const override;

    DeClickerProcessor& getDSP() { return dsp; }

    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
    AudioPluginAudioProcessor& processor;
    DeClickerProcessor dsp;
};


// 3. Visualizer Class
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

class DeClickerVisualizer : public FrequencyGraphVisualizer, public juce::ValueTree::Listener {
private:
    DeClickerNode& node;
    juce::ValueTree localState;

public:
    DeClickerVisualizer(AudioPluginAudioProcessor& proc, DeClickerNode& nodeRef, juce::ValueTree& state);
    ~DeClickerVisualizer() override;

    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    void paint(juce::Graphics& g) override;
};
