#include <juce_core/juce_core.h>  
#include <JuceHeader.h>
#include "BinaryData.h"
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/PluginEditor.h"

// ui - reyna
#include "Pitchblade/ui/TopBar.h"
#include "Pitchblade/ui/DaisyChain.h"
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/EffectPanel.h"
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/DaisyChainItem.h"

#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/ui/TooltipManager.h"   //tooltips 

// panels - Austin and reyna
#include "Pitchblade/panels/SettingsPanel.h"
#include "Pitchblade/panels/PresetsPanel.h"

//==============================================================================
// helper to convert DaisyChain Row to processing row procRow - reyna
struct ProcRow { juce::String left, right; };
static std::vector<ProcRow> toProcRows(const std::vector<DaisyChain::Row>& uiRows) {
    std::vector<ProcRow> out;
    out.reserve(uiRows.size());
    for (auto& r : uiRows) {
        out.push_back({ r.left, r.right });
    }
    return out;
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p): AudioProcessorEditor(&p),processorRef(p), 
                                                                    daisyChain(p, p.getEffectNodes()),
                                                                    effectPanel(p, p.getEffectNodes()), 
                                                                    visualizer(p, p.getEffectNodes()),
                                                                    settingsPanel(p), presetsPanel(p)
{   
    //Austin
    //Stuff for the settings panel. Making a listener and setting it to invisible to start
    addAndMakeVisible(settingsPanel);
    settingsPanel.setAlwaysOnTop(true);
    settingsPanel.setVisible(false);
    topBar.settingsButton.addListener(this);

	// reyna presets panel 
    addChildComponent(presetsPanel);
    presetsPanel.setAlwaysOnTop(true);
    presetsPanel.setVisible(false);
    topBar.presetButton.addListener(this);

    
    // gui frontend / ui reyna ///////////////////////////////
	setLookAndFeel(nullptr),    //reset look and feel
	setSize(800, 600);          //set editor size
	setLookAndFeel(&customLF);  //apply custom look and feel globally

    addAndMakeVisible(topBar);
    addAndMakeVisible(daisyChain);
    addAndMakeVisible(effectPanel);
    addAndMakeVisible(visualizer);

	// lock daisychain reordering when mouse is down - reyna
    daisyChain.onItemMouseUp = [this]() {
        closeOverlaysIfOpen();
        daisyChain.setReorderLocked(false);
        };
    {
        std::lock_guard<std::recursive_mutex> lock(processorRef.getMutex());
        if (!processorRef.getEffectNodes().empty()){
            effectPanel.refreshTabs();
            visualizer.refreshTabs();
        }
    }

	//tooltip manager / reyna ///////////////////////////////////////////
	// load tooltips from file or binary data (wouldnt read from file had to add to binary)
    auto tooltipFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile("assets")
        .getChildFile("tooltips.txt");

    if (tooltipFile.existsAsFile()) {
        // load normally from disk 
        tooltipManager.loadTooltipsFromFile(tooltipFile);
    } else {
		// plugin cant access paths, load from binary data
        juce::MemoryInputStream mis(BinaryData::tooltips_txt, BinaryData::tooltips_txtSize, false);
        juce::String content = mis.readEntireStreamAsString();
		// write to temp file and load
        auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("tooltips_temp.txt");
		// write content to temp file
        temp.replaceWithText(content);  
		tooltipManager.loadTooltipsFromFile(temp);  // load from temp file
    }

    tooltipManager.initialize( &customLF);     // initialize tooltpwindow using tooltipmanager
    applyRowTooltips(); //apply tooltips to every daisychain row

	// assign tooltips to top bar and global daisychain buttons
    topBar.presetButton.setTooltip(tooltipManager.getTooltipFor("presetButton"));
    topBar.bypassButton.setTooltip(tooltipManager.getTooltipFor("bypassButton"));
    topBar.lockBypassButton.setTooltip(tooltipManager.getTooltipFor("lockBypassButton"));

    topBar.settingsButton.setTooltip(tooltipManager.getTooltipFor("settingsButton"));
    daisyChain.addButton.setTooltip(tooltipManager.getTooltipFor("addButton"));
    daisyChain.duplicateButton.setTooltip(tooltipManager.getTooltipFor("duplicateButton"));
    daisyChain.deleteButton.setTooltip(tooltipManager.getTooltipFor("deleteButton"));

  
	// global bypass button / reyna  ////////////////////////////////////////////
    // Top bar bypass daisychain
    // now acts by toggling all individual effect bypasses
    topBar.bypassButton.setClickingTogglesState(false);

    // LockBypass button, restores the old global bypass behavior
    topBar.lockBypassButton.setClickingTogglesState(false);
    topBar.lockBypassButton.onClick = [this]() {

        // disable lock interaction when overlays are open
        if (isShowingSettings || isShowingPresets)
            return;

        isLockBypassActive = !isLockBypassActive;
        const bool newState = isLockBypassActive;
        processorRef.setBypassed(newState);

        // color the lock bypass button
        topBar.setButtonActive(topBar.lockBypassButton, newState);

        daisyChain.setGlobalBypassVisual(newState);   // restore old grey out behavior
        daisyChain.setChainControlsEnabled(!newState);
        daisyChain.setReorderLocked(newState);

        // if locked, no interaction allowed
        for (int i = 0; i < daisyChain.items.size(); ++i) {
            if (auto* row = daisyChain.items[i]) {
                row->setInterceptsMouseClicks(!newState, !newState);
                row->bypass.setEnabled(!newState);
                row->modeButton.setEnabled(!newState);
                row->button.setEnabled(!newState);
                row->rightButton.setEnabled(!newState);
                row->rightBypass.setEnabled(!newState);
                row->rightMode.setEnabled(!newState);

                row->setAlpha(newState ? 0.55f : 1.0f);
            }
        }

        daisyChain.repaint();
        };

    topBar.bypassButton.onClick = [this]() {
        // if not all bypassed, bypass everything
        // if already all bypassed, unbypass everything
        const bool allBypassed = areAllEffectsBypassed();
        const bool newState = !allBypassed;  

        setAllEffectsBypassed(newState);

        // update top bar button and daisychain global state
        topBar.setButtonActive(topBar.bypassButton, newState);
        };

    // keep global bypass button in sync with individual bypasses
    daisyChain.onAnyBypassChanged = [this]() {
        syncGlobalBypassButton();
        };

	//connect DaisyChain buttons to EffectPanel
    for (int i = 0; i < daisyChain.items.size(); ++i) {

        if (auto* row = daisyChain.items[i]) {
            const juce::String effectName = row->getName();

            row->button.onClick = [this, effectName]() {
                // Find node by effect name
                std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
                auto& nodes = processorRef.getEffectNodes();
                for (int n = 0; n < (int)nodes.size(); ++n) {
                    if (nodes[n] && nodes[n]->effectName == effectName) {
                        effectPanel.showEffect(n);
                        visualizer.showVisualizer(n);
                        break;
                    }
                }
                
                //Austin
                //If the settings panel is open, then close it and reopen the proper thing in the daisy chain
                if (isShowingSettings || isShowingPresets) {
                    isShowingSettings = false;
                    settingsPanel.setVisible(false);

                    visualizer.setVisible(true);
                    effectPanel.setVisible(true);

                    // reyna presets panel. close if open daisychain
                    isShowingPresets = false;
                    presetsPanel.setVisible(false);
                    visualizer.setVisible(true);
                    effectPanel.setVisible(true);
                }
                closeOverlaysIfOpen();
                setActiveEffectByName(effectName);
            };
    }
        //keeps daiychain ui reordering consistant with processor ////////////////////////////
        daisyChain.onReorderFinished = [this]() {
            // new API for multiple rows - get current UI layout and send it to the processor
            const auto& rows = daisyChain.getCurrentLayout();       // get current layout
            std::vector<AudioPluginAudioProcessor::Row> procRows;   // prepare processing rows
            procRows.reserve(rows.size());
            for (const auto& r : rows) {                            // convert to processing rows
                procRows.push_back({ r.left, r.right });
            }
            processorRef.requestLayout(procRows);                       // request layout update

            // close presets if open - reyna
            if (auto* editor = dynamic_cast<AudioPluginAudioProcessorEditor*>(getTopLevelComponent())) {
                if (editor->isPresetsVisible())
                    editor->closeOverlaysIfOpen();
            }

            visualizer.clearVisualizer();   // safely clear old node references
            // reconnect buttons after reorder
            for (int i = 0; i < daisyChain.items.size(); ++i) {
                // for single and double rows
                if (auto* row = daisyChain.items[i]) {
                    const juce::String leftName = row->getName();
                    // LEFT btn
                    row->button.onClick = [this, leftName]() {
                        closeOverlaysIfOpen();
                        std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
                        auto& nodes = processorRef.getEffectNodes();

                        for (int n = 0; n < (int)nodes.size(); ++n) {
                            if (nodes[n] && nodes[n]->effectName == leftName) {
                                effectPanel.showEffect(n);
                                visualizer.showVisualizer(n);
                                break;
                            }
                        }

                        //Austin
                        //If the settings panel is open, then close it and reopen the proper thing in the daisy chain
                        if (isShowingSettings) {
                            isShowingSettings = false;
                            settingsPanel.setVisible(false);

                            visualizer.setVisible(true);
                            effectPanel.setVisible(true);
                        }

                        // reyna presets panel. close if open daisychain
                        if (isShowingPresets) {
                            isShowingPresets = false;
                            presetsPanel.setVisible(false);
                            visualizer.setVisible(true);
                            effectPanel.setVisible(true);
                        }
                        setActiveEffectByName(leftName);
                        };

                    // RIGHT btn
                    row->rightButton.onClick = [this, name = row->rightEffectName]() {
                        if (name.isEmpty()) return;
                        std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
                        auto& nodes = processorRef.getEffectNodes();
                        // find the index by name, then open that tab
                        for (int n = 0; n < (int)nodes.size(); ++n) {
                            if (nodes[n] && nodes[n]->effectName == name) {
                                effectPanel.showEffect(n);
                                visualizer.showVisualizer(n);
                                break;
                            }
                        }
                    };
                }

            }
            applyRowTooltips();     // reapply tooltips after reorder
            if (activeEffectName.isNotEmpty())
                setActiveEffectByName(activeEffectName);

            };
            

        presetsPanel.onPresetActionFinished = [this]() {
            // keep preset page visible
            isShowingPresets = true;
            presetsPanel.setVisible(true);

            //rebuild ui after preset data is updated
            juce::MessageManager::callAsync([this]() {
                // full UI rebuild after preset operation
                rebuildAndSyncUI();
            
                // keep daisychain grayed out when presets panel is open
                if (isShowingPresets) {
                    daisyChain.setChainControlsEnabled(false);
                }
            });     
        };
    }
}

// reyna - rebuild daisy chain and effect panel ui to sync with processor
void AudioPluginAudioProcessorEditor::rebuildAndSyncUI() {
    //Pull fresh rows from processor before rebuilding UI
    auto procRows = processorRef.getCurrentLayoutRows();
    if (!procRows.empty()) {
        daisyChain.setReorderLocked(false);
        std::vector<DaisyChain::Row> uiRows;
        uiRows.reserve(procRows.size());
        for (const auto& r : procRows) {
            uiRows.push_back({ r.left, r.right });
        }
        daisyChain.setRows(uiRows);
    }

    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    juce::Logger::outputDebugString("Rebuilding DaisyChain + Panels");

    //daisyChain.resetRowsToNodes();    // rows matches current effectNodes for daisychain ui
    daisyChain.rebuild();             // rebuild rows + reset callbacks
    effectPanel.refreshTabs();        // rebind tab components
    visualizer.refreshTabs();         // sync visualizers
    resized();                        
    repaint();

    // reconnect DaisyChain buttons to EffectPanel + Visualizer
    for (int i = 0; i < daisyChain.items.size(); ++i) {
        if (auto* row = daisyChain.items[i]) {

            const juce::String effectName = row->getName(); // find rows left effect name
            row->button.onClick = [this, effectName]() {

                std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
                auto& nodes = processorRef.getEffectNodes();

                // Find the actual effect index by its name
                for (int n = 0; n < (int)nodes.size(); ++n) {
                    if (nodes[n] && nodes[n]->effectName == effectName) {
                        effectPanel.showEffect(n);
                        visualizer.showVisualizer(n);
                        setActiveEffectByName(effectName);
                        break;
                    }
                }
            };
            // handle right side if double row
            if (!row->rightEffectName.isEmpty()) {
                const juce::String rightName = row->rightEffectName;
                row->rightButton.onClick = [this, rightName]() {
                        auto& nodes = processorRef.getEffectNodes();

                        for (int n = 0; n < (int)nodes.size(); ++n) {
                            if (nodes[n] && nodes[n]->effectName == rightName) {
                                effectPanel.showEffect(n);
                                visualizer.showVisualizer(n);
                                break;
                            }
                        }
                        closeOverlaysIfOpen();  
                        setActiveEffectByName(rightName);
                    };
            }
        }
    }
    applyRowTooltips();
    if (activeEffectName.isNotEmpty())
        setActiveEffectByName(activeEffectName);
}

// set active daisychain name button as pink
void AudioPluginAudioProcessorEditor::setActiveEffectByName(const juce::String& effectName) {
    activeEffectName = effectName;
    //for double rows
    for (int i = 0; i < daisyChain.items.size(); ++i) {
        if (auto* row = daisyChain.items[i]) {
            // left
            row->button.setColour(juce::TextButton::buttonColourId, Colors::panel);
            row->button.setColour(juce::TextButton::buttonOnColourId, Colors::panel);
            row->button.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
            row->button.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
            // right
            if (!row->rightEffectName.isEmpty()) {
                row->rightButton.setColour(juce::TextButton::buttonColourId, Colors::panel);
                row->rightButton.setColour(juce::TextButton::buttonOnColourId, Colors::panel);
                row->rightButton.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
                row->rightButton.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
            }
            row->repaint(); 
        }
    }
    // set active panel to accent
    for (int i = 0; i < daisyChain.items.size(); ++i) {
        if (auto* row = daisyChain.items[i]) {
            if (row->getName() == effectName) {
                row->button.setColour(juce::TextButton::buttonColourId, Colors::accent);
                row->button.setColour(juce::TextButton::buttonOnColourId, Colors::accent);
                row->button.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
                row->button.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
                row->repaint();
                break;
            }
            if (!row->rightEffectName.isEmpty() && row->rightEffectName == effectName) {
                row->rightButton.setColour(juce::TextButton::buttonColourId, Colors::accent);
                row->rightButton.setColour(juce::TextButton::buttonOnColourId, Colors::accent);
                row->rightButton.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
                row->rightButton.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
                row->repaint();
                break;
            }
        }
    }
    daisyChain.repaint();
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
	setLookAndFeel(nullptr); //reset look and feel
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(Colors::background);

    // outer program border
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds().reduced(3), 6);   

}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    //ui reyna//////////////////////////////////////////
    auto area = getLocalBounds().reduced(6);
    //topbar height
    auto top = area.removeFromTop(40);
    topBar.setBounds(top);
    //daisychain width
    auto left = area.removeFromLeft(200);
    daisyChain.setBounds(left);

    //Austin
    //Settings panel bounds are set while the area is the entire right side, since the settings don't need a visualizer division
    settingsPanel.setBounds(area.reduced(3));
	// reyna presets panel
    presetsPanel.setBounds(area.reduced(3));

    //effects panel
    auto center = area.removeFromTop(area.getHeight() / 2);
    effectPanel.setBounds(center);
    //visualizer
    visualizer.setBounds(area);
}

//Austin
//Check to see if the settings button was clicked, and then 
void AudioPluginAudioProcessorEditor::buttonClicked(juce::Button* button){
    if(button == &topBar.settingsButton){
        //Toggle state
        isShowingSettings = !isShowingSettings;

        // reyna - updated how daisychain will lock 
		// Lock or unlock daisychain reordering based on settings visibility
        if (!isLockBypassActive) {
            daisyChain.setChainControlsEnabled(!isShowingSettings);
            daisyChain.setReorderLocked(isShowingSettings);
        }

        ////show or hide the panels based on the state of the settings
        settingsPanel.setVisible(isShowingSettings);
        effectPanel.setVisible(!isShowingSettings);
        visualizer.setVisible(!isShowingSettings);
        presetsPanel.setVisible(false);
        isShowingPresets = false;

		// enable/disable daisychain buttons based on settings visibility
        topBar.setButtonActive(topBar.settingsButton, isShowingSettings);
        topBar.setButtonActive(topBar.presetButton, false);

        if (isShowingSettings)
            topBar.setButtonActive(topBar.presetButton, false);
    }

    // reyna: preset button
    if (button == &topBar.presetButton) {
        isShowingPresets = !isShowingPresets;
        if (!isLockBypassActive) {
            daisyChain.setChainControlsEnabled(!isShowingPresets);
            daisyChain.setReorderLocked(isShowingPresets);
        }


        presetsPanel.setVisible(isShowingPresets);
        visualizer.setVisible(!isShowingPresets);
        effectPanel.setVisible(!isShowingPresets);
        settingsPanel.setVisible(false);
        isShowingSettings = false;

        topBar.setButtonActive(topBar.presetButton, isShowingPresets);
        topBar.setButtonActive(topBar.settingsButton, false);
     
        if (isShowingPresets)
            topBar.setButtonActive(topBar.settingsButton, false);
    }

    repaint();
    return;
}

// reyna - apply tooltips to every daisychain row
void AudioPluginAudioProcessorEditor::applyRowTooltips() {
    for (int i = 0; i < daisyChain.items.size(); ++i) {
        if (auto* row = daisyChain.items[i]) {
            // left side effect
            const juce::String leftKey = "effect." + row->getName();
            row->button.setTooltip(tooltipManager.getTooltipFor(leftKey));

            auto modeKey = row->modeButton.getProperties().getWithDefault("tooltipKey", juce::String());
            row->modeButton.setTooltip(tooltipManager.getTooltipFor(modeKey.toString()));
            row->bypass.setTooltip(tooltipManager.getTooltipFor("bypass"));

            // right side effect if present
            if (!row->rightEffectName.isEmpty()) {
                const juce::String rightKey = "effect." + row->rightEffectName;
                row->rightButton.setTooltip(tooltipManager.getTooltipFor(rightKey));

                auto rightModeKey = row->rightMode.getProperties().getWithDefault("tooltipKey", juce::String());
                row->rightMode.setTooltip(tooltipManager.getTooltipFor(rightModeKey.toString()));
                //row->modeButton.setTooltip(tooltipManager.getTooltipFor("modeButton"));
            }
        }
    }
}

///////////////////////////////////////////////////top bar functions
// reyna settings panel functions
void AudioPluginAudioProcessorEditor::showSettings() {
    closeOverlaysIfOpen();                          //close presets first
    daisyChain.setReorderLocked(true);              // lock daisychain
    isShowingPresets = !isShowingPresets;           // set flag

    if (isShowingSettings) {
        // closing settings
        isShowingSettings = false;
        settingsPanel.setVisible(false);
        daisyChain.setReorderLocked(false);                        // allow drag/move again
        topBar.setButtonActive(topBar.settingsButton, false);      // unpink button
        visualizer.setVisible(true);
        effectPanel.setVisible(true);
    } else {
        // opening settings
        isShowingSettings = true;
        settingsPanel.setVisible(true);
        daisyChain.setReorderLocked(true);                         // disable drag/move
        topBar.setButtonActive(topBar.settingsButton, true);       // turn button pink
        visualizer.setVisible(false);
        effectPanel.setVisible(false);
    }

    addAndMakeVisible(presetsBackdrop);
    addAndMakeVisible(settingsPanel);
    // lock reordering while Settings is up
    if (isShowingSettings)
        daisyChain.setReorderLocked(true);

    resized();
    repaint();
}

// reyna presets panel functions
void AudioPluginAudioProcessorEditor::showPresets() {
    closeOverlaysIfOpen();                    // close settings first
	daisyChain.setReorderLocked(true);  // lock daisychain
	isShowingPresets = !isShowingPresets;            // set flag

    if (isShowingPresets) {
        // closing presets
        isShowingPresets = false;
        presetsPanel.setVisible(false);
        daisyChain.setReorderLocked(false);
        topBar.setButtonActive(topBar.presetButton, false);
        visualizer.setVisible(true);
        effectPanel.setVisible(true);
    } else {
        // opening presets
        isShowingPresets = true;
        presetsPanel.setVisible(true);
        daisyChain.setReorderLocked(true);
        topBar.setButtonActive(topBar.presetButton, true);
        visualizer.setVisible(false);
        effectPanel.setVisible(false);
    }

	// hide other panels
    addAndMakeVisible(presetsBackdrop);
    addAndMakeVisible(presetsPanel);

	// lock daisychain during preset operations
    if (isShowingPresets)
        daisyChain.setReorderLocked(true);

    resized();
    repaint();
}

// reyna - close both overlays if any are open
void AudioPluginAudioProcessorEditor::closeOverlaysIfOpen() {
    bool closedSomething = false;

    if (isShowingSettings) {
        isShowingSettings = false;
        settingsPanel.setVisible(false);
        topBar.setButtonActive(topBar.settingsButton, false);
        closedSomething = true;
    }
    if (isShowingPresets) {
        isShowingPresets = false;
        presetsPanel.setVisible(false);
        topBar.setButtonActive(topBar.presetButton, false);
        closedSomething = true;
    }
	// if either was open, we close
    if (closedSomething) {
        if (!isLockBypassActive) {
            daisyChain.setChainControlsEnabled(true);
            daisyChain.setReorderLocked(false);
        }
        daisyChain.repaint();
    }

    //daisyChain.setChainControlsEnabled(true);
    //daisyChain.setReorderLocked(false);
    visualizer.setVisible(true);
    effectPanel.setVisible(true);

    // restore daisychain mode button colors
    for (auto* row : daisyChain.items) {
        if (row)
            row->updateModeVisual();   
    }
    daisyChain.repaint();
}


bool AudioPluginAudioProcessorEditor::isPresetsVisible() const {
    return isShowingPresets && presetsPanel.isVisible();
}

bool AudioPluginAudioProcessorEditor::isSettingsVisible() const { 
    return isShowingSettings && settingsPanel.isVisible();
}

//global bypass functions - reyna
bool AudioPluginAudioProcessorEditor::areAllEffectsBypassed() const {
    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    auto& nodes = processorRef.getEffectNodes();

    if (nodes.empty())
        return false;

    for (auto& node : nodes) {
        if (node && !node->bypassed)
            return false;
    }
    return true;
}

void AudioPluginAudioProcessorEditor::setAllEffectsBypassed(bool shouldBypass) {
    {
        // update processor nodes
        std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
        auto& nodes = processorRef.getEffectNodes();
        for (auto& node : nodes) {
            if (node)
                node->bypassed = shouldBypass;
        }
    }

    // update DaisyChain to match node bypass states
    std::lock_guard<std::recursive_mutex> lg(processorRef.getMutex());
    auto& nodes = processorRef.getEffectNodes();

    for (int i = 0; i < daisyChain.items.size(); ++i) {
        if (auto* row = daisyChain.items[i]) {
            // left cell
            bool leftBypassed = false;
            for (auto& node : nodes) {
                if (node && node->effectName == row->getName()) {
                    leftBypassed = node->bypassed;
                    break;
                }
            }
            row->updateBypassVisual(leftBypassed);

            // right cell if present
            if (!row->rightEffectName.isEmpty()) {
                bool rightBypassed = false;
                for (auto& node : nodes) {
                    if (node && node->effectName == row->rightEffectName) {
                        rightBypassed = node->bypassed;
                        break;
                    }
                }
                row->updateSecondaryBypassVisual(rightBypassed);
            }
        }
    }
}

void AudioPluginAudioProcessorEditor::syncGlobalBypassButton() {
    const bool allBypassed = areAllEffectsBypassed();
    topBar.setButtonActive(topBar.bypassButton, allBypassed);
}