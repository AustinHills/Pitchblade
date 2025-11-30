#pragma once
// Author: huda
// reyna updated to new valuetree value system and nodebased system
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"
// Visualizer lives separately (VisualizerPanel tabs)
#include "Pitchblade/ui/EqualizerVisualizer.h"

// ===================== Panel (UI) =====================
class EqualizerPanel : public juce::Component, public juce::ValueTree::Listener {
public:
    //explicit EqualizerPanel (AudioPluginAudioProcessor& proc);
    EqualizerPanel(AudioPluginAudioProcessor& p, juce::ValueTree& state, const juce::String& nodeTitle);

    // display panel
    void paint(juce::Graphics& g) override;
    void resized() override;
    juce::String panelTitle;

    // destructor
    ~EqualizerPanel() override;

private:
    static void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text, double min, double max, double step, bool isGain);

    AudioPluginAudioProcessor& processor;
    juce::ValueTree localState;  // valuetree for node permaters

    // No embedded visualizer; knobs-only panel

    juce::Slider lowFreq,  lowGain,
                 midFreq,  midGain,
                 highFreq, highGain;

    juce::Label  lowFreqLabel,  lowGainLabel,
                 midFreqLabel,  midGainLabel,
                 highFreqLabel, highGainLabel;
    juce::Label equalizerLabel;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
};

class EqualizerNode : public EffectNode
{
public:
    // create node with default apvts and register under EffectNodes
    explicit EqualizerNode(AudioPluginAudioProcessor& proc)
        : EffectNode(proc, "EqualizerNode", "Equalizer")
    {
        juce::ValueTree st("EqualizerNode");
        st.setProperty("LowFreq", 200.0f, nullptr);
        st.setProperty("LowGain", 0.0f, nullptr);
        st.setProperty("MidFreq", 1000.0f, nullptr);
        st.setProperty("MidGain", 0.0f, nullptr);
        st.setProperty("HighFreq", 6000.0f, nullptr);
        st.setProperty("HighGain", 0.0f, nullptr);
        st.setProperty("uuid", juce::Uuid().toString(), nullptr);

        // make sure the global tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        // attach this new tree to EffectNodes as a new child
        processor.apvts.state.addChild(st, -1, nullptr);

        // assign this new unique state to this node
        nodeState = st;
    }

    // use node state for the panel
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<EqualizerPanel>(proc, getMutableNodeState(), effectName);
    }

    // Provide a visualizer component for VisualizerPanel
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor&) override {
        try { return std::make_unique<EqualizerVisualizer>(processor); }
        catch (...) { return nullptr; }
    }

    // keep existing DSP path 
    // push local state into the DSP and process
   void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
    // Do not read ValueTree properties on the audio thread (not thread-safe).
    // The UI updates the Equalizer parameters via thread-safe setters when knobs move.
    proc.getEqualizer().processBlock(buffer);
}

    //reynas daisychain and presets stuff /////////////////////////////////////////

    // clone
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);

        auto* self = const_cast<EqualizerNode*>(this);
        auto clonePtr = std::make_shared<EqualizerNode>(self->processor);
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);

        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName);
        clonePtr->bypassed = bypassed;
        return clonePtr;
    }

    // save to xml
    std::unique_ptr<juce::XmlElement> toXml() const override {
        auto xml = std::make_unique<juce::XmlElement>("EqualizerNode");
        xml->setAttribute("name", effectName);

        const auto& st = getNodeState();
        xml->setAttribute("LowFreq", (float)st.getProperty("LowFreq", 200.0f));
        xml->setAttribute("LowGain", (float)st.getProperty("LowGain", 0.0f));
        xml->setAttribute("MidFreq", (float)st.getProperty("MidFreq", 1000.0f));
        xml->setAttribute("MidGain", (float)st.getProperty("MidGain", 0.0f));
        xml->setAttribute("HighFreq", (float)st.getProperty("HighFreq", 6000.0f));
        xml->setAttribute("HighGain", (float)st.getProperty("HighGain", 0.0f));
        return xml;
    }

    // load from xml 
    void loadFromXml(const juce::XmlElement& xml) override {
        auto& st = getMutableNodeState();
        st.setProperty("LowFreq", (float)xml.getDoubleAttribute("LowFreq", 200.0), nullptr);
        st.setProperty("LowGain", (float)xml.getDoubleAttribute("LowGain", 0.0), nullptr);
        st.setProperty("MidFreq", (float)xml.getDoubleAttribute("MidFreq", 1000.0), nullptr);
        st.setProperty("MidGain", (float)xml.getDoubleAttribute("MidGain", 0.0), nullptr);
        st.setProperty("HighFreq", (float)xml.getDoubleAttribute("HighFreq", 6000.0), nullptr);
        st.setProperty("HighGain", (float)xml.getDoubleAttribute("HighGain", 0.0), nullptr);
    }
};
