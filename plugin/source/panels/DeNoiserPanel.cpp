//Austin Hills

#include "Pitchblade/panels/DeNoiserPanel.h"
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"

DeNoiserPanel::DeNoiserPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
                : processor(proc), localState(state), panelTitle(nodeTitle) {

    //Label
    deNoiserLabel.setText(panelTitle, juce::dontSendNotification);
    addAndMakeVisible(deNoiserLabel);
    deNoiserLabel.setName("NodeTitle");

    //Learn button
    addAndMakeVisible(learnButton);

    //Clicking triggers learning and starts timer
    learnButton.onClick = [this]() {
        if(currentState == 0){
            //Go from idle to counting down
            currentState = 1;

            //Tell processor to start learning
            localState.setProperty("DenoiserLearn",true,nullptr);

            //Set up count down
            countdownTimeRemaining = learnDuration;

            //Update UI for learning mode
            statusLabel.setText("Learning. Do not make noise.", juce::dontSendNotification);
            statusLabel.setVisible(true);
            learnButton.setEnabled(false);
            learnButton.setButtonText("Learning... " + juce::String(countdownTimeRemaining,1));

            //Start the timer
            startTimer(100);
        }
    };

    //New status label
    statusLabel.setText("Learning. Do not make noise.", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    statusLabel.setVisible(false);

    //Reduction slider
    reductionSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    reductionSlider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,25);
    reductionSlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(reductionSlider);

    //Reduction label
    reductionLabel.setText("Reduction Intensity",juce::dontSendNotification);
    reductionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reductionLabel);

    //Link sliders to local state properties
    const float startReduction = (float)localState.getProperty("DenoiserReduction",0.5f);
    reductionSlider.setRange(0.0,1.0f,0.01f);
    reductionSlider.setValue(startReduction,juce::dontSendNotification);
    reductionSlider.onValueChange = [this]() {
        localState.setProperty("DenoiserReduction",(float)reductionSlider.getValue(),nullptr);
        };

    //Add this panel as a listener to the local state
    localState.addListener(this);
}

DeNoiserPanel::~DeNoiserPanel(){
    stopTimer();
    if(localState.isValid()){
        localState.removeListener(this);
    }
}

void DeNoiserPanel::paint(juce::Graphics& g){
    g.drawRect(getLocalBounds(),2);
}

void DeNoiserPanel::resized(){
    auto area = getLocalBounds();

    //Title label at the top
    deNoiserLabel.setBounds(area.removeFromTop(50));
    learnButton.setBounds(area.removeFromTop(30).reduced(20,-5));

    //Bounds for status label below the button
    statusLabel.setBounds(area.removeFromTop(30));

    auto dials = area.reduced(10);

    //Center the single dial
    int dialWidth = dials.getWidth() / 3;
    dials.removeFromLeft(dialWidth);
    auto reductionArea = dials.removeFromLeft(dialWidth).reduced(5);

    //Positioning reduction label and slider
    reductionLabel.setBounds(reductionArea.removeFromTop(10));
    reductionSlider.setBounds(reductionArea);
}

void DeNoiserPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    if(tree == localState){
        if(property==juce::Identifier("DenoiserReduction")){
            reductionSlider.setValue((float)tree.getProperty("DenoiserReduction"),juce::dontSendNotification);
        }
    }
}

//Timer stuff
void DeNoiserPanel::timerCallback(){
    if(currentState == 1){
        countdownTimeRemaining -= 0.1f;

        //Update stuff if countdown is done
        if(countdownTimeRemaining <= 0.0f){
            stopTimer();

            localState.setProperty("DenoiserLearn",false,nullptr);

            statusLabel.setText("Noise profile captured",juce::dontSendNotification);
            learnButton.setButtonText("Re-learn Noise Profile");
            learnButton.setEnabled(true);

            //Move state to captured
            currentState = 2;
            countdownTimeRemaining = messageDuration;
            startTimer(100);
        }else{
            //still counting
            learnButton.setButtonText("Learning... " + juce::String(countdownTimeRemaining,1));
        }
    }else if (currentState == 2){
        //Waiting for captured message to time out
        countdownTimeRemaining -= 0.1f;

        //Message time out
        if(countdownTimeRemaining <= 0.0f){
            stopTimer();
            statusLabel.setVisible(false);
            //Back to idle
            currentState = 0;
        }
    }
}

//Visualizer stuff
DeNoiserVisualizer::~DeNoiserVisualizer(){
    if(localState.isValid()){
        localState.removeListener(this);
    }
}

void DeNoiserVisualizer::timerCallback(){
    //Get data from processor
    auto spectrum = deNoiserNode.getDSP().getSpectrumData();
    auto noise = deNoiserNode.getDSP().getNoiseProfileData();

    //Push data to visualizer
    updateSpectrumData(spectrum);
    updateSecondarySpectrumData(noise);

    FrequencyGraphVisualizer::timerCallback();
}

void DeNoiserVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    juce::ignoreUnused(tree, property);
}

void DeNoiserVisualizer::paint(juce::Graphics& g){
    FrequencyGraphVisualizer::paint(g);
}

// XML serialization - Austin
std::unique_ptr<juce::XmlElement> DeNoiserNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("DeNoiserNode");
    xml->setAttribute("name", effectName);
    xml->setAttribute("DenoiserReduction", (float)getNodeState().getProperty("DenoiserReduction", 0.0f));
    return xml;
}

void DeNoiserNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();
    s.setProperty("DenoiserReduction", (float)xml.getDoubleAttribute("DenoiserReduction", 0.0f), nullptr);
}