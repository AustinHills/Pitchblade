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

#include "Pitchblade/panels/EffectNode.h"

// helper to make unique effect names when adding/duplicating
static juce::String makeUniqueName(const juce::String& baseName, const std::vector<std::shared_ptr<EffectNode>>& nodes) {
    int counter = 1;
    juce::String newName = baseName;
	// lambda to check if name exists
    auto nameExists = [&](const juce::String& name) {
            for (auto& n : nodes)
				if (n && n->effectName == name)     // check against effect names in nodes
                    return true;
            return false;
        };
	
    while (nameExists(newName)) {
        newName = baseName + " " + juce::String(++counter);  } // ++ until unique
    return newName;
}

//convert UI rows into processor rows
static std::vector<AudioPluginAudioProcessor::Row> toProcessorRows(const std::vector<DaisyChain::Row>& uiRows) {
    std::vector<AudioPluginAudioProcessor::Row> out;
    out.reserve(uiRows.size());
    for (auto& r : uiRows)
        out.push_back({ r.left, r.right });
    return out;
}

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

    // if there are existing nodes in the processor, create default rows
    // creates the default rows in the daisychain on first launch
	{   // lock processor mutex for thread safety
        std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
        if (!effectNodes.empty() && rows.empty()) {
            for (auto& node : effectNodes) {
                if (!node) continue;
                Row r;
                r.left = node->effectName;
                rows.push_back(r);
            }
            rebuild(); // build the default UI chain
        }
    }
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
        if (rows[i].left == name)  return { i, false, true };
        if (rows[i].right == name) return { i, true , true };
    }
    return { -1, false, false };
}

//reset rows to match effectNodes vector
//rebuilds the rows from effectNodes as straight one per row
void DaisyChain::resetRowsToNodes() {
    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    rows.clear();
    for (auto& n : effectNodes) {
        if (!n) continue;
        Row r; r.left = n->effectName;
        rows.push_back(std::move(r));
    }
}


void DaisyChain::rebuild() {
    // clear UI rows
    for (auto* it : items) effectsContainer.removeChildComponent(it);
    items.clear(true);

	juce::Array<int> rightToClear;  // indices of rows to clear right slot if invalid
	// will not mutate rows, just read from it to build UI
	// create rows from effectnodes effect names and chain modes /////////////////////////////////////
    for (int i = 0; i < (int)rows.size(); ++i) {
        const auto& rowData = rows[i];  // get name from current order list

		auto* row = new DaisyChainItem(rowData.left, i);    // create row with effect name
        effectsContainer.addAndMakeVisible(row);
        items.add(row);

        // prevent drag when overlays are open
        row->canDrag = [this]() { return !isReorderLocked(); };
        // close overlays and unlock when the user releases on the chain
        row->onAnyInteraction = [this]() {
                if (onItemMouseUp) onItemMouseUp();   // forward to editor
            };

        // LEFT node //////////////
        auto nodeLeft = findNodeByName(rowData.left);
        const bool leftBypassed = nodeLeft ? nodeLeft->bypassed : false;
        row->updateBypassVisual(leftBypassed);

        // update global bypass state visually
        row->onBypassChanged = [this, name = rowData.left, row](int index, bool state) {
            // find node by name and update its bypass state
            if (auto n = findNodeByName(name)) { 
                n->bypassed = state; 
            }
            row->updateBypassVisual(state);
         };

        // find neighboring row states for ui connections 
        int leftModeId = 1;
        if (nodeLeft) {
            // use the nodes existing chainMode that was set by preset
            leftModeId = juce::jlimit(1, 4, (int)nodeLeft->chainMode);
        } else {
            const bool prevIsDouble = (i > 0) ? rows[i - 1].hasRight() : false;
            const bool currIsDouble = rowData.hasRight();
            const bool nextIsDouble = (i + 1 < (int)rows.size()) ? rows[i + 1].hasRight() : false;

            // find left node chain mode for ui
            if (currIsDouble)          leftModeId = 3;          // double down
            else if (prevIsDouble)     leftModeId = 4;          // unite
            else if (nextIsDouble)     leftModeId = 2;          // split
        }
		// set left node chain mode
        row->setChainModeId(leftModeId);
        row->updateModeVisual();    
       /* if (auto nodeLeft = findNodeByName(rowData.left)) {
            nodeLeft->chainMode = (ChainMode)leftModeId;
        }*/

        // Right node if present //////////////////////
        if (rowData.right.isNotEmpty()) {
			auto nodeR = findNodeByName(rowData.right); // find right node by name
            if (!nodeR) { 
				rightToClear.add(i);                                    // mark for clearing if node not found
            } else {
				// set right effect
                row->setSecondaryEffect(rowData.right);
                if (nodeR) nodeR->chainMode = ChainMode::DoubleDown;    // secondary mode is always DoubleDown in a double row

                const bool rightBypassed = nodeR->bypassed;             // get right bypass state
                row->updateSecondaryBypassVisual(nodeR->bypassed);      // update right bypass visual
                row->onSecondaryBypassChanged = [this, name = rowData.right, row](int index, bool state) {  // right bypass callback
                    // find node by name and update its bypass state
                    if (auto n = findNodeByName(name)) { 
                        n->bypassed = state; 
                    }                   
                    row->updateSecondaryBypassVisual(state);     // update visual
                };
            }

        } else {
			// clear right if no right effect
            rightToClear.add(i);
        }

        // chaining mode //////////////////////////////////
        // update chain mode callback ui using name instead of index (to avoid issues with reordering)
        row->onModeChanged = [this, row](int index, int modeId) {
			/// lambda to handle mode update by name
            // handle both left and right if they exist
            auto handleMode = [&](const juce::String& name) {
                if (auto n = findNodeByName(name)) {
                    n->chainMode = (ChainMode)juce::jlimit(1, 4, modeId);
                }
            };

            if (row->getName().isNotEmpty())     { handleMode(row->getName()); } // left
            if (!row->rightEffectName.isEmpty()) { handleMode(row->rightEffectName); } // right
			//processorRef.requestReorder(getCurrentOrder()); // notify processor of potential chain change
            processorRef.requestLayout(toProcessorRows(rows));
        };

        row->onReorder = [this](int kind, juce::String dragName, int targetRow) { // reorder callback from drag n drop ui
			if (reorderLocked) return; // prevent reordering if locked
            handleReorder(kind, dragName, targetRow);
            };

        // disable dragging when reorder is locked
        row->setInterceptsMouseClicks(true, true);
        row->onAnyInteraction = [this]() {
            if (reorderLocked)
                return; // ignore drag attempts
            };
    };

	// clear invalid right slots
	// apply after building all rows to avoid index issues
    for (int idx : rightToClear) {
        if (idx >= 0 && idx < (int)rows.size())
            rows[(size_t)idx].right.clear();
    }

    resized();
    repaint();
	if (globalBypassed) { setGlobalBypassVisual(true); } //  global bypass visual state
}

//reorders the global effects list and rebuilds UI
void DaisyChain::handleReorder(int kind, const juce::String& dragName, int targetRow) {
    if (reorderLocked || rows.empty()) return;

    // clamp target row
    targetRow = juce::jlimit(0, (int)rows.size(), targetRow);

    // locate source name
    auto [srcRow, srcIsRight, found] = findRowAndSide(rows, dragName);
    if (!found) return;

    // remove source 
    const bool sourceAboveTarget = (srcRow >= 0 && srcRow < targetRow); {
        auto& r = rows[(size_t)srcRow];
        if (!srcIsRight) 
            // removing left
            if (r.hasRight()) { r.left = r.right; r.right.clear(); 
        } else { 
                rows.erase(rows.begin() + srcRow); 
        } else {
            // removing right side of a double
            r.right.clear();
        }
    }

    // if delete a whole row and it was above the target, target ++ 1
    if (sourceAboveTarget && targetRow > 0) targetRow--;

    // check if row is empty
    if (rows.empty()) {
        // always create a new single row
        rows.push_back({ dragName, {} });
        juce::MessageManager::callAsync([this]() {
            rebuild();
            if (onReorderFinished) onReorderFinished();
            });
        return;
    }

    //  -2 create double row
    if (kind == -2)
    {
        // inserting into right slot of targetRow if possible
        // else create a brand new row below targetRow
        const int safeRow = juce::jlimit(0, (int)rows.size() - 1, targetRow);
        auto& t = rows[(size_t)safeRow];
        if (!t.hasRight() && t.left != dragName) {
            t.right = dragName; // becomes a double row
        } else {
            // target already double so add a new single row below
            rows.insert(rows.begin() + juce::jlimit(0, (int)rows.size(), safeRow + 1), { dragName, {} });
        }
    } else {
        // -1 create single row
        // vertical insert 
        targetRow = juce::jlimit(0, (int)rows.size(), targetRow);
        rows.insert(rows.begin() + targetRow, { dragName, {} });
    }

    // rebuild asynchronously 
    juce::MessageManager::callAsync([this]() {
        rebuild();
        /*if (onReorderFinished) onReorderFinished();*/
        processorRef.requestLayout(toProcessorRows(rows));
        if (onReorderFinished) onReorderFinished();
        });
}

void DaisyChain::resized() {
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

    int y = 0;
    for (int i = 0; i < items.size(); ++i) { 	// layout each item
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

    // small path icons for chain arrows
    auto drawDownArrow = [&](juce::Graphics& gr, juce::Point<float> c) {
            juce::Path p;
            p.startNewSubPath(c.x - 5, c.y - 5);
            p.lineTo(c.x, c.y + 5);
            p.lineTo(c.x + 5, c.y - 5);
            p.closeSubPath();
            gr.setColour(Colors::accentTeal); // teal
            gr.fillPath(p);
        };

    auto drawSplitArrow = [&](juce::Graphics& gr, juce::Point<float> c)  {
            juce::Path p;
            p.startNewSubPath(c.x, c.y - 5);
            p.lineTo(c.x - 5, c.y + 5);
            p.startNewSubPath(c.x, c.y - 5);
            p.lineTo(c.x + 5, c.y + 5);
            gr.setColour(Colors::accentPink); // pink
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

            gr.setColour(Colors::accentPurple); // purple
            gr.strokePath(p, juce::PathStrokeType(lineWidth));
        };

    auto drawUniteArrow = [&](juce::Graphics& gr, juce::Point<float> c) {
            juce::Path p;
            c.y -= 2.0f;
            p.startNewSubPath(c.x - 5, c.y - 5);
            p.lineTo(c.x, c.y + 5);
            p.lineTo(c.x + 5, c.y - 5);
            gr.setColour(Colors::accentBlue); // blue unite
            gr.strokePath(p, juce::PathStrokeType(2.0f));
        };

	//drawing arrows between rows 
    const int rowCount = (int)rows.size();
    for (int i = 0; i + 1 < rowCount; ++i) {
		// guarded pointers incase of invalid rows
		DaisyChainItem* cur = (i < items.size() ? items[i] : nullptr);          // current
		DaisyChainItem* next = (i + 1 < items.size() ? items[i + 1] : nullptr); // next
        if (!cur || !next) continue;

        // midpoint between bottom of current and top of next 
        juce::Point<int> curBottom = getLocalPoint( cur, juce::Point<int>(cur->getWidth() / 2, cur->getHeight()));
        juce::Point<int> nextTop = getLocalPoint( next, juce::Point<int>(next->getWidth() / 2, 0));

        float xMid = 0.5f * (curBottom.x + nextTop.x);
        float yMid = 0.5f * (curBottom.y + nextTop.y);

        xMid += 2.0f; 
        juce::Point<float> mid(xMid, yMid);

        bool thisIsDouble = (i < rowCount && rows[i].hasRight());
        bool nextIsDouble = (i + 1 < rowCount && rows[i + 1].hasRight());

        // drawing 
        if (thisIsDouble && nextIsDouble)       {drawDoubleDownArrows(g, mid); } 
        else if (thisIsDouble && !nextIsDouble) { drawUniteArrow(g, mid); } 
        else if (!thisIsDouble && nextIsDouble) { drawSplitArrow(g, mid); } 
        else                                    { drawDownArrow(g, mid);
        }
    }
}

// flatten rows to old API
std::vector<juce::String> DaisyChain::getCurrentOrder() const {
	std::vector<juce::String> flat; // flattened list
    for (auto& r : rows) {
        flat.push_back(r.left);
        if (r.hasRight()) flat.push_back(r.right);
    }
    return flat;
}


// grey out individual bypass when global bypassed
void DaisyChain::setGlobalBypassVisual(bool state) {
    globalBypassed = state;
    for (auto* row : items) {
        // LEFT
        if (globalBypassed) {
            // grey out
            row->bypass.setEnabled(false);
            row->bypass.setColour(juce::TextButton::buttonColourId, Colors::accent.withAlpha(0.5f));
            row->bypass.setColour(juce::TextButton::buttonOnColourId, Colors::accent.withAlpha(0.5f));
            row->bypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText.withAlpha(0.5f));
            row->bypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText.withAlpha(0.5f));
        } else {
            // restore
            const bool state = row->bypassed;
            const auto bg = state ? juce::Colours::hotpink : Colors::panel;

            row->bypass.setEnabled(true);
            row->bypass.setColour(juce::TextButton::buttonColourId, bg);
            row->bypass.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
            row->bypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
            row->bypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
        }
        row->bypass.repaint();

        // RIGHT (if present)
        if (row->isDoubleRow) {
            if (globalBypassed) {
                row->rightBypass.setEnabled(false);
                row->rightBypass.setColour(juce::TextButton::buttonColourId, Colors::accent.withAlpha(0.5f));
                row->rightBypass.setColour(juce::TextButton::buttonOnColourId, Colors::accent.withAlpha(0.5f));
                row->rightBypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText.withAlpha(0.5f));
                row->rightBypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText.withAlpha(0.5f));
            }
            else {
                const bool stateR = row->rightBypassed;
                const auto bgR = stateR ? juce::Colours::hotpink : Colors::panel;

                row->rightBypass.setEnabled(true);
                row->rightBypass.setColour(juce::TextButton::buttonColourId, bgR);
                row->rightBypass.setColour(juce::TextButton::buttonOnColourId, juce::Colours::lightgrey);
                row->rightBypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
                row->rightBypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
            }
            row->rightBypass.repaint();
        }
    }
}

// enable/disable chain controls (for locking during preset/settings)
void DaisyChain::setChainControlsEnabled(bool enabled) {
    addButton.setEnabled(enabled);
    duplicateButton.setEnabled(enabled);
    deleteButton.setEnabled(enabled);
    
    for (auto* item : items) {
        item->setInterceptsMouseClicks(enabled, enabled);
        item->grip.setInterceptsMouseClicks(enabled, enabled);
        item->rightGrip.setInterceptsMouseClicks(enabled, enabled);
    }
}

// lock reordering and drag/drop when viewing settings/presets
void DaisyChain::setReorderLocked(bool locked) {
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
	juce::PopupMenu menu;       // create menu manually add all effects
    menu.addItem(1, "Gain");
    menu.addItem(2, "Noise Gate");
    menu.addItem(3, "Compressor");
    menu.addItem(4, "De-Esser");
    menu.addItem(5, "De-Noiser");
    menu.addItem(6, "Formant");
    menu.addItem(7, "Pitch");
    menu.addItem(8, "Equalizer");

    menu.setLookAndFeel(&getLookAndFeel());
    addButton.setColour(juce::TextButton::buttonColourId, Colors::accent);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton), [this](int result) {
        addButton.setColour(juce::TextButton::buttonColourId, Colors::button);

            if (result == 0) return;
			// create new node based on selection
            std::shared_ptr<EffectNode> newNode;
            switch (result) {
            case 1: newNode = std::make_shared<GainNode>(processorRef); break;
            case 2: newNode = std::make_shared<NoiseGateNode>(processorRef); break;
            case 3: newNode = std::make_shared<CompressorNode>(processorRef); break;
            case 4: newNode = std::make_shared<DeEsserNode>(processorRef); break;
            case 5: newNode = std::make_shared<DeNoiserNode>(processorRef); break;
            case 6: newNode = std::make_shared<FormantNode>(processorRef); break;
            case 7: newNode = std::make_shared<PitchNode>(processorRef); break;
            case 8: newNode = std::make_shared<EqualizerNode>(processorRef); break;
            }

			// add to processor + ui lists
            if (newNode) {
                newNode->effectName = makeUniqueName(newNode->effectName, effectNodes);
				// create and attach ValueTree to APVTS
                effectNodes.push_back(newNode);
				//updated to use rows instead of effectNames directly
                Row r;
                r.left = newNode->effectName;
                rows.push_back(r);

				// rebuild the chain
                auto oldCb = onReorderFinished;
                onReorderFinished = nullptr;
                rebuild();
                onReorderFinished = oldCb;
                processorRef.requestLayout(toProcessorRows(rows));

                if (onReorderFinished) onReorderFinished();
            }
        });
}

// menu to duplicate existing effect nodes
void DaisyChain::showDuplicateMenu() {
    if (reorderLocked) return;  // prevent adding if locked
	juce::PopupMenu menu; // create menu with existing effect names
	{   // lock processor mutex for thread safety
        //std::lock_guard<std::mutex> lg(processorRef.getMutex());
        for (int i = 0; i < effectNodes.size(); ++i) {
            menu.addItem(i + 1, effectNodes[i]->effectName);
        }
    }
    menu.setLookAndFeel(&getLookAndFeel());
    duplicateButton.setColour(juce::TextButton::buttonColourId, Colors::accent);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&duplicateButton), [this](int result)
        {
            duplicateButton.setColour(juce::TextButton::buttonColourId, Colors::button);
            if (result == 0) return;
            const int index = result - 1;
            if (index < 0 || index >= effectNodes.size()) return;

            auto original = effectNodes[index];
            if (!original) return;

            // clone node object
            auto clone = original->clone();
            if (!clone) return;

            // clone its valueTree (parameters) and attach to apvts
            juce::ValueTree clonedTree(original->getNodeTypeConst() + "_" + juce::Uuid().toString());
            clonedTree.copyPropertiesAndChildrenFrom(original->getNodeStateConst(), nullptr);

			processorRef.apvts.state.addChild(clonedTree, -1, nullptr);     // add to apvts
			clone->getNodeStateRef() = clonedTree;          // set cloned tree to new node

			clone->effectName = makeUniqueName(original->effectName, effectNodes);  // make unique name
			effectNodes.push_back(clone);                   // add to processor list
            // add a new single row for it
            Row r;
            r.left = clone->effectName;
            rows.push_back(r);

			auto oldCb = onReorderFinished;     // rebuild chain
            onReorderFinished = nullptr;    
            rebuild();
            onReorderFinished = oldCb;          
            processorRef.requestLayout(toProcessorRows(rows));

            if (onReorderFinished) onReorderFinished();
        });
}
// menu to delete existing effect nodes
void DaisyChain::showDeleteMenu() {
    if (reorderLocked) return;  // prevent adding if locked
	juce::PopupMenu menu;   // create menu with existing effect names
    {   // lock processor mutex for thread safety
        //std::lock_guard<std::mutex> lg(processorRef.getMutex());
        for (int i = 0; i < effectNodes.size(); ++i) {
            menu.addItem(i + 1, effectNodes[i]->effectName);
        }
    }
    menu.setLookAndFeel(&getLookAndFeel());
    deleteButton.setColour(juce::TextButton::buttonColourId, Colors::accent);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&deleteButton), [this](int result)
        {
            deleteButton.setColour(juce::TextButton::buttonColourId, Colors::button);
            if (result == 0) return; // user canceled
            const int index = result - 1;
            if (index < 0 || index >= effectNodes.size()) return;

            auto name = effectNodes[index]->effectName;

            // remove from processor + UI lists
            effectNodes.erase(effectNodes.begin() + index);

            // remove any row containing that name (left or right)
            rows.erase(std::remove_if(rows.begin(), rows.end(),
                [&](const Row& r)
                {
                    return r.left == name || r.right == name;
                }),
                rows.end());

            // rebuild the chain
            juce::MessageManager::callAsync([this]() {
                rebuild();
                processorRef.requestLayout(toProcessorRows(rows));
                if (onReorderFinished)
                    onReorderFinished();
                });
        });
}

