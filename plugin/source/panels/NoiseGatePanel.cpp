// austin

#include "Pitchblade/panels/NoiseGatePanel.h"
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"

//noise gate panel display
NoiseGatePanel::NoiseGatePanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle) 
                                : processor(proc), localState(state), panelTitle(nodeTitle) {
    //label names for dials - reyna
    thresholdSlider.setName("Threshold");
    attackSlider.setName("GateAttack");
    releaseSlider.setName("Release");

    // Label
    noiseGateLabel.setText(panelTitle, juce::dontSendNotification);
    addAndMakeVisible(noiseGateLabel);
    noiseGateLabel.setName("NodeTitle");

    // Threshold slider
    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    //Set the isReadOnly flag to false to allow user to edit - Austin
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);

	// reynas changes - adding value tree functionality
    thresholdSlider.setRange(-100.0f, 0.0f, 0.1f);
    thresholdSlider.setValue((float)localState.getProperty("GateThreshold", -100.0f), juce::dontSendNotification); // read from tree
    thresholdSlider.onValueChange = [this]() { // NEW: write to tree
        localState.setProperty("GateThreshold", (float)thresholdSlider.getValue(), nullptr);
        };

      //Added these two to make them more nice looking and obvious for what they are - Austin
    thresholdSlider.setNumDecimalPlacesToDisplay(1);
    thresholdSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(thresholdSlider);

    ////////////////////

    // Attack slider
    attackSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    //Set the isReadOnly flag to false to allow user to edit - Austin
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
	// reynas changes - adding value tree functionality
    attackSlider.setRange(1.0f, 200.0f, 1.0f);
    attackSlider.setValue((float)localState.getProperty("GateAttack", 25.0f), juce::dontSendNotification); // read from tree
    attackSlider.onValueChange = [this]() { // NEW: write to tree
        localState.setProperty("GateAttack", (float)attackSlider.getValue(), nullptr);
        };

    //Added these two to make them more nice looking and obvious for what they are - Austin
    attackSlider.setNumDecimalPlacesToDisplay(1);
    attackSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(attackSlider);

    ////////////////////

    // Release slider
    releaseSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    //Set the isReadOnly flag to false to allow user to edit - Austin
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
	// reynas changes - adding value tree functionality
    releaseSlider.setRange(10.0f, 1000.0f, 1.0f);
    releaseSlider.setValue((float)localState.getProperty("GateRelease", 100.0f), juce::dontSendNotification); // NEW
    releaseSlider.onValueChange = [this]() { // NEW
        localState.setProperty("GateRelease", (float)releaseSlider.getValue(), nullptr);
        };

    //Added these two to make them more nice looking and obvious for what they are - Austin
    releaseSlider.setNumDecimalPlacesToDisplay(1);
    releaseSlider.setTextValueSuffix(" ms");
    addAndMakeVisible(releaseSlider);

    ///////////////////

    //reynas changes - adding value tree functionality
    const float startThresholdDb = (float)localState.getProperty("GateThreshold", 0.0f);      // get starting gain from local state, if none, default to 0.0f
    thresholdSlider.setRange(-100.0, 0.0, 0.1);                                        // set slider range
    thresholdSlider.setValue(startThresholdDb, juce::dontSendNotification);                      // set slider to match starting gain
    thresholdSlider.onValueChange = [this]() {
        localState.setProperty("GateThreshold", (float)thresholdSlider.getValue(), nullptr);
        };

    const float startAttackMs = (float)localState.getProperty("GateAttack", 25.0f);
    attackSlider.setRange(1.0f, 200.0f, 1.0f);
    attackSlider.setValue(startAttackMs, juce::dontSendNotification);
    attackSlider.onValueChange = [this]() {
        localState.setProperty("GateAttack", (float)attackSlider.getValue(), nullptr);
        };

    const float startReleaseMs = (float)localState.getProperty("GateRelease", 100.0f);
    releaseSlider.setRange(10.0f, 1000.0f, 1.0f);
    releaseSlider.setValue(startReleaseMs, juce::dontSendNotification);
    releaseSlider.onValueChange = [this]() {
        localState.setProperty("GateRelease", (float)releaseSlider.getValue(), nullptr);
        };
}

void NoiseGatePanel::resized()
{
    auto area = getLocalBounds();

    // Label at top
    noiseGateLabel.setBounds(area.removeFromTop(30));

    // Divide remaining width for 3 dials
    auto dials = area.reduced(10);
    int dialWidth = dials.getWidth() / 3;

    // Create areas for each slider and label

    auto thresholdArea = dials.removeFromLeft(dialWidth).reduced(5);
    auto attackArea = dials.removeFromLeft(dialWidth).reduced(5);
    auto releaseArea = dials.reduced(5);
    
    //Positioning threshold label and slider
    thresholdLabel.setBounds(thresholdArea.removeFromTop(20));
    thresholdSlider.setBounds(thresholdArea);

    //Positioning attack label and slider
    attackLabel.setBounds(attackArea.removeFromTop(20));
    attackSlider.setBounds(attackArea);

    //Positioning release label and slider
    releaseLabel.setBounds(releaseArea.removeFromTop(20));
    releaseSlider.setBounds(releaseArea);
}
void NoiseGatePanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);
}

//////////////////////////////////////////////////////////

// destructor   - reyna
NoiseGatePanel::~NoiseGatePanel() {
    if (localState.isValid())
        localState.removeListener(this);
}

// value tree listener callback - reyna
void NoiseGatePanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    if (tree == localState) {
        if (property == juce::Identifier("GateThreshold"))
            thresholdSlider.setValue((float)tree.getProperty("GateThreshold"), juce::dontSendNotification);
        else if (property == juce::Identifier("GateAttack"))
            attackSlider.setValue((float)tree.getProperty("GateAttack"), juce::dontSendNotification);
        else if (property == juce::Identifier("GateRelease"))
            releaseSlider.setValue((float)tree.getProperty("GateRelease"), juce::dontSendNotification);
    }
}

// XML serialization - reyna
std::unique_ptr<juce::XmlElement> NoiseGateNode::toXml() const {
	// create XML element for NoiseGateNode
    auto xml = std::make_unique<juce::XmlElement>("NoiseGateNode");
    // setting values
    xml->setAttribute("name", effectName);
    xml->setAttribute("GateThreshold", (float)getNodeState().getProperty("GateThreshold", -100.0f));
    xml->setAttribute("GateAttack", (float)getNodeState().getProperty("GateAttack", 25.0f));
    xml->setAttribute("GateRelease", (float)getNodeState().getProperty("GateRelease", 100.0f));
    return xml;
}

void NoiseGateNode::loadFromXml(const juce::XmlElement& xml) {
	// loading values from XML attributes into ValueTree
    auto& s = getMutableNodeState();
    s.setProperty("GateThreshold", (float)xml.getDoubleAttribute("GateThreshold", -100.0f), nullptr);
    s.setProperty("GateAttack", (float)xml.getDoubleAttribute("GateAttack", 25.0f), nullptr);
    s.setProperty("GateRelease", (float)xml.getDoubleAttribute("GateRelease", 100.0f), nullptr);
}

NoiseGateVisualizer::~NoiseGateVisualizer(){
    if (localState.isValid())
        localState.removeListener(this);
}

// Update the graph by polling the node
void NoiseGateVisualizer::timerCallback(){ 
    float newDbLevel = noiseGateNode.getOutputLevelAtomic().load();
    pushData(newDbLevel);
    RealTimeGraphVisualizer::timerCallback(); // Call base to trigger repaint
}

// Update threshold line if property changes
void NoiseGateVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    if (tree == localState && property == juce::Identifier("GateThreshold"))
    {
        float newThreshold = (float)localState.getProperty("GateThreshold", -100.0f);
        setThreshold(newThreshold, true);
    }
}