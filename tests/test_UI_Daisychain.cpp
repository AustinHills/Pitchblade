//reyna 
#include <gtest/gtest.h>
#include <JuceHeader.h>

#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/DaisyChain.h"

// test helper to make daisychain rows , mirrors the one in daisychain.cpp
static std::vector<AudioPluginAudioProcessor::Row>
toProcessorRows_ForTests(const std::vector<DaisyChain::Row>& uiRows) {
    std::vector<AudioPluginAudioProcessor::Row> out;
    out.reserve(uiRows.size());
    for (auto& r : uiRows)
        out.push_back({ r.left, r.right });
    return out;
}

//all ui tests 
// TC-21 DaisyChain Initialization
// verifies number of rows equals number of effectNodes
TEST(DaisyChainTest, BuildsRowsFromProcessorNodes) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    const auto& layout = dc.getCurrentLayout();
    EXPECT_EQ(layout.size(), nodes.size());
}

// TC-22 verifies that DaisyChain order matches processor order
TEST(DaisyChainTest, CurrentOrderMatchesLayout) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    auto order = dc.getCurrentOrder();

    ASSERT_EQ(order.size(), nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
        EXPECT_EQ(order[i], nodes[i]->effectName);
}

// TC-23 verifies resetRowsToNodes restores the original processor layout
TEST(DaisyChainTest, ResetRowsToNodesRestoresDefaultOrder) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    proc.getMutex().lock();
    auto& layout = const_cast<std::vector<DaisyChain::Row>&>(dc.getCurrentLayout());
    if (!layout.empty())
        layout[0].left = "BrokenName";
    proc.getMutex().unlock();

    dc.resetRowsToNodes();
    const auto& fixed = dc.getCurrentLayout();

    ASSERT_EQ(fixed.size(), nodes.size());
    if (!nodes.empty())
        EXPECT_EQ(fixed[0].left, nodes[0]->effectName);
}

// TC-22 DaisyChain Default Global State
// verifies reorderLocked default and bypass visual flag
TEST(DaisyChainTest, DefaultGlobalStateIsCorrect) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    EXPECT_FALSE(dc.isReorderLocked());

    dc.setGlobalBypassVisual(false);
    SUCCEED();
}

// TC-23 effectNodes Mapping
// verifies each row label matches its EffectNode name
TEST(DaisyChainTest, EffectNodeNamesMapToRowsCorrectly) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    const auto& layout = dc.getCurrentLayout();

    ASSERT_EQ(layout.size(), nodes.size());

    for (size_t i = 0; i < nodes.size(); ++i)
        EXPECT_EQ(layout[i].left, nodes[i]->effectName);
}

// TC-24 DaisyChain Item Bypass Toggle
// verifies that toggling the bypass button updates the EffectNode bypass state
TEST(DaisyChainTest, ItemBypassTogglesNodeState) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();  

    ASSERT_FALSE(nodes.empty());
    ASSERT_GE(dc.items.size(), 1);

    auto* item = dc.items[0];
    ASSERT_NE(item, nullptr);

    // find matching EffectNode
    auto targetName = item->getName();
    std::shared_ptr<EffectNode> targetNode = nullptr;
    for (auto& n : nodes)
        if (n && n->effectName == targetName)
            targetNode = n;

    ASSERT_NE(targetNode, nullptr);

    EXPECT_FALSE(targetNode->bypassed);

    // bypass click
    item->onBypassChanged(item->getIndex(), true);

    EXPECT_TRUE(targetNode->bypassed);
    EXPECT_TRUE(item->bypassed);
}

// TC-25 DaisyChain Bypass Toggle Repeat Behavior
// verifies two presses restore the original active state and do not affect other nodes
TEST(DaisyChainTest, ItemBypassDoubleClickRestoresState) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();  

    ASSERT_GE(nodes.size(), 2);
    ASSERT_GE(dc.items.size(), 2);

    auto* item = dc.items[0];
    ASSERT_NE(item, nullptr);

    // find matching EffectNode
    auto targetName = item->getName();
    std::shared_ptr<EffectNode> targetNode = nullptr;
    for (auto& n : nodes)
        if (n && n->effectName == targetName)
            targetNode = n;

    ASSERT_NE(targetNode, nullptr);

    // record state of all nodes
    std::vector<bool> originalStates;
    for (auto& n : nodes)
        originalStates.push_back(n->bypassed);

    // first click bypass
    item->onBypassChanged(item->getIndex(), true);
    EXPECT_TRUE(targetNode->bypassed);

    // second click restore
    item->onBypassChanged(item->getIndex(), false);
    EXPECT_FALSE(targetNode->bypassed);

    // check if other nodes unchanged
    for (size_t i = 1; i < nodes.size(); ++i)
        EXPECT_EQ(nodes[i]->bypassed, originalStates[i]);
}

// TC-26 Reorder Behavior
// verifies dragging moves UI items
TEST(DaisyChainTest, HandleReorderMovesItemToNewRow) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();
    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto first = nodes[0]->effectName;
        auto second = nodes[1]->effectName;

        dc.handleReorder(-1, first, 2);

        auto order = dc.getCurrentOrder();
        ASSERT_EQ(order.size(), nodes.size());
        EXPECT_EQ(order[0], second);
    }
}

// TC-27 Reorder Lock Behavior
// verifies reordering does nothing while locked
// tests flag enabling and disabling
//TEST(DaisyChainTest, ReorderLockEnablesAndDisables) {
//    AudioPluginAudioProcessor proc;
//    auto& nodes = proc.getEffectNodes();
//
//    DaisyChain dc(proc, nodes);
//
//    EXPECT_FALSE(dc.isReorderLocked());
//    dc.setReorderLocked(true);
//    EXPECT_TRUE(dc.isReorderLocked());
//    dc.setReorderLocked(false);
//    EXPECT_FALSE(dc.isReorderLocked());
//}

// TC-27 Reorder Lock Behavior
// verifies reordering is blocked when locked
// tests that order remains unchanged
TEST(DaisyChainTest, ReorderIsBlockedWhenLocked) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();
    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto before = dc.getCurrentOrder();

        dc.setReorderLocked(true);
        dc.handleReorder(-1, nodes[0]->effectName, 2);

        auto after = dc.getCurrentOrder();
        EXPECT_EQ(before, after);
    }
}

// TC-28 Reorder Unlock Behavior
// verifies reorder works again after unlock
TEST(DaisyChainTest, ReorderWorksAfterUnlock) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto first = nodes[0]->effectName;
        auto second = nodes[1]->effectName;

        dc.setReorderLocked(true);
        dc.setReorderLocked(false);

        dc.handleReorder(-1, first, 2);

        auto order = dc.getCurrentOrder();
        ASSERT_EQ(order.size(), nodes.size());
        EXPECT_EQ(order[0], second);
    }
}

// TC-29 Split Mode
// verifies dragging into right side creates split row
TEST(DaisyChainTest, SplitModeUpdatesProcessorLayout) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto left = nodes[0]->effectName;
        auto right = nodes[1]->effectName;

        dc.handleReorder(-2, right, 0);

        const auto& layout = dc.getCurrentLayout();

        ASSERT_FALSE(layout.empty());
        EXPECT_EQ(layout[0].left, left);
        EXPECT_EQ(layout[0].right, right);
    }
}

// TC-30 Double Down Behavior
// verifies two parallel nodes appear in a double row
TEST(DaisyChainTest, DoubleDownCreatesTwoParallelNodes) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto left = nodes[0]->effectName;
        auto right = nodes[1]->effectName;

        dc.handleReorder(-2, right, 0);

        const auto& layout = dc.getCurrentLayout();

        ASSERT_FALSE(layout.empty());
        EXPECT_TRUE(layout[0].hasRight());
        EXPECT_EQ(layout[0].left, left);
        EXPECT_EQ(layout[0].right, right);
    }
}

// TC-31 Unite Mode
// verifies double row collapses back into single row
TEST(DaisyChainTest, UniteModeCollapsesDoubleRow) {
    AudioPluginAudioProcessor proc;
    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);

    if (nodes.size() > 1) {
        auto left = nodes[0]->effectName;
        auto right = nodes[1]->effectName;

        dc.handleReorder(-2, right, 0);

        auto& layout = const_cast<std::vector<DaisyChain::Row>&>(dc.getCurrentLayout());
        layout[0].right = "";

        dc.rebuild();

        const auto& after = dc.getCurrentLayout();
        EXPECT_EQ(after[0].left, left);
        EXPECT_FALSE(after[0].hasRight());
    }
}

// TC-32 Mode Change Constraint Behavior
TEST(DaisyChainTest, ModeChangesBlockedByConstraints) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_GE(dc.items.size(), 2u);
    auto* item = dc.items[0];
    ASSERT_NE(item, nullptr);

    const auto originalMode = item->getChainModeId();

    // blocked when reorderLocked
    dc.setReorderLocked(true);
    item->onModeChanged(item->getIndex(), 3);  
    EXPECT_EQ(item->getChainModeId(), originalMode);

    dc.setReorderLocked(false);

    // blocked when overlay visible 
    dc.getProperties().set("ReorderLocked", true);   
    item->onModeChanged(item->getIndex(), 2);        
    EXPECT_EQ(item->getChainModeId(), originalMode);

    dc.getProperties().set("ReorderLocked", false);

    // invalid index , negative
    item->onModeChanged(-5, 4);    // unite
    EXPECT_EQ(item->getChainModeId(), originalMode);

    // invalid index , too large
    item->onModeChanged(999, 1);
    EXPECT_EQ(item->getChainModeId(), originalMode);
}


// TC-33 Add, Copy, and Delete Buttons
TEST(DaisyChainTest, AddCopyDeleteModifyBothUIAndProcessor) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    // Build initial DaisyChain from current nodes
    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_FALSE(nodes.empty());
    const auto initialCount = nodes.size();
    ASSERT_EQ(dc.items.size(), initialCount);

    // add
    {
        auto clone = nodes[0]->clone();
        clone->effectName = nodes[0]->effectName + " Added";
        nodes.push_back(clone);

        dc.resetRowsToNodes();
        dc.rebuild();
    }

    EXPECT_EQ(nodes.size(), initialCount + 1);
    EXPECT_EQ(dc.items.size(), initialCount + 1);

    // copy
    {
        const int copyIndex = 0;
        auto copied = nodes[copyIndex]->clone();
        copied->effectName = nodes[copyIndex]->effectName + " Copy";
        nodes.push_back(copied);

        dc.resetRowsToNodes();
        dc.rebuild();
    }

    EXPECT_EQ(nodes.size(), initialCount + 2);
    EXPECT_EQ(dc.items.size(), initialCount + 2);

    // delete
    {
        const auto removedName = nodes.back()->effectName;
        nodes.pop_back();

        dc.resetRowsToNodes();
        dc.rebuild();

        bool stillPresent = false;
        for (auto* it : dc.items)
        {
            ASSERT_NE(it, nullptr);
            if (it->getName() == removedName)
                stillPresent = true;
        }

        EXPECT_FALSE(stillPresent);
    }

    EXPECT_EQ(nodes.size(), initialCount + 1);
    EXPECT_EQ(dc.items.size(), initialCount + 1);
}


// TC-34 Settings and Presets Overlay Lock Behavior
TEST(DaisyChainTest, SettingsAndPresetsLockReorder) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_GE(nodes.size(), 2u);
    ASSERT_FALSE(dc.isReorderLocked());

    // base order
    auto baseline = dc.getCurrentOrder();
    ASSERT_EQ(baseline.size(), nodes.size());

	// lock order for "open settings"
    dc.setReorderLocked(true);
    EXPECT_TRUE(dc.isReorderLocked());

    // try to reorder while locked, should be ignored
    dc.handleReorder(/*rowIndex*/ -1, baseline.back(), 0);
    auto lockedOrder = dc.getCurrentOrder();
    EXPECT_EQ(lockedOrder, baseline);

	// lock order for "open Presets"
    dc.setReorderLocked(true);
    EXPECT_TRUE(dc.isReorderLocked());
}

// TC-35 Overlay Close Unlock Behavior
TEST(DaisyChainTest, ClosingOverlayUnlocksReorder) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_GE(nodes.size(), 2u);
    ASSERT_FALSE(dc.isReorderLocked());

    // base order before anything
    auto baseline = dc.getCurrentOrder();
    ASSERT_EQ(baseline.size(), nodes.size());

    // lock for open overlay
    dc.setReorderLocked(true);
    ASSERT_TRUE(dc.isReorderLocked());

    // while locked, reordering should be ignored
    dc.handleReorder(-1, baseline.back(), 0);
    auto lockedOrder = dc.getCurrentOrder();
    EXPECT_EQ(lockedOrder, baseline);

    // close overlay means unlocking
    dc.setReorderLocked(false);
    EXPECT_FALSE(dc.isReorderLocked());

    dc.handleReorder(-1, baseline.back(), 0);
    auto unlockedOrder = dc.getCurrentOrder();
    ASSERT_EQ(unlockedOrder.size(), baseline.size());
    // check  that order actually changed
    EXPECT_NE(unlockedOrder, baseline);
    EXPECT_EQ(unlockedOrder[0], baseline.back());
}


// TC-36 Global Bypass Button Behavior
TEST(DaisyChainTest, GlobalBypassUpdatesProcessorAndUI) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_GE(dc.items.size(), 1u);
    // Precondition: global bypass off
    EXPECT_FALSE(proc.isBypassed());

    // Record original enabled states
    std::vector<bool> originalEnabled;
    originalEnabled.reserve(dc.items.size());
    for (auto* item : dc.items)
    {
        ASSERT_NE(item, nullptr);
        originalEnabled.push_back(item->bypass.isEnabled());
    }

    // global bypass click
    {
        proc.setBypassed(true);           
        dc.setGlobalBypassVisual(true);   
    }

    EXPECT_TRUE(proc.isBypassed());
    for (auto* item : dc.items)
        EXPECT_FALSE(item->bypass.isEnabled());

    // click again to turn global bypass off
    {
        // Same comment as above
        proc.setBypassed(false);
        dc.setGlobalBypassVisual(false);
    }

    EXPECT_FALSE(proc.isBypassed());

    for (size_t i = 0; i < dc.items.size(); ++i)
        EXPECT_EQ(dc.items[i]->bypass.isEnabled(), originalEnabled[i]);
}

// TC-37 Per Node Bypass Preservation
TEST(DaisyChainTest, NodeBypassStatesArePreservedAfterGlobalBypassToggle) {
    AudioPluginAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    auto& nodes = proc.getEffectNodes();

    DaisyChain dc(proc, nodes);
    dc.rebuild();

    ASSERT_GE(nodes.size(), 2u);

    // set up known bypass states
    nodes[0]->bypassed = true;
    nodes[1]->bypassed = false;

    std::vector<bool> original;
    original.reserve(nodes.size());
    for (auto& n : nodes)
        original.push_back(n->bypassed);

    // enable global bypass
    proc.setBypassed(true);
    dc.setGlobalBypassVisual(true);

    EXPECT_TRUE(proc.isBypassed());

    // disable global bypass
    proc.setBypassed(false);
    dc.setGlobalBypassVisual(false);

    EXPECT_FALSE(proc.isBypassed());

    // node states should match original
    for (size_t i = 0; i < nodes.size(); ++i)
        EXPECT_EQ(nodes[i]->bypassed, original[i]);
}

// TC-38 State Load Reconstruction
TEST(DaisyChainTest, StateLoadReconstructsExactDaisyChainLayout) {
    // First processor : create save state
    AudioPluginAudioProcessor proc1;
    proc1.prepareToPlay(44100.0, 512);

    auto& nodes1 = proc1.getEffectNodes();

    DaisyChain dc1(proc1, nodes1);
    dc1.rebuild();

    ASSERT_GE(nodes1.size(), 2u);

    // layout change : reverse order
    auto layout1 = dc1.getCurrentLayout();
    std::reverse(layout1.begin(), layout1.end());
    proc1.requestLayout(toProcessorRows_ForTests(layout1));

    dc1.resetRowsToNodes();
    dc1.rebuild();

    auto orderBeforeSave = dc1.getCurrentOrder();

    juce::MemoryBlock state;
    proc1.getStateInformation(state);

    // second processor : load state, build DaisyChain, check it matches
    AudioPluginAudioProcessor proc2;
    proc2.prepareToPlay(44100.0, 512);

    proc2.setStateInformation(state.getData(), (int)state.getSize());

    auto& nodes2 = proc2.getEffectNodes();
    DaisyChain dc2(proc2, nodes2);
    dc2.rebuild();

    auto orderAfterLoad = dc2.getCurrentOrder();

    ASSERT_EQ(orderBeforeSave.size(), orderAfterLoad.size());
    EXPECT_EQ(orderBeforeSave, orderAfterLoad);
}

