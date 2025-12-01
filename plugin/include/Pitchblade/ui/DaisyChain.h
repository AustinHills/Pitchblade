// reyna
/*
    The DaisyChain class builds and manages the entire routing sidebar for Pitchblade. 
    It displays the list of effect nodes as rows that the user can reorder, duplicate, 
    delete, or combine into double rows. 

    The class mirrors the processor's effectNodes list and converts user actions into layout
    and reorder requests for the processor. 
    
    DaisyChain also handles drag and drop logic, chain mode updates, bypass visuals, and 
    the Add, Copy, and Delete menus. 
    
    It is only a UI component that keeps the layout consistent with the processor's internal routing.
*/

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/DaisyChainItem.h"
#include "Pitchblade/panels/EffectNode.h"

//sidebar component showing the chain of effects
class DaisyChain : public juce::Component {
public:
    DaisyChain(AudioPluginAudioProcessor& proc, std::vector<std::shared_ptr<EffectNode>>& nodes);

    void resized() override;
    void paint(juce::Graphics&) override;
	void setGlobalBypassVisual(bool globalBypassed);    // grayed out when global bypassed
	void setChainControlsEnabled(bool enabled);         // enable/disable chain controls

	//refeshes ui from effectnodes
    void rebuild();
    std::function<void()> onReorderFinished;

	// multiple rows support for double down chain mode. place two effects side by side will make them double down
    struct Row {
        juce::String left; juce::String right; bool hasRight() const {  // check if row has right effect
            return right.isNotEmpty();
        }
    };

    // helper accessors
    const std::vector<Row>& getCurrentLayout() const { return rows; }   // new layout model
    std::vector<juce::String> getCurrentOrder() const;                  // flatten rows for old API

	juce::OwnedArray<DaisyChainItem> items; // ui rows

    //setter getter for rows
    void setRows(const std::vector<Row>& newRows) {  rows = newRows; rebuild(); }
    std::vector<Row> getRows() const {  return rows; }
    void clearRows() { rows.clear(); }

	//add + copy buttons
    juce::TextButton addButton{ "Add" };
    juce::TextButton duplicateButton{ "Copy" };
    juce::TextButton deleteButton{ "Del" };

	// menus for add/duplicate/delete
    void showAddMenu();
    void showDuplicateMenu();
    void showDeleteMenu();

	// for single formant/pitch effects only
    bool hasFormant() const;
    bool hasPitch() const;

	// lock reordering and drag/drop when viewing settings/presets
    void setReorderLocked(bool locked);
    bool isReorderLocked() const { return reorderLocked; }

    // called when a daisyChainItem mouseUp happens
    std::function<void()> onItemMouseUp;

    // notify editor when any bypass state changes
    std::function<void()> onAnyBypassChanged;

    void resetRowsToNodes(); // force rows to mirror processor/effectNodes for loading presets

    int getNumItems() const { return items.size(); }    // get number of items

	// get item at index
    DaisyChainItem* getItem(int index) const {
        if (index < 0 || index >= items.size())
            return nullptr;
        return items[index];
    }

//private:
	// reorder handler for multi row support
    // kind: -1 vertical insert, -2 right-slot insert (double row)
    void handleReorder(int kind, const juce::String& dragName, int targetRow);

	std::shared_ptr<EffectNode> findNodeByName(const juce::String& name) const; // helper to find node by name
    std::vector<std::shared_ptr<EffectNode>>& effectNodes;                      // refern to processor's chain

private:
	std::vector<Row> rows;              // current layout model
	juce::Viewport scrollArea;          // scroll area for daisy chain
	juce::Component effectsContainer;   // container for effect items
	bool globalBypassed = false;        // global bypass state

	// top bar preset/settings panel state
	bool reorderLocked = false;                     // lock reordering when viewing settings/presets
	std::function<void()> onRequestClosePanels;     // request to close settings/presets panels
	std::function<void()> onRequestUnlockChain;     // request to unlock chain for reordering

    AudioPluginAudioProcessor& processorRef;        // store processor reference
};