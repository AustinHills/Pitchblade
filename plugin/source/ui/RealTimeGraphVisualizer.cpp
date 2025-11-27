//Written by Austin Hills

#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

//Initializing by setting the parameters as defined
RealTimeGraphVisualizer::RealTimeGraphVisualizer(juce::AudioProcessorValueTreeState& vts, const juce::String& label, juce::Range<float> range, bool isLog, int numOfYAxisLabels)
    : apvts(vts)
{
    yAxisLabel = label;
    yAxisRange = range;
    isLogarithmic = isLog;
    numYAxisLabels = numOfYAxisLabels;

    //Listen to framerate parameter
    apvts.addParameterListener("GLOBAL_FRAMERATE",this);

    int initialIndex = *apvts.getRawParameterValue("GLOBAL_FRAMERATE");

    switch(initialIndex){
        case 1:
            startTimerHz(5);
            break;
        case 2:
            startTimerHz(15);
            break;
        case 3:
            startTimerHz(30);
            break;
        case 4:
            startTimerHz(60);
            break;
        default:
            startTimerHz(30);
            break;
    }
}

//Ensuring that the timer can't do anything if the visualizer is destroyed
RealTimeGraphVisualizer::~RealTimeGraphVisualizer(){
    stopTimer();
    apvts.removeParameterListener("GLOBAL_FRAMERATE",this);
}

//big function to handle all visual stuff
void RealTimeGraphVisualizer::paint(juce::Graphics& g){
    //Fill background
    g.fillAll(Colors::panel);
    
    //Draw labels
    drawLabels(g);

    //Save current graphics state and clip everything outside of the graph's bounds
    g.saveState();
    g.reduceClipRegion(graphBounds);

    //Draw graph. Lock the dataMutex to ensure the audio thread doesn't modify the queue while it is being read
    //Curly brackets are used to create a new, inner scope, where objects can be created inside and not exist outside of it
    {
        juce::ScopedLock lock(dataMutex);
        drawGraph(g);
    }

    //Draw the threshold line
    drawThresholdLine(g);
}

void RealTimeGraphVisualizer::resized(){
    auto bounds = getLocalBounds();

    //padding 
    constexpr int pad = 15;
    bounds = bounds.reduced(pad);

    labelBounds = bounds.removeFromLeft(labelWidth);

    graphBounds = bounds.reduced(0,5);

    maxDataPoints = graphBounds.getWidth();

    //Though the window cannot currently be resized, if it ever is able to be, then excess data points must be able to be pruned
    juce::ScopedLock lock(dataMutex);
    while(dataQueue.size() > maxDataPoints){
        dataQueue.pop_front();
    }

}

void RealTimeGraphVisualizer::timerCallback(){
    //Only repaint if the visualizer is actually visible, otherwise don't since it'll introduce unneeded processing time
    if(isShowing()){
        repaint();
    }
}

//This function is called from the audio thread to push new data in
void RealTimeGraphVisualizer::pushData(float newDataPoint){
    //Lock mutex to gain access
    juce::ScopedLock lock(dataMutex);

    //Add new data in the back of the queue
    dataQueue.push_back(newDataPoint);

    //Purge old data if the queue is now larger than graph width
    while(dataQueue.size() > maxDataPoints){
        dataQueue.pop_front();
    }

    //Mutex is unlocked automatically when it goes out of scope
}

//This function is called from the message thread when the threshold's slider value has changed
void RealTimeGraphVisualizer::setThreshold(float yValue, bool enabled){
    thresholdValue = yValue;
    thresholdEnabled = enabled;
}

//Draw labels
void RealTimeGraphVisualizer::drawLabels(juce::Graphics& g){
    g.setColour(Colors::buttonText);
    g.setFont(12.0f);

    //Draw border
    g.drawRect(graphBounds);

    //Draw y axis label (often dB)
    g.drawText(yAxisLabel,labelBounds.reduced(2),juce::Justification::centred,1);

    //Draws the remaining labels aside from the two above
    if(numYAxisLabels < 2){
        return;
    }else{
        for(int i = 0;i<numYAxisLabels;i++){
            //Calculate value for this label
            float proportion = (float)i / (float)(numYAxisLabels - 1);
            float value;

            //Logarithmic must have the start point be non-negative. Will default to linear otherwise
            if(isLogarithmic & (yAxisRange.getStart() >= 0)){
                float safeStart;
                if(yAxisRange.getStart()==0){
                    safeStart = 0.00001f;
                }else{
                    safeStart = yAxisRange.getStart();
                }

                float logStart = log10(safeStart);
                float logEnd = log10(yAxisRange.getEnd());
                value = pow(10.0f,logStart + proportion * (logEnd - logStart));
            }else{
                value = yAxisRange.getStart() + proportion * (yAxisRange.getEnd() - yAxisRange.getStart());
            }

            //Calculate the pixel position using the helper
            float y = mapValuetoY(value);

            //Formatting the text to make it nice for larger numbers
            //For instance 15423 would display as 15k. No decimals
            juce::String labelText;
            if(value >= 1000.0f){
                labelText = juce::String(value/1000.0,0) + "k";
            }else{
                labelText = juce::String(value,0);
            }

            g.drawText(labelText,labelBounds.getX(),y-6,labelWidth,12,juce::Justification::centredRight);
        }
    }

}

//Graph drawing function. The function assumes that the dataMutex is already locked
void RealTimeGraphVisualizer::drawGraph(juce::Graphics& g){
    //If there is nothing to draw, do not draw
    if(dataQueue.empty()){
        return;
    }

    juce::Path graphPath;
    juce::Path fillPath;        // added underneath fillpath to graph -reyna

    g.setColour(Colors::accent);

    //Get graph dimensions

    const float graphW = (float)graphBounds.getWidth();
    const float graphH = (float)graphBounds.getHeight();
    const float graphX = (float)graphBounds.getX();
    const float graphY = (float)graphBounds.getY();
    const float graphB = (float)graphBounds.getBottom();

    //Get the first data point to start the path
    float firstValue = dataQueue.front();

    //jmap maps a value from one range to another. The data value is mapped to the coordinate range
    //Please note that the pixel range is from bottom to top
    float startY = mapValuetoY(firstValue);

    //Start the path at the first data point at the left side of the graph
    graphPath.startNewSubPath(graphX,startY);

    fillPath.startNewSubPath(graphX, graphB);
    fillPath.lineTo(graphX, startY);

    //Calculate how many pixels wide each data point is
    float stepX = graphW / (float)maxDataPoints;
    float currentX = graphX + stepX;

    //Add remaining points to the path
    for(size_t i = 1; i < dataQueue.size(); i++){
        float value = dataQueue[i];
        float y = mapValuetoY(value);
        graphPath.lineTo(currentX,y);
        fillPath.lineTo(currentX, y);
        currentX+=stepX;
    }

    fillPath.lineTo(currentX - stepX, graphB);
    fillPath.closeSubPath();

    // fill gradient under the line - reyna
    juce::ColourGradient grad(
        Colors::accentPink.withAlpha(0.5f), graphBounds.getX(), graphBounds.getY(),
        Colors::panel.withAlpha(0.5f), graphBounds.getX(), graphBounds.getBottom(),
        false
    );

    g.setGradientFill(grad);
    g.fillPath(fillPath);

    //Draw the path to the screen with a 2 pixel stroke
    g.setColour(Colors::accent);
    g.strokePath(graphPath,juce::PathStrokeType(2.0f));
}

//Draw threshold line
void RealTimeGraphVisualizer::drawThresholdLine(juce::Graphics& g){
    //If there is nothing to draw, then do not draw
    if(!thresholdEnabled){
        return;
    }

    //Map the current threshold value to a y-pixel coordinate
    float lineY = mapValuetoY(thresholdValue);

    //Ensure that the value does not appear outside of the graph bounds, just in case
    lineY = juce::jlimit((float)graphBounds.getY(),(float)graphBounds.getBottom(),lineY);

    g.setColour(Colors::accent);

    //Define the dash pattern
    float dashes[] = {4.0f,4.0f};

    //Draw the dashed line across the graph horizontally
    g.drawDashedLine(juce::Line<float>((float)graphBounds.getX(),lineY,(float)graphBounds.getRight(),lineY),dashes,2);
}

//Helper function for mapping values
float RealTimeGraphVisualizer::mapValuetoY(float value) const{
    const float graphB = (float)graphBounds.getBottom();
    const float graphY = (float)graphBounds.getY();
    const float start = yAxisRange.getStart();
    const float end = yAxisRange.getEnd();

    //This requires the range to be positive, and it will not work if the start of the range is negative
    //For prevention of errors, if the range is negative, then it is treated as if it is linear
    if(isLogarithmic & (start >= 0)){
        float safeStart;
        float safeValue;
        if(start==0){
            safeStart = 0.00001f;
        }else{
            safeStart = start;
        }
        if(value==0){
            safeValue = 0.00001f;
        }else{
            safeValue = value;
        }

        float logStart = log10(safeStart);
        float logEnd = log10(end);
        float logValue = log10(safeValue);
        return juce::jmap(logValue, logStart, logEnd, graphB, graphY);
    }else{
        return juce::jmap(value,start,end,graphB,graphY);
    }
}

//If the user changes the FPS in the settings, change it
void RealTimeGraphVisualizer::parameterChanged(const juce::String& parameterID, float newValue){
    if(parameterID == "GLOBAL_FRAMERATE"){
        stopTimer();
        switch((int)newValue){
        case 1:
            startTimerHz(5);
            break;
        case 2:
            startTimerHz(15);
            break;
        case 3:
            startTimerHz(30);
            break;
        case 4:
            startTimerHz(60);
            break;
        default:
            startTimerHz(30);
            break;
    }
    }
}