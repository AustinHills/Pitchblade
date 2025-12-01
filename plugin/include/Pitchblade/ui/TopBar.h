// reyna
/*
    The TopBar class builds the upper control bar of the Pitchblade interface.
    It displays the logo and provides quick access buttons for Presets,
    Global Bypass, and Settings. The TopBar does not handle processing logic.
    It only forwards button interactions to the PluginEditor so the editor
    can open panels or update the bypass state.
*/

#pragma once
#include <JuceHeader.h>

//top bar
class TopBar : public juce::Component {
public:
    TopBar();
    void resized() override;
    void paint(juce::Graphics&) override;

    void setButtonActive(juce::TextButton& button, bool active);    // for coloring button

	// buttons to be accessed by editor
    juce::TextButton presetButton{ "Presets" };
    juce::TextButton bypassButton{ "Bypass" };
    juce::TextButton settingsButton{ "Settings" };
    juce::TextButton lockBypassButton{ "Lock" };

	// logo image
    juce::ImageComponent logo;
};