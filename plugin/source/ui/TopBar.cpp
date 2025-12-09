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
    
}

// turn button pink if active
void TopBar::setButtonActive(juce::TextButton& button, bool active) {
    const auto color = active ? Colors::accent : Colors::panel;
    button.setColour(juce::TextButton::buttonColourId, color);
    button.setColour(juce::TextButton::buttonOnColourId, color);
    button.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
    button.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
    button.repaint();
}
