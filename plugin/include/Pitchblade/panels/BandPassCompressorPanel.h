// Written by Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/BandPassCompressorProcessor.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

// Forward declaration
class BandPassCompressorNode;

// Visualizer with custom dual-threshold lines
class BandPassCompressorVisualizer : public FrequencyGraphVisualizer, public juce::ValueTree::Listener
{
private:
    BandPassCompressorNode& node;
    juce::ValueTree localState;
    AudioPluginAudioProcessor& processor;
    
    void updateThresholds();

public:
    explicit BandPassCompressorVisualizer(AudioPluginAudioProcessor& proc, BandPassCompressorNode& nodeRef, juce::ValueTree& state);
    ~BandPassCompressorVisualizer() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};

// Main Panel
class BandPassCompressorPanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    explicit BandPassCompressorPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle);
    ~BandPassCompressorPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

private:
    AudioPluginAudioProcessor& processor;
    juce::ValueTree localState;
    juce::String panelTitle;

    // Parameters
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider;
    juce::Slider minFreqSlider, maxFreqSlider;
    juce::ToggleButton listenButton { "Listen" };

    juce::Label titleLabel;
    // Labels handled by sliders now
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandPassCompressorPanel)
};

// Node
class BandPassCompressorNode : public EffectNode
{
public:
    explicit BandPassCompressorNode(AudioPluginAudioProcessor& proc);
    BandPassCompressorNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState);

    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override;
    
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override;
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override;
    std::shared_ptr<EffectNode> clone() const override;

    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

    BandPassCompressorProcessor& getDSP() { return dsp; }

private:
    AudioPluginAudioProcessor& processor;
    BandPassCompressorProcessor dsp;
    double lastSampleRate = 0.0;
};
