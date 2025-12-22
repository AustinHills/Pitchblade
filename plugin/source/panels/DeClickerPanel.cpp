//Austin Hills

#include "Pitchblade/panels/DeClickerPanel.h"
#include "Pitchblade/ui/ColorPalette.h"
#include <JuceHeader.h>

// ==========================================================
// DeClickerPanel
// ==========================================================

DeClickerPanel::DeClickerPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
    : processor(proc), localState(state), panelTitle(nodeTitle)
{
    // Title
    titleLabel.setText(panelTitle, juce::dontSendNotification);
    titleLabel.setName("NodeTitle");
    addAndMakeVisible(titleLabel);

    // Sensitivity Slider
    sensitivitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
    sensitivitySlider.setNumDecimalPlacesToDisplay(2);
    sensitivitySlider.setName("Aggression");
    addAndMakeVisible(sensitivitySlider);

    sensitivitySlider.setRange(0.0, 1.0, 0.01);
    sensitivitySlider.setValue((float)localState.getProperty("Sensitivity", 0.5f), juce::dontSendNotification);
    
    sensitivitySlider.onValueChange = [this]() {
        localState.setProperty("Sensitivity", (float)sensitivitySlider.getValue(), &processor.undoManager);
    };
    sensitivitySlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    // History Slider
    historySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    historySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 25);
    historySlider.setNumDecimalPlacesToDisplay(0);
    historySlider.setName("Scan Window");
    addAndMakeVisible(historySlider);

    historySlider.setRange(1.0, 50.0, 1.0);
    historySlider.setValue((int)localState.getProperty("History", 5), juce::dontSendNotification);

    historySlider.onValueChange = [this]() {
        localState.setProperty("History", (int)historySlider.getValue(), &processor.undoManager);
    };
    historySlider.onDragStart = [this]() {
        processor.undoManager.beginNewTransaction();
    };

    localState.addListener(this);
}

DeClickerPanel::~DeClickerPanel() {
    if (localState.isValid()) localState.removeListener(this);
}

void DeClickerPanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);
}

void DeClickerPanel::resized() {
    auto area = getLocalBounds();

    // Title
    titleLabel.setBounds(area.removeFromTop(50));

    // Sliders
    auto dials = area.reduced(10);
    int dialWidth = dials.getWidth() / 2;

    sensitivitySlider.setBounds(dials.removeFromLeft(dialWidth).reduced(5));
    historySlider.setBounds(dials.reduced(5));
}

void DeClickerPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    if (tree != localState) return;

    if (property == juce::Identifier("Sensitivity")) {
        sensitivitySlider.setValue((float)tree.getProperty("Sensitivity"), juce::dontSendNotification);
    }
    else if (property == juce::Identifier("History")) {
        historySlider.setValue((int)tree.getProperty("History"), juce::dontSendNotification);
    }
}


// ==========================================================
// DeClickerNode
// ==========================================================

DeClickerNode::DeClickerNode(AudioPluginAudioProcessor& proc) 
    : EffectNode(proc, "DeClickerNode", "De-Clicker"), processor(proc) 
{
    // Defaults
    if (!getMutableNodeState().hasProperty("Sensitivity"))
        getMutableNodeState().setProperty("Sensitivity", 0.5f, nullptr);
    if (!getMutableNodeState().hasProperty("History"))
        getMutableNodeState().setProperty("History", 5, nullptr);

    // Initial DSP Prep
    dsp.prepare(proc.getSampleRate(), 2); // Default to stereo, will resize in process if needed
}

DeClickerNode::DeClickerNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
    : EffectNode(proc, existingState), processor(proc) 
{
     dsp.prepare(proc.getSampleRate(), 2);
}

void DeClickerNode::process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) {
    juce::ignoreUnused(proc);

    // Update Params
    float sens = (float)getNodeState().getProperty("Sensitivity", 0.5f);
    int hist = (int)getNodeState().getProperty("History", 5);

    dsp.setSensitivity(sens);
    dsp.setHistoryLength(hist);

    // Process
    dsp.process(buffer);
}

std::unique_ptr<juce::Component> DeClickerNode::createPanel(AudioPluginAudioProcessor& proc) {
    return std::make_unique<DeClickerPanel>(proc, getMutableNodeState(), effectName);
}

std::unique_ptr<juce::Component> DeClickerNode::createVisualizer(AudioPluginAudioProcessor& proc) {
    return std::make_unique<DeClickerVisualizer>(proc, *this, getMutableNodeState());
}

std::shared_ptr<EffectNode> DeClickerNode::clone() const {
    auto copiedTree = getNodeState().createCopy();
    copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);

    auto* self = const_cast<DeClickerNode*>(this);
    auto clonePtr = std::make_shared<DeClickerNode>(self->processor);
    clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);

    self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
    clonePtr->setDisplayName(effectName);
    return clonePtr;
}

std::unique_ptr<juce::XmlElement> DeClickerNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("DeClickerNode");
    xml->setAttribute("name", effectName);
    xml->setAttribute("Sensitivity", (float)getNodeState().getProperty("Sensitivity", 0.5f));
    xml->setAttribute("History", (int)getNodeState().getProperty("History", 5));
    return xml;
}

void DeClickerNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();
    s.setProperty("Sensitivity", (float)xml.getDoubleAttribute("Sensitivity", 0.5f), nullptr);
    s.setProperty("History", (int)xml.getIntAttribute("History", 5), nullptr);
}


// ==========================================================
// DeClickerVisualizer
// ==========================================================

DeClickerVisualizer::DeClickerVisualizer(AudioPluginAudioProcessor& proc, DeClickerNode& nodeRef, juce::ValueTree& state)
    : FrequencyGraphVisualizer(proc.apvts, 5, 2), node(nodeRef), localState(state)
{
}

DeClickerVisualizer::~DeClickerVisualizer() {
    if (localState.isValid()) localState.removeListener(this);
}

void DeClickerVisualizer::timerCallback() {
    // 1. Get Data
    auto original = node.getDSP().getInputSpectrumData();
    auto processed = node.getDSP().getProcessedSpectrumData();

    // 2. Push to Visualizer
    // Secondary = Original (Background reference)
    updateSecondarySpectrumData(original);
    // Primary = Processed (The result)
    updateSpectrumData(processed);

    // 3. Chain up
    FrequencyGraphVisualizer::timerCallback();
}

void DeClickerVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    juce::ignoreUnused(tree, property);
}

void DeClickerVisualizer::paint(juce::Graphics& g) {
    FrequencyGraphVisualizer::paint(g);
}
