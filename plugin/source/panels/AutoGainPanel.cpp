// Written by Austin Hills

#include "Pitchblade/panels/AutoGainPanel.h"

AutoGainPanel::AutoGainPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
    : processor(proc), localState(state), panelTitle(nodeTitle)
{
    // Listen to value tree
    localState.addListener(this);

    // --- Target Slider ---
    addAndMakeVisible(targetSlider);
    targetSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    targetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    targetSlider.setRange(-60.0, 0.0, 0.1);
    targetSlider.setValue((double)localState.getProperty("AutoGainTarget"));
    targetSlider.onValueChange = [this]() {
        localState.setProperty("AutoGainTarget", targetSlider.getValue(), nullptr);
    };

    addAndMakeVisible(targetLabel);
    targetLabel.setText("Target dB", juce::dontSendNotification);
    targetLabel.setJustificationType(juce::Justification::centred);
    targetLabel.attachToComponent(&targetSlider, false);

    // --- Threshold Slider ---
    addAndMakeVisible(thresholdSlider);
    thresholdSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    thresholdSlider.setRange(-100.0, 0.0, 0.1);
    thresholdSlider.setValue((double)localState.getProperty("AutoGainThreshold"));
    thresholdSlider.onValueChange = [this]() {
        localState.setProperty("AutoGainThreshold", thresholdSlider.getValue(), nullptr);
    };

    addAndMakeVisible(thresholdLabel);
    thresholdLabel.setText("Threshold", juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centred);
    thresholdLabel.attachToComponent(&thresholdSlider, false);

    // --- Attack Slider ---
    addAndMakeVisible(attackSlider);
    attackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    attackSlider.setRange(0.1, 500.0, 0.1);
    attackSlider.setValue((double)localState.getProperty("AutoGainAttack"));
    attackSlider.onValueChange = [this]() {
        localState.setProperty("AutoGainAttack", attackSlider.getValue(), nullptr);
    };

    addAndMakeVisible(attackLabel);
    attackLabel.setText("Attack (ms)", juce::dontSendNotification);
    attackLabel.setJustificationType(juce::Justification::centred);
    attackLabel.attachToComponent(&attackSlider, false);

    // --- Release Slider ---
    addAndMakeVisible(releaseSlider);
    releaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    releaseSlider.setRange(10.0, 2000.0, 0.1);
    releaseSlider.setValue((double)localState.getProperty("AutoGainRelease"));
    releaseSlider.onValueChange = [this]() {
        localState.setProperty("AutoGainRelease", releaseSlider.getValue(), nullptr);
    };

    addAndMakeVisible(releaseLabel);
    releaseLabel.setText("Release (ms)", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centred);
    releaseLabel.attachToComponent(&releaseSlider, false);
}

AutoGainPanel::~AutoGainPanel()
{
    localState.removeListener(this);
}

void AutoGainPanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Simple 4-column layout
    int width = area.getWidth() / 4;
    int height = area.getHeight();
    
    // Add top margin for labels attached to components
    int topMargin = 20;

    targetSlider.setBounds(area.getX(), area.getY() + topMargin, width, height - topMargin);
    thresholdSlider.setBounds(area.getX() + width, area.getY() + topMargin, width, height - topMargin);
    attackSlider.setBounds(area.getX() + width * 2, area.getY() + topMargin, width, height - topMargin);
    releaseSlider.setBounds(area.getX() + width * 3, area.getY() + topMargin, width, height - topMargin);
}

void AutoGainPanel::paint(juce::Graphics& g)
{
    // Basic background if needed, usually handled by parent
}

void AutoGainPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree != localState) return;

    if (property == juce::Identifier("AutoGainTarget"))
        targetSlider.setValue(localState.getProperty(property), juce::dontSendNotification);
    else if (property == juce::Identifier("AutoGainThreshold"))
        thresholdSlider.setValue(localState.getProperty(property), juce::dontSendNotification);
    else if (property == juce::Identifier("AutoGainAttack"))
        attackSlider.setValue(localState.getProperty(property), juce::dontSendNotification);
    else if (property == juce::Identifier("AutoGainRelease"))
        releaseSlider.setValue(localState.getProperty(property), juce::dontSendNotification);
}

// Visualizer Implementation ===========================================

AutoGainVisualizer::~AutoGainVisualizer()
{
    localState.removeListener(this);
}

void AutoGainVisualizer::timerCallback()
{
    // Push new value to graph
    float db = autoGainNode.getOutputLevelAtomic().load();
    pushData(db);
    repaint();
}

void AutoGainVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == juce::Identifier("AutoGainThreshold"))
    {
        float thresh = (float)localState.getProperty(property);
        setThreshold(thresh, true);
    }
    else if (property == juce::Identifier("AutoGainTarget"))
    {
        repaint(); 
    }
}

void AutoGainVisualizer::paint(juce::Graphics& g)
{
    // Call base class paint first
    RealTimeGraphVisualizer::paint(g);

    // Draw Target Line
    // Replicate layout logic to find graph area
    auto bounds = getLocalBounds();
    bounds = bounds.reduced(15);
    bounds.removeFromLeft(40); // labelWidth
    auto graphBounds = bounds.reduced(0, 5);

    float targetDb = (float)localState.getProperty("AutoGainTarget", -6.0f);
    
    // Map -100 to 0 -> Bottom to Top
    float y = juce::jmap(targetDb, -100.0f, 0.0f, (float)graphBounds.getBottom(), (float)graphBounds.getY());
    
    y = juce::jlimit((float)graphBounds.getY(), (float)graphBounds.getBottom(), y);

    g.setColour(Colors::accentPink); 
    float dashes[] = { 4.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>((float)graphBounds.getX(), y, (float)graphBounds.getRight(), y), dashes, 2);
}

// Node Serialization ==================================================

std::unique_ptr<juce::XmlElement> AutoGainNode::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("AutoGainNode");
    xml->setAttribute("AutoGainTarget", (double)getNodeState().getProperty("AutoGainTarget"));
    xml->setAttribute("AutoGainThreshold", (double)getNodeState().getProperty("AutoGainThreshold"));
    xml->setAttribute("AutoGainAttack", (double)getNodeState().getProperty("AutoGainAttack"));
    xml->setAttribute("AutoGainRelease", (double)getNodeState().getProperty("AutoGainRelease"));
    return xml;
}

void AutoGainNode::loadFromXml(const juce::XmlElement& xml)
{
    if (xml.hasAttribute("AutoGainTarget"))
        getMutableNodeState().setProperty("AutoGainTarget", xml.getDoubleAttribute("AutoGainTarget"), nullptr);
    if (xml.hasAttribute("AutoGainThreshold"))
        getMutableNodeState().setProperty("AutoGainThreshold", xml.getDoubleAttribute("AutoGainThreshold"), nullptr);
    if (xml.hasAttribute("AutoGainAttack"))
        getMutableNodeState().setProperty("AutoGainAttack", xml.getDoubleAttribute("AutoGainAttack"), nullptr);
    if (xml.hasAttribute("AutoGainRelease"))
        getMutableNodeState().setProperty("AutoGainRelease", xml.getDoubleAttribute("AutoGainRelease"), nullptr);
}
