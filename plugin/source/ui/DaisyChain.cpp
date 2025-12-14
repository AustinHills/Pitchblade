// reyna 
//#pragma once
#include "Pitchblade/ui/DaisyChain.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "Pitchblade/ui/DaisyChainItem.h"
#include <algorithm>

// for the add/duplicate/delete menus
#include "Pitchblade/panels/GainPanel.h"
#include "Pitchblade/panels/NoiseGatePanel.h"
#include "Pitchblade/panels/CompressorPanel.h"
#include "Pitchblade/panels/BandPassCompressorPanel.h"
#include "Pitchblade/panels/DeEsserPanel.h"
#include "Pitchblade/panels/DeNoiserPanel.h"
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/panels/PitchPanel.h"
#include "Pitchblade/panels/EqualizerPanel.h"
#include "Pitchblade/panels/AdaptiveDeNoiserPanel.h"
#include "Pitchblade/panels/SaturationPanel.h"

#include "Pitchblade/panels/VST3Panel.h"

#include "Pitchblade/panels/EffectNode.h"

// helper to make unique effect names when adding/duplicating
static juce::String makeUniqueName(const juce::String& baseName, const std::vector<std::shared_ptr<EffectNode>>& nodes) {
    // get base name by removing any existing numbers
    juce::String cleanBase = baseName.trim();

    // find last space
    int lastSpace = cleanBase.lastIndexOfChar(' ');
    if (lastSpace > 0) {
        juce::String suffix = cleanBase.substring(lastSpace + 1);

        // check if suffix is a number
        bool isNumber = true;
        for (auto c : suffix)
            if (!juce::CharacterFunctions::isDigit(c))
                isNumber = false;

        if (isNumber)
            cleanBase = cleanBase.substring(0, lastSpace); // remove the number
    }

    // make unique name
    juce::String newName = cleanBase;
    int counter = 2;

	// check if name exists
    auto nameExists = [&](const juce::String& name) {
        for (auto& n : nodes)
            if (n && n->effectName == name)
                return true;
        return false;
        };

    if (!nameExists(newName))
        return newName;

	// append numbers until unique
    while (nameExists(cleanBase + " " + juce::String(counter)))
        counter++;

	// return unique name
    return cleanBase + " " + juce::String(counter);
}

// DaisyChain constructor
DaisyChain::DaisyChain(AudioPluginAudioProcessor& proc, std::vector<std::shared_ptr<EffectNode>>& nodes) :processorRef(proc), effectNodes(nodes) {
	// add + duplicate buttons
    addAndMakeVisible(addButton);
    addAndMakeVisible(duplicateButton);
    addAndMakeVisible(deleteButton);
    addAndMakeVisible(undoButton);
    addAndMakeVisible(redoButton);

    //tooltip conection
    addButton.getProperties().set("tooltipKey", "addButton");
    duplicateButton.getProperties().set("tooltipKey", "duplicateButton");
    deleteButton.getProperties().set("tooltipKey", "deleteButton");
    undoButton.getProperties().set("tooltipKey", "undoButton");
    redoButton.getProperties().set("tooltipKey", "redoButton");

	// scroll area for effects
    addAndMakeVisible(scrollArea);
    scrollArea.setViewedComponent(&effectsContainer, false);
	// menu callbacks
    addButton.onClick = [this]() { showAddMenu(); };
    duplicateButton.onClick = [this]() { showDuplicateMenu(); };
    deleteButton.onClick = [this]() { showDeleteMenu(); };
    undoButton.onClick = [this] { processorRef.undoManager.undo(); };
    redoButton.onClick = [this] { processorRef.undoManager.redo(); };

    // Attach to the Chain value tree
    auto chain = processorRef.apvts.state.getChildWithName("Chain");
    if (chain.isValid()){
        chain.addListener(this);

        for (auto child : chain) {
            child.addListener(this);
        }
    }
    else 
        processorRef.apvts.state.addListener(this); // Fallback
        
    // Initial build
    rebuild();
}

DaisyChain::~DaisyChain() {
    auto chain = processorRef.apvts.state.getChildWithName("Chain");
    if (chain.isValid()) {
        chain.removeListener(this);
        
        // Remove listener from every child node
        for (auto child : chain)
            child.removeListener(this);
    } else {
        processorRef.apvts.state.removeListener(this);
    }
}

// check if any row has a formant / pitch effect
// only allowing one of each type in the chain. has audio bugs if multiple formant or pitch effects are present
bool DaisyChain::hasFormant() const {
    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    for (auto& n : effectNodes) {
        if (n && n->effectName.startsWith("Formant")) return true;
    }
    return false;
}

bool DaisyChain::hasPitch() const {
    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    for (auto& n : effectNodes) {
        if (n && n->effectName.startsWith("Pitch")) return true;
    }
    return false;
}

// helper to find node by name
std::shared_ptr<EffectNode> DaisyChain::findNodeByName(const juce::String& name) const {
	std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());    // lock for thread safety
    for (auto& n : effectNodes)
		if (n && n->effectName == name)    // check name
            return n;
    return {};
}
// helper to convert chainmode to modeId for ui
static int toModeIdFromNode(const std::shared_ptr<EffectNode>& n) {
    if (!n) return 1;
    const int id = (int)n->chainMode;
    return juce::jlimit(1, 4, id);
}

//helper to find name in rows - rowIndex, isRightCell, found
static std::tuple<int, bool, bool> findRowAndSide(const std::vector<DaisyChain::Row>& rows, const juce::String& name) {
    for (int i = 0; i < (int)rows.size(); ++i) {
		// check left and right
        if (rows[i].left == name)  return { i, false, true };
        if (rows[i].right == name) return { i, true , true };
    }
    return { -1, false, false };
}

// rebuilds the UI from current rows and effectNodes
void DaisyChain::rebuild() {
    for (auto* it : items) effectsContainer.removeChildComponent(it);
    items.clear(true);

    auto& nodes = processorRef.getEffectNodes();
    DaisyChainItem* currentRow = nullptr;
    
    for (int i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        if (!node) continue;
        
        bool isRightSide = false;
        
        // Logic: A node is on the Right Side ONLY if:
        // 1. We have a current row
        // 2. That row has space
        // 3. This node is strictly ChainMode::DoubleDown (3)
        // (LeftDouble (5) must start a new row)
        if (currentRow != nullptr && !currentRow->hasRight && node->chainMode == ChainMode::DoubleDown) {
            isRightSide = true;
        }
        
        if (isRightSide) {
            // Add to Right
            currentRow->setSecondaryEffect(node->effectName);
            currentRow->updateSecondaryBypassVisual(node->bypassed);
            
            currentRow->onSecondaryBypassChanged = [this, node](int index, bool b) {
                node->bypassed = b;
                if (onAnyBypassChanged) onAnyBypassChanged();
            };
        } else {
            // Start New Row
            currentRow = new DaisyChainItem(node->effectName, items.size());
            effectsContainer.addAndMakeVisible(currentRow);
            items.add(currentRow);

            //Connect context menu
            currentRow->onContextMenu = [this](int idx, bool right) {
                showContextMenu(idx, right);
            };
            
            currentRow->updateBypassVisual(node->bypassed);
            
            // Set Visuals (D, S, DD, U)
            currentRow->setChainModeId(static_cast<int>(node->chainMode));
            currentRow->updateModeVisual();

            currentRow->onReorder = [this](int kind, juce::String name, int targetRow) {
                handleReorder(kind, name, targetRow);
            };

            currentRow->onBypassChanged = [this, node](int index, bool b) {
                node->bypassed = b;
                if (onAnyBypassChanged) onAnyBypassChanged();
            };

            currentRow->onModeChanged = [this, node](int index, int modeId) {
                if (isReorderLocked()) return; // check lock here or in showModeMenu

                // Since we don't have a showModeMenu method yet (it seems), we might implement toggling
                // or just accept the modeId if it comes from the UI badge.
                // The test calls onModeChanged(..., 3).
                // DaisyChainItem modeButton usually doesn't emit mode ID, it just clicks?
                // Wait, DaisyChainItem has no onClick for modeButton.
                // NOTE: I am adding this handler to satisfy the test interactions, 
                // assuming the test manually triggers this callback.
                // But normally this loopback would close the loop from UI -> Logic.
                
                // If the test manually invokes onModeChanged, we must update the node.
                node->chainMode = static_cast<ChainMode>(modeId);
                // Then propagate to state (which triggers listeners -> rebuild)
                node->getMutableNodeState().setProperty("chainMode", modeId, &processorRef.undoManager);
            };
        }
    }

    resized();
    repaint();
}

//reorders the global effects list and rebuilds UI
void DaisyChain::handleReorder(int kind, const juce::String& dragName, int targetRow) {
    if (reorderLocked) return;

    auto chain = processorRef.apvts.state.getChildWithName("Chain");
    
    // 1. Find index of dragged node in ValueTree
    int oldIndex = -1;
    for (int i = 0; i < chain.getNumChildren(); ++i) {
        if (chain.getChild(i).getProperty("name") == dragName) {
            oldIndex = i;
            break;
        }
    }
    if (oldIndex == -1) return;

    processorRef.undoManager.beginNewTransaction();

    // 2. Cleanup Old Neighbors (Fix broken double rows before move)
    // If we move a node, its previous partner (if any) becomes an orphan.
    {
        auto draggedNode = chain.getChild(oldIndex);
        int currentMode = (int)draggedNode.getProperty("chainMode");
        
        // If we are moving the LEFT side of a double row, fix the RIGHT side
        if (currentMode == 5 /*LeftDouble*/) {
            if (oldIndex + 1 < chain.getNumChildren()) {
                auto rightPartner = chain.getChild(oldIndex + 1);
                if ((int)rightPartner.getProperty("chainMode") == 3 /*DoubleDown*/) {
                    rightPartner.setProperty("chainMode", 1, &processorRef.undoManager);
                }
            }
        }
        // If we are moving the RIGHT side of a double row, fix the LEFT side
        else if (currentMode == 3 /*DoubleDown*/) {
            if (oldIndex - 1 >= 0) {
                auto leftPartner = chain.getChild(oldIndex - 1);
                if ((int)leftPartner.getProperty("chainMode") == 5 /*LeftDouble*/) {
                    leftPartner.setProperty("chainMode", 1, &processorRef.undoManager);
                }
            }
        }
    }

    if (kind == -2) {
        // === Drop to RIGHT (Make Parallel) ===
        // 1. Find the ValueTree index of the node we are dropping ONTO
        int targetVTIndex = -1;
        if (targetRow < items.size()) {
            DaisyChainItem* targetItem = items[targetRow];
            juce::String name = targetItem->getName();
            for (int i = 0; i < chain.getNumChildren(); ++i) {
                if (chain.getChild(i).getProperty("name") == name) {
                    targetVTIndex = i;
                    break;
                }
            }
        }
        
        if (targetVTIndex == -1) return; // Should not happen for side-drop

        auto draggedNode = chain.getChild(oldIndex);
        
        // Set the Target (Left) node to LeftDouble
        auto targetNode = chain.getChild(targetVTIndex);
        targetNode.setProperty("chainMode", 5 /*LeftDouble*/, &processorRef.undoManager);

        // Set Dragged (Right) node to DoubleDown
        draggedNode.setProperty("chainMode", 3 /*DoubleDown*/, &processorRef.undoManager);

        // Insert Index is strictly After the target
        int insertIndex = targetVTIndex + 1;
        
        // FIX: Off-by-one check for moveChild
        if (oldIndex < insertIndex) insertIndex--;

        chain.moveChild(oldIndex, insertIndex, &processorRef.undoManager);
    } 
    else {
        // === Drop Insert (Vertical) ===
        
        // 1. Convert UI Row to ValueTree Index
        int targetVTIndex = -1;
        
        if (targetRow >= items.size()) {
             // Append to end
             targetVTIndex = chain.getNumChildren();
        } else {
             // Insert BEFORE the node at targetRow
             DaisyChainItem* targetItem = items[targetRow];
             juce::String name = targetItem->getName();
             
             for (int i = 0; i < chain.getNumChildren(); ++i) {
                if (chain.getChild(i).getProperty("name") == name) {
                    targetVTIndex = i;
                    break;
                }
             }
             if (targetVTIndex == -1) targetVTIndex = chain.getNumChildren();
        }

        auto draggedNode = chain.getChild(oldIndex);
        
        // Reset mode to Down (1) to ensure it doesn't try to be a double row
        draggedNode.setProperty("chainMode", 1, &processorRef.undoManager); 

        // FIX: Off-by-one check for moveChild
        // When moving an item down the list, we must decrement the target index
        // because removing the item at 'oldIndex' shifts subsequent items up.
        if (oldIndex < targetVTIndex) {
            targetVTIndex--;
        }
        
        chain.moveChild(oldIndex, targetVTIndex, &processorRef.undoManager);
    }

    processorRef.undoManager.beginNewTransaction();
}

// layout the daisy chain component
void DaisyChain::resized() {
	// main area with padding
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(10);
    area.removeFromRight(8);

	//add + duplicate buttons + delete top bar
    auto topBar = area.removeFromTop(40);
    const int buttonWidth = 50;
    const int spacing = 4;

    auto rightEdge = getWidth() - 23;
    deleteButton.setBounds(rightEdge - buttonWidth, topBar.getY() + 4, buttonWidth, topBar.getHeight() - 8);
    duplicateButton.setBounds(deleteButton.getX() - buttonWidth - spacing, deleteButton.getY(), buttonWidth, deleteButton.getHeight());
    addButton.setBounds(duplicateButton.getX() - buttonWidth - spacing, duplicateButton.getY(), buttonWidth, duplicateButton.getHeight());

    area.removeFromBottom(4);
    auto bottomBar = area.removeFromBottom(24); 
    
    int availWidth = bottomBar.getWidth(); 
    int undoWidth = (availWidth - spacing) / 2;
    
    undoButton.setBounds(bottomBar.getX(), bottomBar.getY(), undoWidth, bottomBar.getHeight());
    redoButton.setBounds(bottomBar.getX() + undoWidth + spacing, bottomBar.getY(), undoWidth, bottomBar.getHeight());

    // scrollable list area
    auto scrollBounds = area;
    scrollArea.setBounds(scrollBounds);

    // only vertical scrollbar
    scrollArea.setScrollBarsShown(true, false);

    // scrollbar
    juce::ScrollBar& scrollBar = scrollArea.getVerticalScrollBar();
    const int scrollBarWidth = 6;
    scrollArea.setScrollBarsShown(true, false);
    scrollArea.setScrollBarThickness(scrollBarWidth);
    scrollArea.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, juce::Colours::grey);
    scrollArea.getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId, Colors::panel);

    scrollArea.setViewedComponent(&effectsContainer, false);

    const int listRightPadding = scrollBarWidth + 28; // a little gap next to the bar

	// layout each row in the effects container
    const int singleH = 56;   // height for single
	const int doubleH = 86;   // double down height
    const int width = scrollArea.getWidth() - 8;

    // layout each item
    int y = 0;
    for (int i = 0; i < items.size(); ++i) { 	
        auto* item = items[i];
        if (!item) continue;

		const bool isDouble = item->isDoubleRow;  // check if double row
        const int width = scrollArea.getWidth() - 8;

        if (item->isDoubleRow) {
            item->setBounds(0, y, width, doubleH);
            y += doubleH;
        } else {
            item->setBounds(0, y, width, singleH);
            y += singleH;
        }
    }
    // set the container to  height
    effectsContainer.setBounds(0, 0, scrollArea.getWidth(), y);
}

// paint the daisy chain background and arrows
void DaisyChain::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        Colors::panel,
        r.getX(), r.getY(),
        Colors::panel.darker(0.3f),
        r.getX(), r.getBottom(),
        false
    );

    g.setGradientFill(gradient);
    g.fillRect(r);
    g.drawRect(getLocalBounds(), 2);

    if (items.size() <= 1)
        return;

    // grey overlay when reordering is locked
    if (reorderLocked) {
        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.fillAll();
    }

    // Arrow Helpers (Keep these as they were)
    auto drawDownArrow = [&](juce::Graphics& gr, juce::Point<float> c) {
        juce::Path p;
        p.startNewSubPath(c.x - 5, c.y - 5);
        p.lineTo(c.x, c.y + 5);
        p.lineTo(c.x + 5, c.y - 5);
        p.closeSubPath();
        gr.setColour(Colors::accentTeal);
        gr.fillPath(p);
    };

    auto drawSplitArrow = [&](juce::Graphics& gr, juce::Point<float> c) {
        juce::Path p;
        p.startNewSubPath(c.x, c.y - 5);
        p.lineTo(c.x - 5, c.y + 5);
        p.startNewSubPath(c.x, c.y - 5);
        p.lineTo(c.x + 5, c.y + 5);
        gr.setColour(Colors::accentPink);
        gr.strokePath(p, juce::PathStrokeType(2.0f));
    };

    auto drawDoubleDownArrows = [&](juce::Graphics& gr, juce::Point<float> c) {
        juce::Path p;
        float height = 10.0f;
        float spacing = 10.0f;
        float lineWidth = 2.0f;
        p.startNewSubPath(c.x - spacing / 2, c.y - height / 2);
        p.lineTo(c.x - spacing / 2, c.y + height / 2);
        p.startNewSubPath(c.x + spacing / 2, c.y - height / 2);
        p.lineTo(c.x + spacing / 2, c.y + height / 2);
        gr.setColour(Colors::accentPurple);
        gr.strokePath(p, juce::PathStrokeType(lineWidth));
    };

    auto drawUniteArrow = [&](juce::Graphics& gr, juce::Point<float> c) {
        juce::Path p;
        c.y -= 2.0f;
        p.startNewSubPath(c.x - 5, c.y - 5);
        p.lineTo(c.x, c.y + 5);
        p.lineTo(c.x + 5, c.y - 5);
        gr.setColour(Colors::accentBlue);
        gr.strokePath(p, juce::PathStrokeType(2.0f));
    };

    // Drawing arrows between items (using UI items instead of rows vector)
    const int count = items.size();
    for (int i = 0; i + 1 < count; ++i) {
        DaisyChainItem* cur = items[i];
        DaisyChainItem* next = items[i + 1];
        if (!cur || !next) continue;

        // midpoint 
        juce::Point<int> curBottom = getLocalPoint(cur, juce::Point<int>(cur->getWidth() / 2, cur->getHeight()));
        juce::Point<int> nextTop = getLocalPoint(next, juce::Point<int>(next->getWidth() / 2, 0));

        float xMid = 0.5f * (curBottom.x + nextTop.x);
        float yMid = 0.5f * (curBottom.y + nextTop.y);
        xMid += 2.0f; 
        juce::Point<float> mid(xMid, yMid);

        bool thisIsDouble = cur->isDoubleRow;
        bool nextIsDouble = next->isDoubleRow;

        if (thisIsDouble && nextIsDouble)       drawDoubleDownArrows(g, mid);
        else if (thisIsDouble && !nextIsDouble) drawUniteArrow(g, mid);
        else if (!thisIsDouble && nextIsDouble) drawSplitArrow(g, mid);
        else                                    drawDownArrow(g, mid);
    }
}

// flatten current rows into single list of effect names
std::vector<juce::String> DaisyChain::getCurrentOrder() const {
    std::vector<juce::String> flat;
    for (auto* item : items) {
        if (!item) continue;
        flat.push_back(item->getName());
        if (!item->rightEffectName.isEmpty())
            flat.push_back(item->rightEffectName);
    }
    return flat;
}

// grey out individual bypass when global bypassed
void DaisyChain::setGlobalBypassVisual(bool state) {
	// now just store the state and apply to all items
    globalBypassed = state;

	// apply to all items , skip nulls
    for (auto* row : items) {
        if (!row) continue;

        // left bypass
        row->bypass.setEnabled(!state);
        row->bypass.setAlpha(state ? 0.5f : 1.0f);

        // right bypass
        if (row->isDoubleRow) {
            row->rightBypass.setEnabled(!state);
            row->rightBypass.setAlpha(state ? 0.5f : 1.0f);
        }

        // mode buttons
        row->modeButton.setEnabled(!state);
        row->modeButton.setAlpha(state ? 0.5f : 1.0f);

        if (row->isDoubleRow) {
            row->rightMode.setEnabled(!state);
            row->rightMode.setAlpha(state ? 0.5f : 1.0f);
        }

        // open effect buttons
        row->button.setEnabled(!state);
        row->button.setAlpha(state ? 0.5f : 1.0f);

        if (row->isDoubleRow) {
            row->rightButton.setEnabled(!state);
            row->rightButton.setAlpha(state ? 0.5f : 1.0f);
        }
    }
}

// enable/disable chain controls (for locking during preset/settings)
void DaisyChain::setChainControlsEnabled(bool enabled) {
    // stops unlocking if lockBypass is active
    if (globalBypassed && enabled)
        return;

    addButton.setEnabled(enabled);
    duplicateButton.setEnabled(enabled);
    deleteButton.setEnabled(enabled);
    
	// enable/disable all chain item interactions
    for (auto* item : items) {
        item->setInterceptsMouseClicks(enabled, enabled);
        item->grip.setInterceptsMouseClicks(enabled, enabled);
        item->rightGrip.setInterceptsMouseClicks(enabled, enabled);
    }
}

// lock reordering and drag/drop when viewing settings/presets
void DaisyChain::setReorderLocked(bool locked) {
    // stops unlocking if lockBypass is active
    if (globalBypassed && !locked)
        return;

    reorderLocked = locked;
    
	// store in properties
    getProperties().set("ReorderLocked", locked);

    // disable add/copy/delete
    addButton.setEnabled(!locked);
    duplicateButton.setEnabled(!locked);
    deleteButton.setEnabled(!locked);

    // disable all chain item interactions
    for (auto* row : items) {
        if (!row) continue;
        // disable drag and all clicks
        row->setInterceptsMouseClicks(!locked, !locked);
        row->bypass.setEnabled(!locked);
        row->modeButton.setEnabled(!locked);
        row->button.setEnabled(!locked);
        row->rightButton.setEnabled(!locked);
        row->rightBypass.setEnabled(!locked);
        row->rightMode.setEnabled(!locked);

        if (locked) {
            // gray-out when locked 
            row->button.setAlpha(0.6f);
            row->rightButton.setAlpha(0.6f);
            row->modeButton.setAlpha(0.6f);
            row->rightMode.setAlpha(0.6f);
            row->bypass.setAlpha(0.6f);
            row->rightBypass.setAlpha(0.6f);
        } else {
            // restore full opacity
            row->button.setAlpha(1.0f);
            row->rightButton.setAlpha(1.0f);
            row->modeButton.setAlpha(1.0f);
            row->rightMode.setAlpha(1.0f);
            row->bypass.setAlpha(1.0f);
            row->rightBypass.setAlpha(1.0f);

            // clear colour overrides
            row->modeButton.removeColour(juce::TextButton::buttonColourId);
            row->rightMode.removeColour(juce::TextButton::buttonColourId);
            row->button.removeColour(juce::TextButton::buttonColourId);
            row->rightButton.removeColour(juce::TextButton::buttonColourId);

            // re-apply colors
            row->updateModeVisual();                               // chain-mode colours
            row->updateBypassVisual(row->bypassed);                // left bypass colour
            if (row->isDoubleRow) {
                row->updateSecondaryBypassVisual(row->rightBypassed); // right bypass colour
                row->updateRightModeVisual();                       // update right mode button color    
            }
        }
    }
    repaint();

}

//////////////////////////////////// menus ///////////////////////////////////////////////////////////

// menu to add new effect nodes
void DaisyChain::showAddMenu() {
	if (reorderLocked) return;  // prevent adding if locked

	// check existing formant/pitch
    const bool formantExists = hasFormant();
    const bool pitchExists = hasPitch();

	juce::PopupMenu menu;       // create menu manually add all effects
    menu.addItem(1, "Gain");
    menu.addItem(2, "Noise Gate");
    menu.addItem(3, "Compressor");
    menu.addItem(4, "Band-Pass Compressor");
    menu.addItem(5, "De-Esser");
    menu.addItem(6, "De-Noiser");
    menu.addItem(7, "Adaptive De-Noiser");
    menu.addItem(8, "Formant",  !formantExists);    // disable when one already exists
    menu.addItem(9, "Pitch",    !pitchExists);
    menu.addItem(10, "Equalizer");
    menu.addItem(11, "Saturation");
    menu.addItem(99, "VST3");

	// set look and feel
    menu.setLookAndFeel(&getLookAndFeel());
    addButton.setColour(juce::TextButton::buttonColourId, Colors::accent);

	// show menu async
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton), [this](int result) {
        addButton.setColour(juce::TextButton::buttonColourId, Colors::button);
        if (result == 0) return;
        
        // Define Type string based on result
        juce::String type;
        juce::String baseName;
        switch (result) {
            case 1: type="GainNode"; baseName="Gain"; break;
            case 2: type="NoiseGateNode"; baseName="Noise Gate"; break;
            case 3: type="CompressorNode"; baseName="Compressor"; break;
            case 4: type="BandPassCompressorNode"; baseName="Band-Pass Compressor"; break;
            case 5: type="DeEsserNode"; baseName="De-Esser"; break;
            case 6: type="DeNoiserNode"; baseName="De-Noiser"; break;
            case 7: type="AdaptiveDeNoiserNode"; baseName="Adaptive De-Noiser"; break;
            case 8: type="FormantNode"; baseName="Formant"; break;
            case 9: type="PitchNode"; baseName="Pitch"; break;
            case 10: type="EqualizerNode"; baseName="Equalizer"; break;
            case 11: type="SaturationNode"; baseName="Saturation"; break;
            case 99: type="VST3Node"; baseName="VST3"; break;
        }

        if (type.isEmpty()) return;

        // Create the ValueTree for the new node
        juce::ValueTree newNode(type);
        
        // Calculate unique name (reuse your existing helper, pass effectNodes)
        juce::String uniqueName = makeUniqueName(baseName, effectNodes);
        newNode.setProperty("name", uniqueName, nullptr);
        newNode.setProperty("uuid", juce::Uuid().toString(), nullptr);

        // DO NOT create EffectNode class here. DO NOT push to vector.
        // DO NOT call requestLayout.
        
        // Simply add to APVTS state with UndoManager
        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        chain.addChild(newNode, -1, &processorRef.undoManager);
        
        // The listener in PluginProcessor will instantiate the C++ object.
        // The listener in DaisyChain will call rebuild().

        processorRef.undoManager.beginNewTransaction();
    });
}

// menu to duplicate existing effect nodes
void DaisyChain::showDuplicateMenu() {
    if (reorderLocked) return;  // prevent adding if locked

    // check existing formant/pitch
    const bool formantExists = hasFormant();    
    const bool pitchExists = hasPitch();

    // create menu with existing effect names
    juce::PopupMenu menu;
    for (int i = 0; i < effectNodes.size(); ++i) {
        if (!effectNodes[i]) continue;
        const auto& name = effectNodes[i]->effectName;

        // disable if formant/pitch already exists
        bool disable = false;
        if (name.startsWith("Formant") && formantExists) disable = true;
        if (name.startsWith("Pitch") && pitchExists) disable = true;

        menu.addItem(i + 1, name, !disable);
    }
    
    // set look and feel
    menu.setLookAndFeel(&getLookAndFeel());
    duplicateButton.setColour(juce::TextButton::buttonColourId, Colors::accent);

    // show menu async
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&duplicateButton), [this](int result) {
        duplicateButton.setColour(juce::TextButton::buttonColourId, Colors::button);

        if (result == 0) return;
        const int index = result - 1;
        if (index < 0 || index >= effectNodes.size()) return;

        auto original = effectNodes[index];
        if (!original) return;

        // 1. Create a deep copy of the existing state
        juce::ValueTree originalState = original->getNodeStateConst();
        juce::ValueTree newState = originalState.createCopy();

        // 2. Assign new UUID so it is treated as a unique object
        newState.setProperty("uuid", juce::Uuid().toString(), nullptr);

        // 3. Generate new unique Name
        // We use the helper to ensure we don't get duplicate names like "Gain 2 2"
        juce::String currentName = originalState.getProperty("name").toString();
        juce::String newName = makeUniqueName(currentName, effectNodes);
        newState.setProperty("name", newName, nullptr);

        // 4. Add to APVTS state with UndoManager
        // This single line triggers the ValueTree listener in PluginProcessor, 
        // which creates the Node, updates the vector, and triggers the UI rebuild.
        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        chain.addChild(newState, -1, &processorRef.undoManager);

        processorRef.undoManager.beginNewTransaction();
    });
}

// menu to delete existing effect nodes
void DaisyChain::showDeleteMenu() {
    if (reorderLocked) return;

    juce::PopupMenu menu;
    for (int i = 0; i < effectNodes.size(); ++i) {
        menu.addItem(i + 1, effectNodes[i]->effectName);
    }

    menu.setLookAndFeel(&getLookAndFeel());
    deleteButton.setColour(juce::TextButton::buttonColourId, Colors::accent);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&deleteButton), [this](int result) {
        deleteButton.setColour(juce::TextButton::buttonColourId, Colors::button);
        if (result == 0) return;
        
        const int index = result - 1;
        if (index < 0 || index >= effectNodes.size()) return;

        // Get the ValueTree corresponding to this node
        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        auto child = chain.getChild(index);
        
        if (child.isValid()) {
            // FIX: Unbind the partner node before deleting to prevent accidental merging.
            
            // We use effectNodes (C++ objects) to check the current topology
            // because they hold the calculated ChainMode (LeftDouble vs DoubleDown).
            if (index < effectNodes.size()) {
                auto node = effectNodes[index];
                
                // Case 1: Deleting the LEFT node of a pair
                // The survivor is on the RIGHT (index + 1).
                if (node->chainMode == ChainMode::LeftDouble) {
                     if (index + 1 < effectNodes.size()) {
                         auto nextNode = effectNodes[index + 1];
                         // If the next node is indeed the partner (DoubleDown)
                         if (nextNode->chainMode == ChainMode::DoubleDown) {
                             // Reset the survivor's property to Down (1)
                             // This stops it from snapping to the row above.
                             auto nextChild = chain.getChild(index + 1);
                             nextChild.setProperty("chainMode", 1, &processorRef.undoManager);
                         }
                     }
                }
                // Case 2: Deleting the RIGHT node of a pair
                // The survivor is on the LEFT (index - 1).
                else if (node->chainMode == ChainMode::DoubleDown) {
                    if (index - 1 >= 0) {
                        auto prevNode = effectNodes[index - 1];
                        // If the prev node is the partner (LeftDouble)
                        if (prevNode->chainMode == ChainMode::LeftDouble) {
                             // Reset the survivor to Down (1)
                             auto prevChild = chain.getChild(index - 1);
                             prevChild.setProperty("chainMode", 1, &processorRef.undoManager);
                        }
                    }
                }
            }

            // Now delete the target node
            chain.removeChild(child, &processorRef.undoManager);

            processorRef.undoManager.beginNewTransaction();
        }
    });
}

void DaisyChain::valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& child) {
    // Start listening to the new child node immediately
    child.addListener(this);
    rebuild();
}

void DaisyChain::valueTreeChildRemoved(juce::ValueTree& parentTree, juce::ValueTree& child, int) {
    // Stop listening to the removed child node
    child.removeListener(this);
    rebuild();
}

void DaisyChain::valueTreeChildOrderChanged(juce::ValueTree&, int, int) {
    rebuild();
}

void DaisyChain::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    // When the VST3 finishes loading, it updates its "name" property.
    // We catch that here and trigger a UI rebuild.
    if (property.toString() == "name") {
        juce::MessageManager::callAsync([this]() { rebuild(); });
    }
}

//Context menu handler
void DaisyChain::showContextMenu(int index, bool isRightSide) {
    if (reorderLocked) return;

    // 1. Find the target row item
    DaisyChainItem* row = getItem(index);
    if (!row) return;

    // 2. Determine effect name based on side
    juce::String effectName = isRightSide ? row->rightEffectName : row->getName();
    
    // 3. Find the actual EffectNode
    auto node = findNodeByName(effectName);
    if (!node) return;

    // 4. Build Menu
    juce::PopupMenu menu;
    menu.setLookAndFeel(&getLookAndFeel());

    // -- Bypass --
    menu.addItem("Bypass", true, node->bypassed, [this, node, row, isRightSide]() {
        node->bypassed = !node->bypassed;
        
        if (isRightSide) row->updateSecondaryBypassVisual(node->bypassed);
        else             row->updateBypassVisual(node->bypassed);

        if (onAnyBypassChanged) onAnyBypassChanged();
    });

    menu.addSeparator();

    // -- Duplicate --
    // Check constraints for single-instance effects
    bool canDuplicate = true;
    if (node->effectName.startsWith("Formant") && hasFormant()) canDuplicate = false;
    if (node->effectName.startsWith("Pitch") && hasPitch()) canDuplicate = false;

    menu.addItem("Duplicate", canDuplicate, false, [this, node]() {
        juce::ValueTree originalState = node->getNodeStateConst();
        juce::ValueTree newState = originalState.createCopy();
        newState.setProperty("uuid", juce::Uuid().toString(), nullptr);
        
        // Generate unique name
        juce::String currentName = originalState.getProperty("name").toString();
        juce::String newName = makeUniqueName(currentName, effectNodes);
        newState.setProperty("name", newName, nullptr);

        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        chain.addChild(newState, -1, &processorRef.undoManager);
        processorRef.undoManager.beginNewTransaction();
    });

    // -- Reset to Default --
    // Implemented by removing current and adding a fresh one of same type
    menu.addItem("Reset to Default", [this, node]() {
        juce::String type = node->getNodeStateConst().getType().toString();
        juce::String name = node->effectName; 
        
        // Correctly parse base name by removing trailing numbers only
        juce::String baseName = name.trim();
        int lastSpace = baseName.lastIndexOfChar(' ');
        if (lastSpace > 0) {
            juce::String suffix = baseName.substring(lastSpace + 1);
            if (suffix.containsOnly("0123456789")) {
                baseName = baseName.substring(0, lastSpace);
            }
        }
        // If no number found, baseName remains equal to name (e.g. "Noise Gate")

        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        
        // Find index in ValueTree
        int vtIndex = -1;
        for(int i=0; i<chain.getNumChildren(); ++i) {
            if(chain.getChild(i).getProperty("name") == node->effectName) {
                vtIndex = i;
                break;
            }
        }
        
        if (vtIndex >= 0) {
            processorRef.undoManager.beginNewTransaction();

            // Remove old
            chain.removeChild(vtIndex, &processorRef.undoManager);
            
            // Create fresh
            juce::ValueTree newNode(type);
            juce::String newName = makeUniqueName(baseName, effectNodes);
            newNode.setProperty("name", newName, nullptr);
            newNode.setProperty("uuid", juce::Uuid().toString(), nullptr);
            
            // Insert at same location
            chain.addChild(newNode, vtIndex, &processorRef.undoManager);
            processorRef.undoManager.beginNewTransaction();
        }
    });

    menu.addSeparator();

    // -- Delete --
    menu.addItem("Delete", [this, node]() {
        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        
        // Find index
        int vtIndex = -1;
        for(int i=0; i<chain.getNumChildren(); ++i) {
            if(chain.getChild(i).getProperty("name") == node->effectName) {
                vtIndex = i;
                break;
            }
        }

        if (vtIndex >= 0) {
            auto child = chain.getChild(vtIndex);
            
            // Handle Double Row Logic (Prevent merging bugs)
            if (node->chainMode == ChainMode::LeftDouble) {
                 if (vtIndex + 1 < effectNodes.size()) {
                     auto nextNode = effectNodes[vtIndex + 1];
                     if (nextNode->chainMode == ChainMode::DoubleDown) {
                         auto nextChild = chain.getChild(vtIndex + 1);
                         nextChild.setProperty("chainMode", 1, &processorRef.undoManager);
                     }
                 }
            }
            else if (node->chainMode == ChainMode::DoubleDown) {
                if (vtIndex - 1 >= 0) {
                    auto prevNode = effectNodes[vtIndex - 1];
                    if (prevNode->chainMode == ChainMode::LeftDouble) {
                         auto prevChild = chain.getChild(vtIndex - 1);
                         prevChild.setProperty("chainMode", 1, &processorRef.undoManager);
                    }
                }
            }
            
            chain.removeChild(child, &processorRef.undoManager);
            processorRef.undoManager.beginNewTransaction();
        }
    });

    menu.showMenuAsync(juce::PopupMenu::Options());
}