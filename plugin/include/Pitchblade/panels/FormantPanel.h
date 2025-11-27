//hudas code
#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/effects/FormantDetector.h"
#include "Pitchblade/effects/FormantShifter.h"
#include "Pitchblade/ui/FormantVisualizer.h"

#ifndef PARAM_FORMANT_SHIFT
  #define PARAM_FORMANT_SHIFT "FORMANT_SHIFT"
#endif
#ifndef PARAM_FORMANT_MIX
  #define PARAM_FORMANT_MIX "FORMANT_MIX"
#endif

class FormantPanel : public juce::Component, public juce::ValueTree::Listener {
public:
    explicit FormantPanel(AudioPluginAudioProcessor& proc);
    FormantPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state);
    ~FormantPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // INTEGRATION TEST BUG: expose sliders so tests can validate APVTS wiring
    juce::Slider& getShiftSlider() { return formantSlider; }
    juce::Slider& getMixSlider()   { return mixSlider; }
    // ValueTree listener
    void valueTreePropertyChanged(juce::ValueTree& tree,
        const juce::Identifier& property) override;

private:
    AudioPluginAudioProcessor& processor;
    juce::Slider gainSlider;
    juce::Label panelTitle;

    // --- Formant Shifter controls
    juce::Label  formantLabel, mixLabel;
    juce::Slider formantSlider, mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttach, mixAttach;

    // node local state for this panel
    juce::ValueTree localState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormantPanel)
};

////////////////////////////////////////////////////////////

// reynas changes > added dsp node defn to ui panel creation
// formant dsp node , processes audio + makes own panel
// inherits from EffectNode base class
class FormantNode : public EffectNode {
public:
    FormantNode (AudioPluginAudioProcessor& proc) : EffectNode (proc, "FormantNode", "Formant"), processor (proc) {
        auto& state = getMutableNodeState();

        if (!state.hasProperty("FORMANT_SHIFT"))
            state.setProperty("FORMANT_SHIFT", 0.0f, nullptr);   // slider range  -50 to 50

        if (!state.hasProperty("FORMANT_MIX"))
            state.setProperty("FORMANT_MIX", 1.0f, nullptr);     // slider range  0 to 1
    }

    void process (AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {
        // --- 0) Grab params from this node's local state
        auto state = getNodeState();
        float shift = (float)state.getProperty("FORMANT_SHIFT", 0.0f);
        float mix = (float)state.getProperty("FORMANT_MIX", 1.0f);   // 0 dry, 1 wet

        // If this node is bypassed, force neutral formant and fully dry
        if (bypassed) {
            shift = 0.0f;
            mix   = 0.0f;
        }

        // Get shifter + detector
        auto& sh = proc.getFormantShifter();
        auto& det = proc.getFormantDetector();

        // Make sure shifter sees the current amount
        sh.setShiftAmount (shift);   // [-50..50] -> internal ratio

        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        if (numCh == 0 || numSamples == 0)
            return;

        // Copy dry input
        dryBuffer.setSize (numCh, numSamples, false, false, true);
        dryBuffer.makeCopyOf (buffer);

        // Process in-place to get the wet signal
        sh.processBlock (buffer); // buffer = wet

        // Keep a copy of the fully-wet output for visualization regardless of Dry/Wet mix
        wetBuffer.setSize (numCh, numSamples, false, false, true);
        wetBuffer.makeCopyOf (buffer);

        //Crossfade dry/wet into buffer
        const float wetGain  = juce::jlimit (0.0f, 1.0f, mix);
        const float dryGain  = 1.0f - wetGain;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* dry = dryBuffer.getReadPointer (ch);
            float* wet = buffer.getWritePointer (ch);

            for (int n = 0; n < numSamples; ++n)
                wet[n] = dryGain * dry[n] + wetGain * wet[n];
        }

        // Analyze the fully wet (shifted) output for the visualizer so motion is obvious
        det.processBlock (wetBuffer);
        auto freqsWet = det.getFormantFrequencies();

        std::sort (freqsWet.begin(), freqsWet.end());
        freqsWet.erase (std::remove_if (freqsWet.begin(), freqsWet.end(),
                                        [] (float f) { return f < 300.f || f > 5000.f; }),
                        freqsWet.end());
        if (freqsWet.size() > 3)
            freqsWet.resize (3);

        proc.setLatestFormants (freqsWet);
    }

    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        juce::ignoreUnused(proc);
        return std::make_unique<FormantPanel>(proc, getMutableNodeState());
    }


    // Provide a visualizer for the bottom VisualizerPanel area
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override
    {
        return std::make_unique<FormantVisualizer>(proc, proc.apvts);
    }

    std::shared_ptr<EffectNode> clone() const override
    {
        return std::make_shared<FormantNode> (processor);
    }

    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml (const juce::XmlElement& xml) override;

private:
    AudioPluginAudioProcessor& processor;

    //buffer to hold the dry input for dry/wet mixing
    juce::AudioBuffer<float> dryBuffer;
    // Visualization-only: wet copy for formant detection (independent of mix)
    juce::AudioBuffer<float> wetBuffer;
};
