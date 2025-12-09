//Austin Hills

#include "Pitchblade/panels/AdaptiveDeNoiserPanel.h"
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"

AdaptiveDeNoiserPanel::AdaptiveDeNoiserPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
                : processor(proc), localState(state), panelTitle(nodeTitle) {

    //Label
    deNoiserLabel.setText(panelTitle, juce::dontSendNotification);
    addAndMakeVisible(deNoiserLabel);
    deNoiserLabel.setName("NodeTitle");

    //Reduction slider
    reductionSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    reductionSlider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,25);
    reductionSlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(reductionSlider);

    //Reduction label
    reductionLabel.setText("Reduction Intensity",juce::dontSendNotification);
    reductionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(reductionLabel);

    //Threshold slider
    thresholdSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,25);
    thresholdSlider.setNumDecimalPlacesToDisplay(2);
    addAndMakeVisible(thresholdSlider);

    //Threshold label
    thresholdLabel.setText("Learning Threshold",juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(thresholdLabel);

    //Link sliders to local state properties
    const float startReduction = (float)localState.getProperty("DenoiserReduction",0.5f);
    reductionSlider.setRange(0.0,1.0f,0.01f);
    reductionSlider.setValue(startReduction,juce::dontSendNotification);
    reductionSlider.onValueChange = [this]() {
        localState.setProperty("DenoiserReduction",(float)reductionSlider.getValue(),&processor.undoManager);
        };
    reductionSlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    const float startThreshold = (float)localState.getProperty("DenoiserThreshold",-60.0f);
    thresholdSlider.setRange(-100.0,0.0f,0.1f);
    thresholdSlider.setValue(startThreshold,juce::dontSendNotification);
    thresholdSlider.onValueChange = [this]() {
        localState.setProperty("DenoiserThreshold",(float)thresholdSlider.getValue(),&processor.undoManager);
        };
    thresholdSlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    //Add this panel as a listener to the local state
    localState.addListener(this);
}

AdaptiveDeNoiserPanel::~AdaptiveDeNoiserPanel(){
    if(localState.isValid()){
        localState.removeListener(this);
    }
}

void AdaptiveDeNoiserPanel::paint(juce::Graphics& g){
    g.drawRect(getLocalBounds(),2);
}

void AdaptiveDeNoiserPanel::resized(){
    auto area = getLocalBounds();

    //Title label at the top
    deNoiserLabel.setBounds(area.removeFromTop(50));

    auto dials = area.reduced(10);

    int columnWidth = dials.getWidth() / 2;

    auto thresholdArea = dials.removeFromLeft(columnWidth).reduced(5);

    //Positioning threshold label and slider
    thresholdLabel.setBounds(thresholdArea.removeFromTop(20));
    thresholdSlider.setBounds(thresholdArea);

    auto reductionArea = dials.reduced(5);

    //Positioning reduction label and slider
    reductionLabel.setBounds(reductionArea.removeFromTop(20));
    reductionSlider.setBounds(reductionArea);
}

void AdaptiveDeNoiserPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    if(tree == localState){
        if(property==juce::Identifier("DenoiserReduction")){
            reductionSlider.setValue((float)tree.getProperty("DenoiserReduction"),juce::dontSendNotification);
        }

        if(property==juce::Identifier("DenoiserThreshold")){
            thresholdSlider.setValue((float)tree.getProperty("DenoiserThreshold"),juce::dontSendNotification);
        }
    }
}

//Visualizer stuff
AdaptiveDeNoiserVisualizer::~AdaptiveDeNoiserVisualizer(){
    if(localState.isValid()){
        localState.removeListener(this);
    }
}

void AdaptiveDeNoiserVisualizer::timerCallback(){
    //Get data from processor
    auto spectrum = deNoiserNode.getDSP().getSpectrumData();
    auto noise = deNoiserNode.getDSP().getNoiseProfileData();

    //Push data to visualizer
    updateSpectrumData(spectrum);
    updateSecondarySpectrumData(noise);

    FrequencyGraphVisualizer::timerCallback();
}

void AdaptiveDeNoiserVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property){
    juce::ignoreUnused(tree, property);
}

void AdaptiveDeNoiserVisualizer::paint(juce::Graphics& g){
    FrequencyGraphVisualizer::paint(g);

    //Now draw custom threshold line on top of the existing graph
    auto bounds = getLocalBounds();
    // Replicate padding from FrequencyGraphVisualizer::resized()
    bounds = bounds.reduced(15); 
    
    // Remove label areas
    bounds.removeFromBottom(20); // xLabelBounds
    bounds.removeFromLeft(40);   // yLabelBounds
    
    // The graph area
    auto graphBounds = bounds.reduced(0, 5);

    // Get the current threshold from our state
    float threshDB = (float)localState.getProperty("DenoiserThreshold", -60.0f);
    
    // Map dB to Y pixel
    // Range defined in base class is -100dB to 0dB
    float yPos = juce::jmap(threshDB, -100.0f, 0.0f, 
                            (float)graphBounds.getBottom(), (float)graphBounds.getY());

    // Clamp to be safe
    yPos = juce::jlimit((float)graphBounds.getY(), (float)graphBounds.getBottom(), yPos);

    // Draw the line
    g.setColour(Colors::accent); // Or your preferred color
    float dashes[] = {4.0f, 4.0f};
    g.drawDashedLine(juce::Line<float>((float)graphBounds.getX(), yPos, (float)graphBounds.getRight(), yPos), 
                        dashes, 2);
}

// XML serialization - Austin
std::unique_ptr<juce::XmlElement> AdaptiveDeNoiserNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("DeNoiserNode");
    xml->setAttribute("name", effectName);
    xml->setAttribute("DenoiserReduction", (float)getNodeState().getProperty("DenoiserReduction", 0.0f));
    xml->setAttribute("DenoiserThreshold", (float)getNodeState().getProperty("DenoiserThreshold", -60.0f));
    return xml;
}

void AdaptiveDeNoiserNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();
    s.setProperty("DenoiserReduction", (float)xml.getDoubleAttribute("DenoiserReduction", 0.0f), nullptr);
    s.setProperty("DenoiserThreshold", (float)xml.getDoubleAttribute("DenoiserThreshold", -60.0f), nullptr);
}