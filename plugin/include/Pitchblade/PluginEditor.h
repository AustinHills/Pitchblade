/*
PluginEditor class builds and manages the entire Pitchblade user interface

It owns all of the UI components, including the TopBar, DaisyChain, EffectPanel, VisualizerPanel,
SettingsPanel, and PresetsPanel. It handles layout, resizing, and interaction logic between all
components. 

It listens for user interaction and keeps the UI in sync with the processors. The editor also 
rebuilds the chain view when loading presets, reordering nodes, or changing effect modes
It also applies the custom Look and Feel for the plugin.
*/

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

#include "Pitchblade/panels/SettingsPanel.h"    // Austin
#include "Pitchblade/panels/PresetsPanel.h"     // reyna
#include "Pitchblade/panels/EffectNode.h"       // for each effectPanel

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              public juce::DragAndDropContainer,
                                              public juce::Button::Listener,     // Austin -  added this for the settings panel
	                                          public juce::MouseListener         // reyna - for presets/settings closing on outside click
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

	void rebuildAndSyncUI();    // reyna - rebuild daisy chain and effect panel ui to sync with processor
	void applyRowTooltips();    // reyna - tooltip helper

    // bypass helpers - reyna
    bool areAllEffectsBypassed() const;
    void setAllEffectsBypassed(bool shouldBypass);
    void syncGlobalBypassButton();
    bool isLockBypassActive = false;

	// active effect button color helpers - reyna
    void setActiveEffectByName(const juce::String& effectName); 

	// getters for presets/settings panels - reyna 
    DaisyChain& getDaisyChain() { return daisyChain; }
    EffectPanel& getEffectPanel() { return effectPanel; }
    VisualizerPanel& getVisualizer() { return visualizer; }

	// helpers to close overlays - reyna
    void closeOverlaysIfOpen();
    void showPresets();
    bool isPresetsVisible() const;
    void showSettings();
    bool isSettingsVisible() const;

	//Austin - button listener for settings/presets panel
    void buttonClicked(juce::Button* button) override;

private:
    // This reference is provided as a quick way for your editor to access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    //reyna ui
	CustomLookAndFeel customLF;     //custom colorpallet using juce lookandfeel
                                                
    TopBar topBar;
    DaisyChain daisyChain;
    EffectPanel effectPanel;
    VisualizerPanel visualizer;

	juce::String activeEffectName;  // for active effect button coloring

	TooltipManager tooltipManager;  // tooltip manager
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

	// reyna presets panel              
	PresetsPanel presetsPanel;          // panel for managing presets
	juce::Component presetsBackdrop;    // backdrop when presets panel is open
    bool isShowingPresets = false;

	//Austin settings panel
    SettingsPanel settingsPanel;
    bool isShowingSettings = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};