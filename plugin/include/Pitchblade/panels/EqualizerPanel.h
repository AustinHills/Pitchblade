#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/ui/EqualizerVisualizer.h"

/*
==============================================================================
    EqualizerPanel 
    - hosts six knobs (low/mid/high freq + gain) bound to the node's
    ValueTree state and thread-safe EQ setters, handling UI layout and state sync.

    Author: Huda Noor

   ************************************************************************** 
   
    EqualizerNode
    - wires this panel into the node system and provides the visualizer.

    Author: Reyna Macabebe
==============================================================================
*/

// ===================== Panel (UI) Author: Huda =====================
class EqualizerPanel : public juce::Component, public juce::ValueTree::Listener {
public:
    //explicit EqualizerPanel (AudioPluginAudioProcessor& proc);
    // Build knob-only EQ panel and bind to the node's ValueTree state
    EqualizerPanel(AudioPluginAudioProcessor& p, juce::ValueTree& state, const juce::String& nodeTitle);

    // display panel
    // Draw simple outline and title
    void paint(juce::Graphics& g) override;
    // Layout six knobs and labels across two rows
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

/*
======================================================================================
    Reynas changes
    - EqualizerNode owns the EQ DSP hookup, ValueTree state, panel creation, visualizer,
    cloning, and preset serialization for the EQ effect within the node-based chain
    - inherits from EffectNode base class
======================================================================================
*/
class EqualizerNode : public EffectNode
{
public:
    // create node with default apvts and register under EffectNodes
    explicit EqualizerNode(AudioPluginAudioProcessor& proc)
        : EffectNode(proc, "EqualizerNode", "Equalizer"), processor(proc)
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
    }

    EqualizerNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
        : EffectNode(proc, existingState), processor(proc) {}

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
    // Apply EQ using current parameters (already updated via UI setters)
    // Do not read ValueTree properties on the audio thread (not thread-safe).
    // The UI updates the Equalizer parameters via thread-safe setters when knobs move.
    proc.getEqualizer().processBlock(buffer);
}

    //reynas daisychain and presets changes /////////////////////////////////////////

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
private:
    AudioPluginAudioProcessor& processor;
};
