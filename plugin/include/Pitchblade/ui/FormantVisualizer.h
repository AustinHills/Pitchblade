#pragma once

#include <JuceHeader.h>
#include <vector>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

/*
==============================================================================
    FormantVisualizer
    Visualizes detected formant frequencies as vertical markers over a log-frequency axis.
    Pulls latest formants from the processor and repaints at the global framerate.

    Author: Huda Noor
==============================================================================
*/

class FormantVisualizer : public juce::Component,
                          private juce::Timer,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    FormantVisualizer(AudioPluginAudioProcessor& processorRef,
                      juce::AudioProcessorValueTreeState& vts);
    ~FormantVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Timer to drive UI refreshes based on GLOBAL_FRAMERATE
    void timerCallback() override;

    // Listen to GLOBAL_FRAMERATE changes
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Data/Config
    AudioPluginAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Child components
    std::unique_ptr<FrequencyGraphVisualizer> freqGraph; // background grid/axes

    // Overlay draws the formant markers on top of the grid
    class FormantOverlay : public juce::Component
    {
    public:
        explicit FormantOverlay(AudioPluginAudioProcessor& procRef,
                                juce::AudioProcessorValueTreeState& vts)
            : proc(procRef), apvtsRef(vts) {}

        void paint(juce::Graphics& g) override;

        // Align with FrequencyGraphVisualizer layout (left and bottom label areas)
        static constexpr int labelWidth  = 40;
        static constexpr int labelHeight = 20;

    private:
        AudioPluginAudioProcessor& proc;
        juce::AudioProcessorValueTreeState& apvtsRef;
        // Visible window tailored to detected formants.
        const juce::Range<float> visibleXAxisHz { 300.0f, 5000.0f };
        float logVisibleStart = std::log10(visibleXAxisHz.getStart());
        float logVisibleEnd  = std::log10(visibleXAxisHz.getEnd());

        float mapVisibleFreqToX(float freq, juce::Rectangle<int> graph) const;
        float mapXToVisibleFreq(float x, juce::Rectangle<int> graph) const;

        // Visualization state to accentuate motion
        std::vector<float> lastFormants;
        bool hasLast = false;
        float pulse = 0.0f;
    };

    std::unique_ptr<FormantOverlay> overlay;
};
