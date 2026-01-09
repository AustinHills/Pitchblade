// reyna
#include <JuceHeader.h>
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/panels/EffectNode.h"

VisualizerPanel::VisualizerPanel(AudioPluginAudioProcessor& proc, std::vector<std::shared_ptr<EffectNode>>& nodes)
                                                                        : processor(proc), effectNodes(nodes) {
    tabs.setTabBarDepth(0);  // hide tab bar
    setOpaque(false);        // prevents hiding tooltip 
    addAndMakeVisible(tabs);
    refreshTabs();

    setInterceptsMouseClicks(false, true);
}

void VisualizerPanel::paint(juce::Graphics& g) {

    auto r = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        Colors::panel,
        r.getX(), r.getY(),
        Colors::panel.darker(0.3f),
        r.getX(), r.getBottom(),
        false
    );

    g.fillRect(getLocalBounds());
    g.drawRect(getLocalBounds(), 2);

    auto bounds = getLocalBounds().reduced(4);   // padding
    g.setColour(Colors::accentPurple.withAlpha(0.6f));
    g.drawRect(bounds, 1);   // thickness 3
}

void VisualizerPanel::resized() {
    tabs.setBounds(getLocalBounds());
}

// show visualizer tab at index
void VisualizerPanel::showVisualizer(int index) {
	// safety check for valid index
    if (index < 0 || index >= tabs.getNumTabs()) {
        return;
    }
    tabs.setCurrentTabIndex(index);
}

// clear all visualizer tabs
void VisualizerPanel::clearVisualizer() {
    tabs.clearTabs();
}

// refresh visualizer tabs based on current effect nodes
void VisualizerPanel::refreshTabs() {
    tabs.clearTabs();

	// make a safe copy of effect nodes
    std::vector<std::shared_ptr<EffectNode>> safeNodes; {
        std::lock_guard<std::recursive_mutex> lock(processor.getMutex());
        safeNodes = effectNodes;                     // copy shared_ptrs, still safe references
    }

	// each node creates its own visualizer 
    for (auto& node : safeNodes) {
        if (!node) {
			continue;   // skip null nodes
        }
        //auto visualizer = node->createVisualizer(processor);
		std::unique_ptr<juce::Component> visualizer;        // prepare pointer
        try {
			visualizer = node->createVisualizer(processor); // may throw
        } catch (...) {
			visualizer = nullptr;                           // on error, set to null
        } if (visualizer) {
            tabs.addTab(node->effectName, juce::Colours::black, visualizer.release(), true);    
        } else {
            // default placeholder , currently for formant and pitch 
            auto* placeholder = new juce::Label({}, node->effectName + " Visualizer");
            placeholder->setJustificationType(juce::Justification::centred);
            placeholder->setFont(juce::Font(18.0f));
            tabs.addTab(node->effectName, juce::Colours::black, placeholder, true);
        }
    }
}