// reyna macabebe

#include "Pitchblade/ui/TopBar.h"
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"

TopBar::TopBar() {
	// load logo image 
    logo.setImage(juce::ImageFileFormat::loadFrom(
        BinaryData::pitchblade_logo_png,
        BinaryData::pitchblade_logo_pngSize
    ));
    logo.setInterceptsMouseClicks(false, false);

	// set button styles
    addAndMakeVisible(logo);
    addAndMakeVisible(settingsButton);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(presetButton);
    addAndMakeVisible(lockBypassButton);

    //tooltip connection
    presetButton.getProperties().set("tooltipKey", "presetButton");
    settingsButton.getProperties().set("tooltipKey", "settingsButton");
    bypassButton.getProperties().set("tooltipKey", "bypassButton");
    lockBypassButton.getProperties().set("tooltipKey", "lockBypassButton");

    // Setup CPU Label
    addAndMakeVisible(cpuLabel);
    cpuLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    cpuLabel.setJustificationType(juce::Justification::centredRight);
    
    refreshColors();
}

void TopBar::refreshColors() {
    cpuLabel.setColour(juce::Label::textColourId, Colors::accentLight);
    
    // Refresh all buttons based on their toggle state
    setButtonActive(settingsButton, settingsButton.getToggleState());
    setButtonActive(bypassButton, bypassButton.getToggleState());
    setButtonActive(presetButton, presetButton.getToggleState());
    setButtonActive(lockBypassButton, lockBypassButton.getToggleState());

    repaint();
}

// paint top bar with gradient
void TopBar::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        Colors::panel,
        r.getX(), r.getY(),
        Colors::panel.darker(0.15f),
        r.getX(), r.getBottom(),
        false
    );

    g.setGradientFill(gradient);

    g.fillRect(getLocalBounds());
    g.drawRect(getLocalBounds(), 2);
}

void TopBar::resized() {
    // laying out each component
    auto area = getLocalBounds();;
    auto logoArea = area.removeFromLeft(200);
    logo.setBounds(logoArea);
    
    lockBypassButton.setBounds(area.removeFromRight(60));
    settingsButton.setBounds(area.removeFromRight(80));
    presetButton.setBounds(area.removeFromRight(80));
    bypassButton.setBounds(area.removeFromRight(80));

    cpuLabel.setBounds(area.removeFromLeft(120).reduced(0, 10));
}

// turn button pink if active
void TopBar::setButtonActive(juce::TextButton& button, bool active) {
    // Store state so we can refresh it later
    button.setToggleState(active, juce::dontSendNotification);

    const auto color = active ? Colors::accent : Colors::panel;
    button.setColour(juce::TextButton::buttonColourId, color);
    button.setColour(juce::TextButton::buttonOnColourId, color);
    button.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
    button.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
    button.repaint();
}

void TopBar::updateCpuStats(float load, float ms) {
    // Format: "12% | 0.5ms"
    // We color it RED if it goes above 90%
    juce::String text = juce::String(load * 100.0f, 1) + "% | " + juce::String(ms, 2) + "ms";
    cpuLabel.setText(text, juce::dontSendNotification);
    
    if (load > 0.9f)
        cpuLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    else
        cpuLabel.setColour(juce::Label::textColourId, Colors::accentLight);
}