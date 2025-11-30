//reyna
/*
    The PresetsPanel class manages loading, saving, and browsing presets for
    Pitchblade.

    It provides a dedicated UI where users can save the current effect chain
    layout, load existing presets from disk, and view preset metadata. The
    panel communicates with the processor to serialize all node states, global
    parameters, and layout information.

    The PresetsPanel does not modify DSP directly. It only triggers preset
    save or load actions and tells the PluginEditor to rebuild the UI after
    preset changes
*/


#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/PluginProcessor.h"

//panel that shows preset management
// load presets, save presets, default presets

class PresetsPanel : public juce::Component {
public:
    explicit PresetsPanel(AudioPluginAudioProcessor& proc);
    ~PresetsPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onPresetActionFinished; // callback after preset action

private:
    AudioPluginAudioProcessor& processor;

    juce::TextButton saveButton{ "Save Preset" };
    juce::TextButton loadButton{ "Load Preset" };
    juce::TextButton defaultButton{ "Use Default Preset" };
    juce::Label statusLabel;    // for switching presets, mesg to let u know if it worked

	// file chooser for loading/saving presets
    std::unique_ptr<juce::FileChooser> chooser;

    void handleSavePreset();
    void handleLoadPreset();
    void handleDefaultPreset();

    void showDefaultMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetsPanel)
};