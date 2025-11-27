// Written by Austin Hills
// Redone to use local ValueTree state instead of APVTS

#include "Pitchblade/panels/CompressorPanel.h"
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"

//Set up of UI components
CompressorPanel::CompressorPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, CompressorNode* nodePtr) : processor(proc), localState(state), node(nodePtr)
{
    //label names for dials - reyna
    thresholdSlider.setName("Threshold");
    ratioSlider.setName("Ratio");
    attackSlider.setName("Attack");
    releaseSlider.setName("Release");
   

    // Label
    compressorLabel.setText("Compressor", juce::dontSendNotification);
    addAndMakeVisible(compressorLabel);
    compressorLabel.setName("NodeTitle");
    
    //  volumemeter - reyna
    volumeMeter = std::make_unique<SimpleVolumeBar>(
        [this]() -> float {
            //post processing value
            if (!node) return -60.0f;
            return node->getOutputLevelAtomic().load();
        },
        //placeholder for pre prossessing volume level
        [this]() -> float { 
            if (!node) return -60.0f;
            return node->getPriorLevelAtomic().load(); 
        }
    );
    addAndMakeVisible(volumeMeter.get());

    //custom toggle for limiter - reyna
    static CustomLookAndFeel gSwitchLF;
    modeButton.setButtonText("Limiter Mode");
    modeButton.setClickingTogglesState(true);
    addAndMakeVisible(modeButton);

    // Set initial button state from local state
    const bool startLimiterMode = (bool)localState.getProperty("CompLimiterMode", false);
    modeButton.setToggleState(startLimiterMode, juce::dontSendNotification);
    
    // On click, update the local state property
    modeButton.onClick = [this]() {
        localState.setProperty("CompLimiterMode", modeButton.getToggleState(), nullptr);
    };

    // make small dials for bottom row - reyna
    static SmallDialLookAndFeel smallDialLF;
    thresholdSlider.setLookAndFeel(&smallDialLF);
    releaseSlider.setLookAndFeel(&smallDialLF);

    // Threshold slider
    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    thresholdSlider.setNumDecimalPlacesToDisplay(1);
    thresholdSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(thresholdSlider);
    
    // Ratio slider
    ratioSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    ratioSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
    ratioSlider.setNumDecimalPlacesToDisplay(1);
    ratioSlider.setTextValueSuffix(" : 1");
    addAndMakeVisible(ratioSlider);
    
    // Attack slider
    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
    attackSlider.setNumDecimalPlacesToDisplay(1);
    attackSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(attackSlider);   

    // Release slider
    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    releaseSlider.setNumDecimalPlacesToDisplay(1);
    releaseSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(releaseSlider);

    ///////////////////
    // Link sliders to local state properties
    const float startThresholdDb = (float)localState.getProperty("CompThreshold", 0.0f);
    thresholdSlider.setRange(-100.0f, 0.0f, 0.1f);
    thresholdSlider.setValue(startThresholdDb, juce::dontSendNotification);
    thresholdSlider.onValueChange = [this]() {
        localState.setProperty("CompThreshold", (float)thresholdSlider.getValue(), nullptr);
        };

    const float startRatio = (float)localState.getProperty("CompRatio", 3.0f);
    ratioSlider.setRange(1.0f, 20.0f, 0.1f);
    ratioSlider.setValue(startRatio, juce::dontSendNotification);
    ratioSlider.onValueChange = [this]() {
        localState.setProperty("CompRatio", (float)ratioSlider.getValue(), nullptr);
        };
    
    const float startAttackMs = (float)localState.getProperty("CompAttack", 50.0f);
    attackSlider.setRange(1.0f, 200.0f, 1.0f);
    attackSlider.setValue(startAttackMs, juce::dontSendNotification);
    attackSlider.onValueChange = [this]() {
        localState.setProperty("CompAttack", (float)attackSlider.getValue(), nullptr);
        };
    
    const float startReleaseMs = (float)localState.getProperty("CompRelease", 250.0f);
    releaseSlider.setRange(10.0f, 1000.0f, 1.0f);
    releaseSlider.setValue(startReleaseMs, juce::dontSendNotification);
    releaseSlider.onValueChange = [this]() {
        localState.setProperty("CompRelease", (float)releaseSlider.getValue(), nullptr);
        };
    
    // Add this panel as a listener to the local state
    localState.addListener(this);

    //Initializing visibility of sliders
    updateSliderVisibility();
};

void CompressorPanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);

    if (volumeMeter) {
        float threshold = (float)localState.getProperty("CompThreshold", -20.0f);
        volumeMeter->setThresholdDecibels(threshold);
    }
}

void CompressorPanel::place(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label, bool useCustomLF) {
    slider.setBounds(area.reduced(10));
    label.setBounds(area.removeFromBottom(20));
}

CompressorPanel::~CompressorPanel() {
    if (localState.isValid())
        localState.removeListener(this);
}

void CompressorPanel::resized() {
    auto area = getLocalBounds();

    //panel label
    compressorLabel.setBounds(area.removeFromTop(30));

    auto r = area.reduced(10, 6);

    // volume meter
    auto meterArea = r.removeFromLeft(80);
    meterArea.removeFromTop(20);         
    meterArea.removeFromBottom(10);
    meterArea.removeFromLeft(20);
    volumeMeter->setBounds(meterArea);

    // left column - threshold and release
    auto leftCol = r.removeFromLeft(r.getWidth() * 0.30f);
    leftCol.translate(0, -20);
    int dialH = (leftCol.getHeight() / 2) ;

    auto thresholdArea = leftCol.removeFromTop(dialH).reduced(-15);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 80, 20);

    place(thresholdArea, thresholdSlider, thresholdLabel, true);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 80, 20);
    leftCol.translate(0,10);
    auto releaseArea = leftCol.reduced(-15); 
    place(releaseArea, releaseSlider, releaseLabel, true);

    // right side - limiter toggle, ratio and attack bottom
    auto rightCol = r.reduced(-8);
    rightCol.translate(-10, -2);
    // limiter toggle
    auto toggleArea = rightCol.removeFromTop(40);
    modeButton.setBounds(toggleArea.withTrimmedTop(-25).withSizeKeepingCentre(250, 40));

    // dials
    int dialW = (rightCol.getWidth() / 2);
    int dialH2 = (rightCol.getHeight());

    auto ratioArea = rightCol.removeFromLeft(dialW);
    place(ratioArea, ratioSlider, ratioLabel, true);

    auto attackArea = rightCol;
    place(attackArea, attackSlider, attackLabel, true);
}

void CompressorPanel::updateSliderVisibility()
{
    // Get the current state from the local state
    const bool isLimiter = (bool)localState.getProperty("CompLimiterMode", false);

    if (isLimiter) {
        modeButton.setButtonText("Compressor Mode");
    }
    else {
        modeButton.setButtonText("Limiter Mode");
    }

    // In limiter mode, grey the ratio and attack sliders - reyna
    const bool compressorActive = !isLimiter;
    auto greyOut = [compressorActive](juce::Slider& slider, juce::Label& label) {
            slider.setEnabled(compressorActive);
            label.setEnabled(compressorActive);
            if (compressorActive) {
                slider.setAlpha(1.0f);
                label.setAlpha(1.0f);
            } else {
                slider.setAlpha(0.4f);
                label.setAlpha(0.4f);
            }
        };
    greyOut(ratioSlider, ratioLabel);
    greyOut(attackSlider, attackLabel);
}

void CompressorPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == localState)
    {
        if (property == juce::Identifier("CompThreshold"))
            thresholdSlider.setValue((float)tree.getProperty("CompThreshold"), juce::dontSendNotification);
        else if (property == juce::Identifier("CompRatio"))
            ratioSlider.setValue((float)tree.getProperty("CompRatio"), juce::dontSendNotification);
        else if (property == juce::Identifier("CompAttack"))
            attackSlider.setValue((float)tree.getProperty("CompAttack"), juce::dontSendNotification);
        else if (property == juce::Identifier("CompRelease"))
            releaseSlider.setValue((float)tree.getProperty("CompRelease"), juce::dontSendNotification);
        else if (property == juce::Identifier("CompLimiterMode"))
        {
            modeButton.setToggleState((bool)tree.getProperty("CompLimiterMode"), juce::dontSendNotification);
            updateSliderVisibility();
        }
    }
}

CompressorVisualizer::~CompressorVisualizer(){
        //Stop listening
        if(localState.isValid()){
            localState.removeListener(this);
        }
    }

//Update the graph
void CompressorVisualizer::timerCallback(){
    float newDbLevel = compressorNode.getOutputLevelAtomic().load();

    //Push it to graph
    pushData(newDbLevel);

    //Call the graph visualizer's timerCallback
    RealTimeGraphVisualizer::timerCallback();
}

void CompressorVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    if(tree==localState && property == juce::Identifier("CompThreshold")){
        //Threshold slider changed, so update the line
        float newThreshold = (float)localState.getProperty("CompThreshold",0.0f);
        setThreshold(newThreshold,true);
    }
}

// XML serialization for saving/loading state - reyna 

std::unique_ptr<juce::XmlElement> CompressorNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("CompressorNode");
    xml->setAttribute("name", effectName);
    
    // Austin - Fixed the local state property names to camelCase
    xml->setAttribute("CompThreshold", (float)getNodeState().getProperty("CompThreshold", 0.0f));
    xml->setAttribute("CompRatio", (float)getNodeState().getProperty("CompRatio", 3.0f));
    xml->setAttribute("CompAttack", (float)getNodeState().getProperty("CompAttack", 50.0f));
    xml->setAttribute("CompRelease", (float)getNodeState().getProperty("CompRelease", 250.0f));
    xml->setAttribute("CompLimiterMode", (int)getNodeState().getProperty("CompLimiterMode", 0));
    
    return xml;
}

void CompressorNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();

    // Austin - Fixed the local state property names to camelCase
    s.setProperty("CompThreshold", (float)xml.getDoubleAttribute("CompThreshold", 0.0f), nullptr);
    s.setProperty("CompRatio", (float)xml.getDoubleAttribute("CompRatio", 3.0f), nullptr);
    s.setProperty("CompAttack", (float)xml.getDoubleAttribute("CompAttack", 50.0f), nullptr);
    s.setProperty("CompRelease", (float)xml.getDoubleAttribute("CompRelease", 250.0f), nullptr);
    s.setProperty("CompLimiterMode", (int)xml.getIntAttribute("CompLimiterMode", 0), nullptr);
}
