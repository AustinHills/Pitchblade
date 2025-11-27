//Austin Hills

#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

//Initializing by setting parameters as defined
FrequencyGraphVisualizer::FrequencyGraphVisualizer(juce::AudioProcessorValueTreeState& vts, int numYLabels, int dispMode)
    : apvts(vts)
{
    displayMode = dispMode;

    numYAxisLabels = numYLabels;

    //Log transformed frequency range
    logFreqStart = log10(xAxisRange.getStart());
    logFreqEnd = log10(xAxisRange.getEnd());

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

//Ensuring that timer can't do anything if visualizer is destroyed
FrequencyGraphVisualizer::~FrequencyGraphVisualizer()
{
    stopTimer();
    apvts.removeParameterListener("GLOBAL_FRAMERATE", this);
}

//Big function to handle all of the visual stuff
void FrequencyGraphVisualizer::paint(juce::Graphics& g){
    // Fill background
    g.fillAll(Colors::panel);

    // Draw labels and grid
    drawLabels(g);

    // Save current graphics state and clip everything outside of the graph's bounds
    g.saveState();
    g.reduceClipRegion(graphBounds);

    // Draw graph. Lock the dataMutex to ensure data isn't modified
    {
        juce::ScopedLock lock(dataMutex);
        
        drawSpectrum(g);

        //If displayMode is set to secondary spectrum, draw that one as well
        if (displayMode == 2)
        {
            drawSecondarySpectrum(g);
        }
    }

    // If displayMode is set to threshold lines, draw those
    if (displayMode == 1)
    {
        drawThresholdLines(g);
    }
}

void FrequencyGraphVisualizer::resized(){
    auto bounds = getLocalBounds();

    //padding 
    constexpr int pad = 15;
    bounds = bounds.reduced(pad);

    xLabelBounds = bounds.removeFromBottom(labelHeight);
    yLabelBounds = bounds.removeFromLeft(labelWidth);
    graphBounds = bounds.reduced(0, 5);
}

void FrequencyGraphVisualizer::timerCallback()
{
    // Only repaint if the visualizer is actually visible
    if (isShowing())
    {
        repaint();
    }
}

//This function is called from the audio thread to update the spectrum data
void FrequencyGraphVisualizer::updateSpectrumData(const std::vector<juce::Point<float>>& newData)
{
    juce::ScopedLock lock(dataMutex);
    spectrumData = newData;

    //Enveloping because the data looked weird in its raw state

    //At start or if size change happens, copy the data
    if(displayedSpectrumData.size() != spectrumData.size()){
        displayedSpectrumData = spectrumData;
        // return;
    }

    //Get frame-independent coefficient
    float timerIntervalMs = (float)getTimerInterval();
    float accelCoeff = juce::jlimit(0.0f,1.0f,timerIntervalMs/accelerationTimeMs);

    for(int i = 0; i < spectrumData.size(); i++){
        float targetY = spectrumData[i].getY();
        float currentY = displayedSpectrumData[i].getY();

        float newY = currentY + (targetY - currentY) * accelCoeff;

        displayedSpectrumData[i].setXY(spectrumData[i].getX(), newY);
    }


}

//Same as above but for the (optional) secondary spectrum
void FrequencyGraphVisualizer::updateSecondarySpectrumData(const std::vector<juce::Point<float>>& newData)
{
    juce::ScopedLock lock(dataMutex);
    secondarySpectrumData = newData;

    //Enveloping because the data looked weird in its raw state

    //At start or if size change happens, copy the data
    if(displayedSecondarySpectrumData.size() != secondarySpectrumData.size()){
        displayedSecondarySpectrumData = secondarySpectrumData;
        // return;
    }

    //Get frame-independent coefficient
    float timerIntervalMs = (float)getTimerInterval();
    float accelCoeff = juce::jlimit(0.0f,1.0f,timerIntervalMs/accelerationTimeMs);

    for(int i = 0; i < secondarySpectrumData.size(); i++){
        float targetY = secondarySpectrumData[i].getY();
        float currentY = displayedSecondarySpectrumData[i].getY();

        float newY = currentY + (targetY - currentY) * accelCoeff;

        displayedSecondarySpectrumData[i].setXY(secondarySpectrumData[i].getX(), newY);
    }
}

//This is called from the message thread when the threshold slider values have changed
void FrequencyGraphVisualizer::setThreshold(float frequency, float amplitude)
{
    xThreshold = frequency;
    yThreshold = amplitude;
}

//Draw labels and grids
void FrequencyGraphVisualizer::drawLabels(juce::Graphics& g)
{
    g.setColour(Colors::buttonText);
    g.setFont(10.0f);

    // Draw border
    g.drawRect(graphBounds);

    // Draw y axis label
    g.drawText("dB", yLabelBounds, juce::Justification::centred, 1);

    // Draw x axis label
    g.drawText("hz", xLabelBounds, juce::Justification::centred, 1);

    // Draw y-axis labels
    if (numYAxisLabels >= 2)
    {
        for (int i = 0; i < numYAxisLabels; i++)
        {
            //Calculate value for this label
            float proportion = (float)i / (float)(numYAxisLabels - 1);
            float value = yAxisRange.getStart() + proportion * (yAxisRange.getEnd() - yAxisRange.getStart());
            float y = mapAmpToY(value);

            juce::String labelText = juce::String(value, 0);

            // Use yLabelBounds for y-label positioning
            g.drawText(labelText, yLabelBounds.getX(), y - 6, labelWidth, 12, juce::Justification::centredRight);
        }
    }

    //Draw x-axis labels and lines
    //Iterate through the lines defined in the header. Make a vertical line for each, but only label "important" ones
    for(int i = 0; i < freqGridLines.size(); i++){
        float freq = freqGridLines[i];
        float x = mapFreqToX(freq);
        
        //Vertical line
        g.setColour(Colors::buttonText.withAlpha(0.5f));
        g.drawVerticalLine(juce::roundToInt(x), (float)graphBounds.getY(), (float)graphBounds.getBottom());

        if(freq == 100 || freq == 1000 || freq == 10000){
            juce::String labelText;
            
            if(freq >= 1000.0f){
                labelText = juce::String(freq/1000.0,0) + "k";
            }else{
                labelText = juce::String(freq,0);
            }

            g.setColour(Colors::buttonText);
            g.drawText(labelText, x - 20, xLabelBounds.getY(), 40, xLabelBounds.getHeight(), juce::Justification::centred);
        }
    }
}

//Graph drawing function. The function assumes that the dataMutex is already locked
void FrequencyGraphVisualizer::drawSpectrum(juce::Graphics& g){
    //If there is nothing to draw, do not draw
    if(displayedSpectrumData.empty()){
        return;
    }

    g.setColour(Colors::accent);
    juce::Path spectrumPath;

    //No duplicate pixels version. When multiple values want to be on the same x pixel, this averages them out
    int lastXPixel = -1;
    float ySum = 0.0f;
    int yCount = 0;

    for(int i = 0; i < displayedSpectrumData.size(); i++){
        float freq = displayedSpectrumData[i].getX();
        float amp = displayedSpectrumData[i].getY();

        //Only process points within the frequency range
        if(freq >= xAxisRange.getStart() && freq <= xAxisRange.getEnd()){
            float x = mapFreqToX(freq);
            float y = mapAmpToY(amp);
            int currentXPixel = (int)x;

            if(i == 0){
                //If this is the first data point, just graph it
                spectrumPath.startNewSubPath(x,y);
                lastXPixel = currentXPixel;
                ySum = y;
                yCount = 1;
            }else if (currentXPixel != lastXPixel){
                //If this is a new pixel, then draw the average of the past pixel's points
                if(yCount > 0){
                    spectrumPath.lineTo(x,ySum/(float)yCount);
                }else{
                    spectrumPath.lineTo(x,y);
                }
                
                lastXPixel = currentXPixel;
                ySum = y;
                yCount = 1;
            }else{
                //Same pixel as before. Add to the sum so it can be averaged
                ySum += y;
                yCount++;
            }
        }
    }

    //Draw last point
    if(yCount > 0){
        spectrumPath.lineTo((float)lastXPixel,ySum / (float)yCount);
    }

    //Draw path with a 2 pixel stroke
    g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));

    // add gradient under graph - reyna /////////
    {
        juce::Path fillPath;
        float bottom = graphBounds.getBottom();
        float left = graphBounds.getX();

        // start bottom left
        fillPath.startNewSubPath(left, bottom);
        int lastX = -1;
        float accY = 0.0f;
        int accCount = 0;

        for (int i = 0; i < displayedSpectrumData.size(); i++) {
            float freq = displayedSpectrumData[i].getX();
            float amp = displayedSpectrumData[i].getY();
            if (freq >= xAxisRange.getStart() && freq <= xAxisRange.getEnd()) {
                float x = mapFreqToX(freq);
                float y = mapAmpToY(amp);
                int px = (int)x;
                if (i == 0) {
                    fillPath.lineTo(x, y);
                    lastX = px;
                    accY = y;
                    accCount = 1;
                } else if (px != lastX) {
                    float averagedY = (accCount > 0 ? accY / (float)accCount : y);
                    fillPath.lineTo(x, averagedY);

                    lastX = px;
                    accY = y;
                    accCount = 1;
                } else {
                    accY += y;
                    accCount++;
                }
            }
        }

        if (accCount > 0)
            fillPath.lineTo((float)lastX, accY / (float)accCount);

        // close at bottom
        fillPath.lineTo((float)lastX, bottom);
        fillPath.closeSubPath();

        // gradient
        juce::ColourGradient grad(
            Colors::accentPink.withAlpha(0.35f), graphBounds.getX(), graphBounds.getY(),
            Colors::panel.withAlpha(0.35f), graphBounds.getX(), graphBounds.getBottom(),
            false
        );

        g.setGradientFill(grad);
        g.fillPath(fillPath);
    } 
}

//Another graph drawing function. The function assumes that the dataMutex is already locked
void FrequencyGraphVisualizer::drawSecondarySpectrum(juce::Graphics& g){
    //If there is nothing to draw, do not draw
    if(displayedSecondarySpectrumData.empty()){
        return;
    }

    g.setColour(Colors::accent.withAlpha(0.5f));
    juce::Path spectrumPath;

    //No duplicate pixels version. When multiple values want to be on the same x pixel, this averages them out
    int lastXPixel = -1;
    float ySum = 0.0f;
    int yCount = 0;

    for(int i = 0; i < displayedSecondarySpectrumData.size(); i++){
        float freq = displayedSecondarySpectrumData[i].getX();
        float amp = displayedSecondarySpectrumData[i].getY();

        //Only process points within the frequency range
        if(freq >= xAxisRange.getStart() && freq <= xAxisRange.getEnd()){
            float x = mapFreqToX(freq);
            float y = mapAmpToY(amp);
            int currentXPixel = (int)x;

            if(i == 0){
                //If this is the first data point, just graph it
                spectrumPath.startNewSubPath(x,y);
                lastXPixel = currentXPixel;
                ySum = y;
                yCount = 1;
            }else if (currentXPixel != lastXPixel){
                //If this is a new pixel, then draw the average of the past pixel's points
                if(yCount > 0){
                    spectrumPath.lineTo(x,ySum/(float)yCount);
                }else{
                    spectrumPath.lineTo(x,y);
                }
                
                lastXPixel = currentXPixel;
                ySum = y;
                yCount = 1;
            }else{
                //Same pixel as before. Add to the sum so it can be averaged
                ySum += y;
                yCount++;
            }
        }
    }

    //Draw last point
    if(yCount > 0){
        spectrumPath.lineTo((float)lastXPixel,ySum / (float)yCount);
    }

    //Draw path with a 2 pixel stroke
    g.strokePath(spectrumPath,juce::PathStrokeType(2.0f));
}

//Draw threshold lines
void FrequencyGraphVisualizer::drawThresholdLines(juce::Graphics& g){
    //Only draw if threshold mode is selected
    if(displayMode==1){
        //Map threshold values to y pixel coordinates
        float lineX = mapFreqToX(xThreshold);
        float lineY = mapAmpToY(yThreshold);

        //Ensure values do not appear outside of graph bounds just in case
        lineX = juce::jlimit((float)graphBounds.getX(), (float)graphBounds.getRight(), lineX);
        lineY = juce::jlimit((float)graphBounds.getY(), (float)graphBounds.getBottom(), lineY);

        g.setColour(Colors::accent);

        //Define the dash pattern
        float dashes[] = {4.0f,4.0f};
        
        //Draw the dashed lines across the graph
        g.drawDashedLine(juce::Line<float>((float)graphBounds.getX(), lineY, (float)graphBounds.getRight(), lineY), dashes, 2);
        g.drawDashedLine(juce::Line<float>(lineX, (float)graphBounds.getY(), lineX, (float)graphBounds.getBottom()), dashes, 2);
    }
}

//Helper function for mapping to the y axis
float FrequencyGraphVisualizer::mapAmpToY(float amp) const{
    const float graphB = (float)graphBounds.getBottom();
    const float graphY = (float)graphBounds.getY();
    const float start = yAxisRange.getStart();
    const float end = yAxisRange.getEnd();

    //Just linear so we return values in the jmap
    return juce::jmap(amp, start, end, graphB, graphY);
}

//Helper function for mapping to the x axis
float FrequencyGraphVisualizer::mapFreqToX(float freq) const{
    const float graphX = (float)graphBounds.getX();
    const float graphR = (float)graphBounds.getRight();

    //Lots simpler than the other one since the range is fixed and starts at 20
    return juce::jmap(log10(freq),logFreqStart,logFreqEnd,graphX,graphR);
}

//If the user changes the FPS in the settings, change it
void FrequencyGraphVisualizer::parameterChanged(const juce::String& parameterID, float newValue){
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