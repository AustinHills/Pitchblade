// reyna

#include "Pitchblade/ui/EffectPanel.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "BinaryData.h"
#include "Pitchblade/panels/EffectNode.h"

//effects panel section
EffectPanel::EffectPanel(AudioPluginAudioProcessor& proc, const std::vector<std::shared_ptr<EffectNode>>& nodes)
                                                                            : processor(proc), effectNodes(nodes) {
    refreshTabs();

    //hide top tab buttons
    tabs.setTabBarDepth(0);
    tabs.setOpaque(false);
    tabs.getTabbedButtonBar().setOpaque(false);
    addAndMakeVisible(tabs);
}

void EffectPanel::resized() {
    //get bounds of panel and sets
    tabs.setBounds(getLocalBounds());
}

void EffectPanel::showEffect(int index) {
    //switches current tab
    tabs.setCurrentTabIndex(index);
}

// paint effect panel with gradient
void EffectPanel::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();

    juce::ColourGradient gradient(
        Colors::background.brighter(0.2f),
        r.getX(), r.getY(),
        Colors::background.brighter(0.05f),
        r.getX(), r.getBottom(),
        false
    );

    g.setGradientFill(gradient);

    g.fillRect(getLocalBounds());
    g.drawRect(getLocalBounds(), 2);

    auto bounds = getLocalBounds().reduced(4);   // padding
    g.setColour(Colors::accentPurple.withAlpha(0.6f));
    g.drawRect(bounds, 1);   // thickness 3

}

// rebuild tabs when effects are added/removed
void EffectPanel::refreshTabs() {
    std::lock_guard<std::recursive_mutex> lock(processor.getMutex());
    //rebuilds tabs based on gobal effects list
    effectNodes = processor.getEffectNodes();
    tabs.clearTabs();
    for (auto& node : effectNodes) {
        if (!node) continue; // skip invalid       
		{   // create panel from node
            auto panel = node->createPanel(processor);
            if (panel)   
                tabs.addTab(node->effectName, juce::Colours::transparentBlack, panel.release(), true);
        }
    }
}