//hudas code
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"

FormantPanel::FormantPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state)
    : processor(proc), localState(state) {
    // Labels + sliders
    //label names for dials - reyna
    formantSlider.setName("Formant");
    mixSlider.setName("Dry/Wet");

	// Listen to local state changes
    localState.addListener(this);

    // Panel title - reyna
    panelTitle.setText("Formant Shifter", juce::dontSendNotification);
    panelTitle.setName("NodeTitle");
    addAndMakeVisible(panelTitle);

    formantLabel.setText("Formant", juce::dontSendNotification);
    addAndMakeVisible(formantLabel);

    formantSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    formantSlider.setRange(-50.0, 50.0, 0.1);
    formantSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 30);
    formantSlider.setRange(-50.0, 50.0, 0.1);
    formantSlider.setSkewFactorFromMidPoint(1.0);
    addAndMakeVisible(formantSlider);

    mixLabel.setText("Dry/Wet", juce::dontSendNotification);
    addAndMakeVisible(mixLabel);

    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 30);
    mixSlider.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(mixSlider);

    // Initialize from ValueTree
    formantSlider.setValue((float)localState.getProperty("FORMANT_SHIFT", 0.0f), juce::dontSendNotification);
    mixSlider.setValue((float)localState.getProperty("FORMANT_MIX", 1.0f), juce::dontSendNotification);

    // Write back to node state
    formantSlider.onValueChange = [this]() {
        localState.setProperty("FORMANT_SHIFT",
            (float)formantSlider.getValue(), nullptr);
        };

    mixSlider.onValueChange = [this]() {
        localState.setProperty("FORMANT_MIX",
            (float)mixSlider.getValue(), nullptr);
        };
}

FormantPanel::~FormantPanel() {
    if (localState.isValid())
        localState.removeListener(this);
}

void FormantPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    if (tree != localState)
        return;

    if (property == juce::Identifier("FORMANT_SHIFT")) {
        formantSlider.setValue( (float)localState.getProperty("FORMANT_SHIFT", formantSlider.getValue()), juce::dontSendNotification);
    } else if (property == juce::Identifier("FORMANT_MIX")) {
        mixSlider.setValue( (float)localState.getProperty("FORMANT_MIX", mixSlider.getValue()), juce::dontSendNotification);
    }
}

void FormantPanel::resized() {
    panelTitle.setBounds(getLocalBounds().removeFromTop(30));

    auto r = getLocalBounds().reduced(12);
    const int labelWidth = 70;     
    const int sliderHeight = 60;
    const int verticalGap = 2;    

    r.removeFromTop(30);

    // Row 1: Formant
    auto row1 = r.removeFromTop(sliderHeight);
    formantLabel.setBounds(row1.removeFromLeft(labelWidth));
    formantSlider.setBounds(row1);
    r.removeFromTop(verticalGap);

    // Row 2: Dry Wet
    auto row2 = r.removeFromTop(sliderHeight);
    mixLabel.setBounds(row2.removeFromLeft(labelWidth));
    mixSlider.setBounds(row2);
}

void FormantPanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);
}


// XML serialization for saving/loading - reyna
std::unique_ptr<juce::XmlElement> FormantNode::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("FormantNode");
    xml->setAttribute("name", effectName);
    xml->setAttribute("FormantShift", (float)getNodeState().getProperty("FORMANT_SHIFT", 0.0f));
    xml->setAttribute("Mix", (float)getNodeState().getProperty("FORMANT_MIX", 100.0f));
    return xml;
}

void FormantNode::loadFromXml(const juce::XmlElement& xml) {
    auto& s = getMutableNodeState();
    s.setProperty("FORMANT_SHIFT", (float)xml.getDoubleAttribute("FormantShift", 0.0f), nullptr);
    s.setProperty("FORMANT_MIX", (float)xml.getDoubleAttribute("Mix", 100.0f), nullptr);
}
