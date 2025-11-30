//Austin Hills
/*
    The SettingsPanel class provides the plugin's global configuration controls.
    It displays UI elements that affect the plugin as a whole rather than any
    single effect, such as the global graph framerate. 
    
    The panel connects its controls directly to parameters in the 
    AudioProcessorValueTreeState so settings remain stored and recalled with presets.

    The SettingsPanel does not manage DSP or routing. 
*/

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/PluginProcessor.h"

//The settings panel class inherits from component rather than effectpanel, since there are too many differences in application
class SettingsPanel : public juce::Component {
private:
    //Needs a reference to the processor to get the APVTS
    AudioPluginAudioProcessor& processor;

    juce::Label framerateLabel;
    //Used a combobox to list specific user choices
    juce::ComboBox framerateDropDown;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> framerateAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
public:
    //Constructor
    SettingsPanel(AudioPluginAudioProcessor& p);

    //Destructor
    ~SettingsPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
};