// reyna

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/DaisyChainItem.h"
#include "Pitchblade/panels/EffectNode.h"

//updated to work with effect nodes instead of vector
//sidebar
class DaisyChain : public juce::Component
{
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
        juce::String left; juce::String right; bool hasRight() const {    // check if row has right effect
            return right.isNotEmpty();
        }
    };
    // rows instead of a flat name list
    // helper accessors
    const std::vector<Row>& getCurrentLayout() const { return rows; }  // new layout model
    std::vector<juce::String> getCurrentOrder() const;      // flatten rows for old API

	juce::OwnedArray<DaisyChainItem> items; // ui rows

    //setter getter for rows
    void setRows(const std::vector<Row>& newRows) {  rows = newRows; rebuild(); }
    std::vector<Row> getRows() const {  return rows; }
    void clearRows() { rows.clear(); }

	//add + copy buttons
    juce::TextButton addButton{ "Add" };
    juce::TextButton duplicateButton{ "Copy" };
    juce::TextButton deleteButton{ "Del" };

    void showAddMenu();
    void showDuplicateMenu();
    void showDeleteMenu();

	// lock reordering and drag/drop when viewing settings/presets
    void setReorderLocked(bool locked);
    bool isReorderLocked() const { return reorderLocked; }

    // called when a daisyChainItem mouseUp happens
    std::function<void()> onItemMouseUp;

    void resetRowsToNodes(); // force rows to mirror processor/effectNodes for loading presets

    int getNumItems() const { return items.size(); }

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
    std::vector<std::shared_ptr<EffectNode>>& effectNodes;  // refern to processor's chain
private:
    std::vector<Row> rows;
    juce::Viewport scrollArea;
    juce::Component effectsContainer;
    bool globalBypassed = false;

	// top bar preset/settings panel state
	bool reorderLocked = false;                     // lock reordering when viewing settings/presets
	std::function<void()> onRequestClosePanels;     // request to close settings/presets panels
	std::function<void()> onRequestUnlockChain;     // request to unlock chain for reordering

    AudioPluginAudioProcessor& processorRef;        // store processor reference
};