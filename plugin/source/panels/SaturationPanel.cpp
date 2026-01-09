// Written by Austin Hills

#include "Pitchblade/panels/SaturationPanel.h"
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"

// Constructor
SaturationPanel::SaturationPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, SaturationNode* nodePtr, const juce::String& nodeTitle)
    : processor(proc), localState(state), node(nodePtr), panelTitle(nodeTitle) 
{
    // Node Title Label
    saturationLabel.setText(panelTitle, juce::dontSendNotification);
    addAndMakeVisible(saturationLabel);
    saturationLabel.setName("NodeTitle");

    // Volume Meter removed

    // Sliders
    static SmallDialLookAndFeel smallDialLF;
    driveSlider.setLookAndFeel(&smallDialLF);
    mixSlider.setLookAndFeel(&smallDialLF);

    // Drive Slider
    driveSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    driveSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    driveSlider.setNumDecimalPlacesToDisplay(1);
    driveSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(driveSlider);
    driveLabel.setText("Drive", juce::dontSendNotification);
    driveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(driveLabel);

    // Mix Slider
    mixSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    mixSlider.setNumDecimalPlacesToDisplay(2);
    mixSlider.setTextValueSuffix("");
    addAndMakeVisible(mixSlider);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixLabel);

    // Initial values from state
    const float startDrive = (float)localState.getProperty("SatDrive", 0.0f);
    driveSlider.setRange(0.0f, 24.0f, 0.1f);
    driveSlider.setValue(startDrive, juce::dontSendNotification);
    
    driveSlider.onValueChange = [this]() {
        localState.setProperty("SatDrive", (float)driveSlider.getValue(), &processor.undoManager);
    };
    driveSlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    const float startMix = (float)localState.getProperty("SatMix", 1.0f);
    mixSlider.setRange(0.0f, 1.0f, 0.01f);
    mixSlider.setValue(startMix, juce::dontSendNotification);

    mixSlider.onValueChange = [this]() {
        localState.setProperty("SatMix", (float)mixSlider.getValue(), &processor.undoManager);
    };
    mixSlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    // Listen for external state changes (e.g. undo/redo)
    localState.addListener(this);
}

SaturationPanel::~SaturationPanel() {
    if (localState.isValid())
        localState.removeListener(this);
}

void SaturationPanel::place(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label) {
    slider.setBounds(area.reduced(10));
    label.setBounds(area.removeFromBottom(20));
}

void SaturationPanel::resized() {
    auto area = getLocalBounds();

    // Panel label at top
    saturationLabel.setBounds(area.removeFromTop(30));

    auto r = area.reduced(10, 6);

    // Remaining area for 2 controls
    int w = r.getWidth() / 2;
    
    // Drive
    auto leftArea = r.removeFromLeft(w);
    place(leftArea, driveSlider, driveLabel);

    // Mix
    auto rightArea = r;
    place(rightArea, mixSlider, mixLabel);
}

void SaturationPanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);
}

void SaturationPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == localState)
    {
        if (property == juce::Identifier("SatDrive"))
            driveSlider.setValue((float)tree.getProperty("SatDrive"), juce::dontSendNotification);
        else if (property == juce::Identifier("SatMix"))
            mixSlider.setValue((float)tree.getProperty("SatMix"), juce::dontSendNotification);
    }
}

// Visualizer Implementation ===================================================

SaturationVisualizer::~SaturationVisualizer(){
    if(localState.isValid()){
        localState.removeListener(this);
    }
}

void SaturationVisualizer::timerCallback(){
    float newDbLevel = saturationNode.getOutputLevelAtomic().load();
    pushData(newDbLevel);
    RealTimeGraphVisualizer::timerCallback();
}

// Node Implementation =========================================================

std::unique_ptr<juce::XmlElement> SaturationNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("SaturationNode");
    xml->setAttribute("name", effectName);
    xml->setAttribute("SatDrive", (float)getNodeState().getProperty("SatDrive", 0.0f));
    xml->setAttribute("SatMix", (float)getNodeState().getProperty("SatMix", 1.0f));
    return xml;
}

void SaturationNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();
    s.setProperty("SatDrive", (float)xml.getDoubleAttribute("SatDrive", 0.0f), nullptr);
    s.setProperty("SatMix", (float)xml.getDoubleAttribute("SatMix", 1.0f), nullptr);
}
