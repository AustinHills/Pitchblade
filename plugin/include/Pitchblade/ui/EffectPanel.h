// reyna
/*
    The EffectPanel class displays each effect inside the plugin. The panel 
    switches tabs when the user selects an effect in the DaisyChain. EffectPanel 
    does not perform DSP. It only builds and manages the UI controls for each 
    effect and keeps them in sync with the processor's ValueTree parameters.
*/

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"

//tabs for each effect
class EffectPanel : public juce::Component {
public:
    explicit EffectPanel(AudioPluginAudioProcessor& proc, const std::vector<std::shared_ptr<EffectNode>>& nodes);

    void resized() override;
    void showEffect(int index);
    void paint(juce::Graphics&) override;

	//refresh tabs when effects are added/removed
    void refreshTabs();

private:
    AudioPluginAudioProcessor& processor;
    // tabbed component for effect panels
    juce::TabbedComponent tabs{ juce::TabbedButtonBar::TabsAtTop };
	// reference to global effect nodes
    std::vector<std::shared_ptr<EffectNode>> effectNodes;
};