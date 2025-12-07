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
#include "Pitchblade/panels/DeEsserPanel.h"
#include "Pitchblade/panels/DeNoiserPanel.h"
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/panels/PitchPanel.h"
#include "Pitchblade/panels/EqualizerPanel.h"

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

    //tooltip conection
    addButton.getProperties().set("tooltipKey", "addButton");
    duplicateButton.getProperties().set("tooltipKey", "duplicateButton");
    deleteButton.getProperties().set("tooltipKey", "deleteButton");

	// scroll area for effects
    addAndMakeVisible(scrollArea);
    scrollArea.setViewedComponent(&effectsContainer, false);
	// menu callbacks
    addButton.onClick = [this]() { showAddMenu(); };
    duplicateButton.onClick = [this]() { showDuplicateMenu(); };
    deleteButton.onClick = [this]() { showDeleteMenu(); };

    // Attach to the Chain value tree
    auto chain = processorRef.apvts.state.getChildWithName("Chain");
    if (chain.isValid())
        chain.addListener(this);
    else 
        processorRef.apvts.state.addListener(this); // Fallback
        
    // Initial build
    rebuild();
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
    // Clear UI
    for (auto* it : items) effectsContainer.removeChildComponent(it);
    items.clear(true);

    // Get the authoritative list from processor (which is synced to VT)
    // Or iterate the VT directly. Let's use effectNodes as it contains the params/bypass state objects.
    auto& nodes = processorRef.getEffectNodes();
    
    // Simple Linear Rebuild (Every node is a row)
    // To restore "Double Row" logic, you would iterate nodes and check chainMode.
    // If current node is DoubleDown, it appends to previous row instead of making new one.
    
    DaisyChainItem* currentRow = nullptr;
    
    for (int i = 0; i < nodes.size(); ++i) {
        auto node = nodes[i];
        if (!node) continue;
        
        bool isRightSide = false;
        
        // Determine if this should be on the right side of the previous row
        // This requires your nodes to persist their ChainMode correctly in the ValueTree
        if (currentRow != nullptr && node->chainMode == ChainMode::DoubleDown) {
            isRightSide = true;
        }
        
        if (isRightSide) {
            // Add to existing row
            currentRow->setSecondaryEffect(node->effectName);
            currentRow->updateSecondaryBypassVisual(node->bypassed);
            // ... setup right side callbacks ...
        } else {
            // New Row
            currentRow = new DaisyChainItem(node->effectName, i);
            effectsContainer.addAndMakeVisible(currentRow);
            items.add(currentRow);
            
            currentRow->updateBypassVisual(node->bypassed);
            // ... setup left side callbacks ...
        }
    }

    resized();
    repaint();
}

//reorders the global effects list and rebuilds UI
void DaisyChain::handleReorder(int kind, const juce::String& dragName, int targetRow) {
    if (reorderLocked) return;

    auto chain = processorRef.apvts.state.getChildWithName("Chain");
    
    // Find index of dragged node in ValueTree
    int oldIndex = -1;
    for (int i = 0; i < chain.getNumChildren(); ++i) {
        if (chain.getChild(i).getProperty("name") == dragName) {
            oldIndex = i;
            break;
        }
    }
    
    if (oldIndex == -1) return;

    // Map targetRow (UI row index) to linear index in ValueTree
    // Since rebuild() maps ValueTree children linearly to rows (mostly), 
    // we can approximate the target index.
    int newIndex = juce::jlimit(0, chain.getNumChildren() - 1, targetRow);
    
    // Check if target is actually a "Double Row" slot (kind == -2)
    // For now, standard reorder:
    if (oldIndex != newIndex) {
        chain.moveChild(oldIndex, newIndex, &processorRef.undoManager);
    }
    
    // If double row logic is needed here (combining nodes):
    if (kind == -2) {
        // Logic to set "chainMode" property to DoubleDown on the target node
        // would go here, interacting with processorRef.undoManager
    }
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
    menu.addItem(4, "De-Esser");
    menu.addItem(5, "De-Noiser");
    menu.addItem(6, "Formant",  !formantExists);    // disable when one already exists
    menu.addItem(7, "Pitch",    !pitchExists);
    menu.addItem(8, "Equalizer");
    menu.addItem(9, "VST3");

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
            case 4: type="DeEsserNode"; baseName="De-Esser"; break;
            case 5: type="DeNoiserNode"; baseName="De-Noiser"; break;
            case 6: type="FormantNode"; baseName="Formant"; break;
            case 7: type="PitchNode"; baseName="Pitch"; break;
            case 8: type="EqualizerNode"; baseName="Equalizer"; break;
            case 9: type="VST3Node"; baseName="VST3"; break;
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
        // We assume effectNodes vector is in sync with Chain children order
        auto chain = processorRef.apvts.state.getChildWithName("Chain");
        auto child = chain.getChild(index);
        
        if (child.isValid()) {
            chain.removeChild(child, &processorRef.undoManager);
        }
    });
}

