// EQ response visualizer using FrequencyGraphVisualizer
#pragma once

#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

// Renders the static EQ frequency response curve for the current Equalizer settings
class EqualizerVisualizer : public juce::Component, private juce::Timer {
public:
    explicit EqualizerVisualizer(AudioPluginAudioProcessor& proc);
    ~EqualizerVisualizer() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // INTEGRATION TEST BUG: expose response data and manual update for tests.
    void forceUpdateForTest();
    std::vector<juce::Point<float>> getLastResponsePoints() const;

private:
    void timerCallback() override;
    void updateResponseCurve();

    AudioPluginAudioProcessor& processor;
    std::unique_ptr<FrequencyGraphVisualizer> graph; // draws frequency vs dB
    std::vector<juce::Point<float>> lastResponse;
    mutable juce::CriticalSection responseLock;

    // Overlay to replace the y-axis labels with [-24, +24] dB
    class YAxisLabelOverlay : public juce::Component {
    public:
        void paint(juce::Graphics& g) override;
    };
    std::unique_ptr<YAxisLabelOverlay> yLabels;

    // Helper to build log-spaced frequency array between 20 Hz and 20 kHz
    static void buildLogFrequencies(std::vector<float>& freqs, int numPoints);
};
