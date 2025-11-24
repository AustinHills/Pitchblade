// reyna
#pragma once
#include <JuceHeader.h>
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/PluginProcessor.h"

//panel that shows audio visuals
class VisualizerPanel : public juce::Component
{
public:
	VisualizerPanel(AudioPluginAudioProcessor& proc, std::vector<std::shared_ptr<EffectNode>>& nodes);   // pass processor + effect nodes for tabs
    juce::TabbedComponent& getTabbedComponent() { return tabs; }
    const juce::TabbedComponent& getTabbedComponent() const { return tabs; }

    int getNumTabs() const { return tabs.getNumTabs(); }
    int getCurrentTabIndex() const { return tabs.getCurrentTabIndex(); }
    juce::String getTabName(int index) const {
        return tabs.getTabNames()[index];
    }

    void resized() override;
    void paint(juce::Graphics& g) override;

    void showVisualizer(int index);
	void clearVisualizer();
    void refreshTabs();
 
    void clearTabs() {
        while (tabs.getNumTabs() > 0)
            tabs.removeTab(0);
    }

private:
	AudioPluginAudioProcessor& processor;               // reference to main processor so visulaizer can access effect data
	std::vector<std::shared_ptr<EffectNode>>& effectNodes;          // global effect nodes
    juce::TabbedComponent tabs{ juce::TabbedButtonBar::TabsAtTop };
};