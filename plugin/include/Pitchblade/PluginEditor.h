#pragma once
#include "PluginProcessor.h"

////ui reyna
#include <JuceHeader.h>
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/TooltipManager.h"

#include "ui/TopBar.h"
#include "ui/DaisyChain.h"
#include "ui/EffectPanel.h"
#include "ui/VisualizerPanel.h"

#include "Pitchblade/panels/EffectNode.h"

#include "Pitchblade/panels/SettingsPanel.h"    // Austin
#include "Pitchblade/panels/PresetsPanel.h"     // reyna


//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              public juce::DragAndDropContainer,
                                              public juce::Button::Listener,     // Austin added this for the settings panel
	                                          public juce::MouseListener         // reyna - for presets/settings closing on outside click
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;
	void rebuildAndSyncUI(); // reyna - rebuild daisy chain and effect panel ui to sync with processor

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

	void applyRowTooltips();    //reyna tooltip helper

    //Austin
    void buttonClicked(juce::Button* button) override;

	// getters for presets and settings panels - reyna 
    DaisyChain& getDaisyChain() { return daisyChain; }
    EffectPanel& getEffectPanel() { return effectPanel; }
    VisualizerPanel& getVisualizer() { return visualizer; }

	// helpers to close overlays - reyna
    void closeOverlaysIfOpen();
    void showPresets();
    bool isPresetsVisible() const;
    void showSettings();
    bool isSettingsVisible() const;

    void setActiveEffectByName(const juce::String& effectName);  //daisychain active button

private:
    // This reference is provided as a quick way for your editor to access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    //reyna ui
	CustomLookAndFeel customLF;     //custom colorpallet using juce lookandfeel
                                                
    TopBar topBar;
    DaisyChain daisyChain;
    
    EffectPanel effectPanel;
    VisualizerPanel visualizer;

    juce::String activeEffectName;

    TooltipManager tooltipManager;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

	// reyna presets panel              
	PresetsPanel presetsPanel;          // panel for managing presets
	juce::Component presetsBackdrop;    // backdrop when presets panel is open
    bool isShowingPresets = false;

    //Austin added this
    SettingsPanel settingsPanel;
    bool isShowingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};