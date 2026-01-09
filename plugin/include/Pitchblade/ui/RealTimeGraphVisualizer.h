//Written by Austin

#pragma once

#include <JuceHeader.h>
#include <deque>
//deque essentially has a lot of the same functionality as vectors, but it has better performance when adding or removing elements from the front. It is designed for queue-like behavior
#include "Pitchblade/ui/ColorPalette.h"


//This visualizer class is a generic visualizer for any data stream. Its primary functionality is as a first-in, first-out queue, displaying a fixed number of recent data points
//It uses a timer to gather new data and repaint itself on a regular interval
//Its y-axis is customizable, and it has an optional horizontal dotted line, which can be used to display user-defined thresholds.
class RealTimeGraphVisualizer : public juce::Component, public juce::Timer, public juce::AudioProcessorValueTreeState::Listener{
private:
    //Draws y-axis labels and bounding box for the graph
    void drawLabels(juce::Graphics& g);

    //Draws graph path from the data queue
    void drawGraph(juce::Graphics& g);

    //Draws the threshold line
    void drawThresholdLine(juce::Graphics& g);

    //Data queue. As described in the include (line 6), this is more efficient for a queue being used for a scrolling graph than a vector would be
    std::deque<float> dataQueue;

    //A mutex to protect the dataQueue. This prevents race conditions that occur when this reads a value at the same time as the processor pushing this value
    //Furthermore, apparently JUCE uses two main threads, with one being the audio thread, which handles everything in the processblock,
    //and a message thread, which handles everything else. The audio thread is initialized by the DAW
    //I didn't think OS would help me this quickly. Never used threads before that class, and now I need to deal with them the very next semester
    juce::CriticalSection dataMutex;

    //Maximum number of data points stored in the queue. This is equal to the pixel width of the graph drawing area
    int maxDataPoints = 0;

    //Text for y-axis
    juce::String yAxisLabel;

    //Range for y-axis
    juce::Range<float> yAxisRange;

    //Threshold enabled
    bool thresholdEnabled = false;

    //Threshold value
    float thresholdValue = 0.0f;

    //Pixel area for the graph line
    juce::Rectangle<int> graphBounds;

    //Pixel area for y-axis labels
    juce::Rectangle<int> labelBounds;

    //Fixed width in pixels for the label area
    const int labelWidth = 40;

    //Boolean determining if the graph will follow a logarithmic or linear scale
    bool isLogarithmic = false;

    //Helper function for mapping values to y axis depending on scale mode (linear vs logarithmic)
    float mapValuetoY(float value) const;

    //Number of y axis labels to draw
    int numYAxisLabels = 4;

    //Padding for data points to maintain consistent scroll speed
    int pointsPerPush = 1;

    //Value tree state
    juce::AudioProcessorValueTreeState& apvts;
public:
    //Constructor. Label is the text next to the y-axis. Range is the minimum and maximum values for the y-axis. Update interval is the refresh rate in hz
    RealTimeGraphVisualizer(juce::AudioProcessorValueTreeState& vts, const juce::String& label, juce::Range<float> range,bool isLog = false, int numOfYAxisLabels = 5);
    ~RealTimeGraphVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    //This function is called at the framerate, and it triggers a repaint()
    void timerCallback() override;

    //Public API stuff
    //Pushes a new data point into the queue for the visualizer
    void pushData(float newDataPoint);

    //Sets a horizontal dotted line at a specific y-value. Can be hidden by setting enabled to false
    void setThreshold(float yValue, bool enabled = true);

    //Update FPS
    void parameterChanged(const juce::String& parameterID, float newValue) override;
};