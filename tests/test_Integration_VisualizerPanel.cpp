//reyna

#include <gtest/gtest.h>
#include <JuceHeader.h>

#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/PluginEditor.h"
#include "Pitchblade/ui/DaisyChain.h"
#include "Pitchblade/ui/VisualizerPanel.h"

// TC-86 DaisyChain Integration with VisualizerPanel
TEST(VisualizerIntegrationTest, DaisyChainSynchronizesWithVisualizerPanel) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // load default preset to fill nodes
    proc.loadDefaultPreset("default");
    AudioPluginAudioProcessorEditor editor(proc);

    auto& daisy = editor.getDaisyChain();
    auto& visualizer = editor.getVisualizer();
    auto& nodes = proc.getEffectNodes();

    //  UI sync
    editor.rebuildAndSyncUI();

    // tab count should match effectNodes count
    int tabCount = visualizer.getTabbedComponent().getNumTabs();
    EXPECT_EQ(tabCount, nodes.size());

    // DaisyChain count should match effectNodes count
    int rowCount = daisy.items.size();
    EXPECT_EQ(rowCount, nodes.size());

    // Order must match effectNodes order
    for (int i = 0; i < nodes.size(); ++i) {
        auto tabName = visualizer.getTabbedComponent().getTabNames()[i];
        EXPECT_EQ(tabName, nodes[i]->effectName);
    }
}

// TC-87 Clicking DaisyChainItem opens the correct visualizer tab
TEST(VisualizerIntegrationTest, ClickingDaisyChainItemOpensCorrectTab) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    proc.loadDefaultPreset("default");

    AudioPluginAudioProcessorEditor editor(proc);

    auto& daisy = editor.getDaisyChain();
    auto& visualizer = editor.getVisualizer();

    editor.rebuildAndSyncUI();

    for (int i = 0; i < daisy.items.size() && i < 3; ++i) {
        daisy.items[i]->button.triggerClick();

        int tabIndex = visualizer.getTabbedComponent().getCurrentTabIndex();

    }
}

