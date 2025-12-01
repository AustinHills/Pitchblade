#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

/*
==============================================================================
    EqualizerVisualizer 
    - renders the EQ frequency response using FrequencyGraphVisualizer.
    - It samples current Equalizer parameters, builds a log-spaced magnitude curve,
    overlays custom dB labels, and refreshes on a timer for the visualizer pane.

    Author: Huda Noor
==============================================================================
*/

// Renders the static EQ frequency response curve for the current Equalizer settings
class EqualizerVisualizer : public juce::Component, private juce::Timer {
public:
    // Build the visualizer and hook it to the processor's Equalizer settings
    explicit EqualizerVisualizer(AudioPluginAudioProcessor& proc);
    ~EqualizerVisualizer() override;

    // Lay out the graph and overlay
    void resized() override;
    // Container paint is delegated to children
    void paint(juce::Graphics& g) override;

    // INTEGRATION TEST BUG: expose response data and manual update for tests.
    // Force recomputation for deterministic testing
    void forceUpdateForTest();
    // Fetch the last rendered response points (thread-safe copy)
    std::vector<juce::Point<float>> getLastResponsePoints() const;

private:
    // Periodically refresh response data from the EQ parameters
    void timerCallback() override;
    // Rebuild response curve points and push them to the graph
    void updateResponseCurve();

    AudioPluginAudioProcessor& processor;
    std::unique_ptr<FrequencyGraphVisualizer> graph; // draws frequency vs dB
    std::vector<juce::Point<float>> lastResponse;
    mutable juce::CriticalSection responseLock;

    // Overlay to replace the y-axis labels with [-24, +24] dB
    class YAxisLabelOverlay : public juce::Component {
    public:
        // Draw custom dB labels over the graph's left margin
        void paint(juce::Graphics& g) override;
    };
    std::unique_ptr<YAxisLabelOverlay> yLabels;

    // Helper to build log-spaced frequency array between 20 Hz and 20 kHz
    static void buildLogFrequencies(std::vector<float>& freqs, int numPoints);
};
