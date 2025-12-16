//Austin Hills

#pragma once

#include <vector>
#include <cmath>
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"

//This visualizer displays amplitude over frequency
class FrequencyGraphVisualizer : public juce::Component, public juce::Timer, public juce::AudioProcessorValueTreeState::Listener{
private:
    //Draws y axis labels, x axis labels, grid, and borders
    void drawLabels(juce::Graphics& g);

    //Builds a path from spectrumData and draws it
    void drawSpectrum(juce::Graphics& g);

    //Builds a path from secondarySpectrumData and draws it
    void drawSecondarySpectrum(juce::Graphics& g);

    //Draws horizontal and vertical threshold lines
    void drawThresholdLines(juce::Graphics& g);

    //Data stuff
    //Since x axis matters more now, I decided to use points. Each point is (frequency, amplitude)
    std::vector<juce::Point<float>> spectrumData;
    std::vector<juce::Point<float>> secondarySpectrumData;

    //Data storage for smoothed values (graph was jumping too much and it was annoying to look at)
    std::vector<juce::Point<float>> displayedSpectrumData;
    std::vector<juce::Point<float>> displayedSecondarySpectrumData;

    //Envelope time in milliseconds for the data lag to make the graph look smoother
    const float accelerationTimeMs = 50;

    //Mutex to protect data vectors
    juce::CriticalSection dataMutex;

    //Display mode. 0 means no special things. 1 means horizontal and vertical threshold lines. 2 means secondary spectrum
    int displayMode = 0;

    //Ranges for axes
    const juce::Range<float> yAxisRange {-100.0f, 0.0f};
    const juce::Range<float> xAxisRange {20.0f,20000.0f};

    //Threshold values
    float xThreshold = 6000.0f;
    float yThreshold = -20.0f;

    //Pixel area for graph line
    juce::Rectangle<int> graphBounds;

    //Pixel area for y-axis labels
    juce::Rectangle<int> yLabelBounds;
    juce::Rectangle<int> xLabelBounds;

    //Fixed width and height in pixels for the label areas
    const int labelWidth = 40;
    const int labelHeight = 20;

    //Cached log values for mapping
    float logFreqStart, logFreqEnd;

    //Helper function for mapping values to y axis
    float mapAmpToY(float amp) const;

    //Helper function for mapping values to x axis
    float mapFreqToX(float freq) const;

    //Number of y axis labels to draw
    int numYAxisLabels = 4;

    //Frequencies to draw grid lines at
    const std::vector<float> freqGridLines = {20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000};

    //Value tree state
    juce::AudioProcessorValueTreeState& apvts;
public:
    //Constructor
    FrequencyGraphVisualizer(juce::AudioProcessorValueTreeState& vts, int numYLabels = 5, int dispMode = 0);

    ~FrequencyGraphVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    //Called at framerate interval, triggers repaint()
    void timerCallback() override;

    //Public API stuff
    //Pushes spectrum info to the point vectors
    void updateSpectrumData(const std::vector<juce::Point<float>>& newData);
    void updateSecondarySpectrumData(const std::vector<juce::Point<float>>& newData);

    //Set threshold lines
    void setThreshold(float frequency, float amplitude);

    //Update FPS
    void parameterChanged(const juce::String& parameterID, float newValue) override;
};
