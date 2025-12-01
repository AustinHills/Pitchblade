//reyna 
/*
    The DaisyChainItem class represents a single row inside the DaisyChain.
    Each item contains the effect button, bypass button, chain mode button,
    drag handles, and optional right side widgets for double rows. 

    also manages all drag and drop highlighting, drop target logic, and
    visual states for bypass and chain mode. 
    
    It does not perform DSP. It only reports user actions such as reordering, 
    mode changes, or bypass changes so the DaisyChain can update the processor 
    layout and rebuild the UI.
*/

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"

#include "Pitchblade/panels/EffectNode.h"

class DaisyChain;

//merges effect buttons and bypass buttons into one row item for daisychain drag n drop
class DaisyChainItem : public juce::Component, public juce::DragAndDropTarget {
public:
	// constructor
	DaisyChainItem(const juce::String& effectName, int index) :  myIndex(index) {
        setName(effectName);  // left cell name

        // drag handle (left)
        grip.setText({}, juce::dontSendNotification);
        grip.setJustificationType(juce::Justification::centred);
        grip.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        grip.addMouseListener(this, true);   // only grip can drag

		//right grip for double rows
        addAndMakeVisible(grip);
        addAndMakeVisible(rightGrip);
        rightGrip.setVisible(true);
        rightGrip.setInterceptsMouseClicks(true, true);
        rightGrip.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        rightGrip.addMouseListener(this, true);

        //main effect button
		button.setButtonText(effectName);
		addAndMakeVisible(button);

		// make buttons transparent
        button.setOpaque(false);
        rightButton.setOpaque(false);
        modeButton.setOpaque(false);
        bypass.setOpaque(false);
        rightMode.setOpaque(false);
        rightBypass.setOpaque(false);

        // turn active button pink on click
        button.onClick = [this] {         
            onEffectSelected = !onEffectSelected; // toggle selection state
            if (onEffectSelected) { button.setColour(juce::TextButton::buttonColourId, Colors::accent); }
            else { button.setColour(juce::TextButton::buttonColourId, Colors::panel); }
                button.repaint();
            };

        // button for chaining mode
        modeButton.setButtonText("M");  
        modeButton.setWantsKeyboardFocus(false);
        addAndMakeVisible(modeButton);

		//bypass button (right)
        bypass.setButtonText("B");
        bypass.setClickingTogglesState(false);
       
		// toggle bypass state and color on click
        bypass.onClick = [this] {
                bypassed = !bypassed;
                if (bypassed) { bypass.setButtonText("B"); }
                else { bypass.setButtonText("B"); }
                // notify daisychain of bypass change
				if (onBypassChanged) onBypassChanged(myIndex, bypassed); 
                // pink when bypassed
				if (bypassed) { bypass.setColour(juce::TextButton::buttonColourId, Colors::accent);   
                } else { bypass.setColour(juce::TextButton::buttonColourId, Colors::panel);
                }
            };

        addAndMakeVisible(bypass);

        // right cell
        rightButton.setVisible(false);
        rightMode.setVisible(false);
        rightBypass.setVisible(false);

        rightBypass.setButtonText("B");
        rightBypass.onClick = [this] {
                rightBypassed = !rightBypassed;
                if (onSecondaryBypassChanged) { onSecondaryBypassChanged(myIndex, rightBypassed); }
                updateSecondaryBypassVisual(rightBypassed);
            };

        // Set initial size
		setSize(200, 40);               
        setInterceptsMouseClicks(true, true);  

        //tooltip connection
        button.getProperties().set("tooltipKey", "effect." + effectName);
        bypass.getProperties().set("tooltipKey", "bypass");
        modeButton.getProperties().set("tooltipKey", "modeButton");
	} 

    //layout
	void resized() override {
        auto area = getLocalBounds().reduced(5);

        // grip : left drag handle
        auto gripArea = area.removeFromLeft(20);
        grip.setBounds(gripArea.withTrimmedTop(8).withHeight(20));
		// move area right to avoid grip
        area = area.translated(- 8, 0);
        // split remaining horizontally if double
        auto rowArea = area;
        if (hasRight) {
            auto leftCell = rowArea.removeFromLeft(rowArea.getWidth() / 2).reduced(4);
            auto rightCell = rowArea.reduced(4);

            layoutCondensedCell(leftCell, button, modeButton, bypass);
            layoutCondensedCell(rightCell, rightButton, rightMode, rightBypass);
			// grab handle stays on left
            auto gripArea = getLocalBounds().reduced(5).removeFromLeft(20);
            grip.setBounds(gripArea.withTrimmedTop(8).withHeight(20));
			// right grip handle
            auto rightGripArea = getLocalBounds().reduced(5).removeFromRight(13).withTrimmedTop(4).withHeight(20);

            rightGrip.setBounds(rightGripArea.withTrimmedTop(4).withHeight(20));
            rightGrip.setVisible(true);
            rightGrip.toFront(true);    // render ontop

        } else  { // single row
            auto leftCell = rowArea.reduced(4);
            layoutNormalCell(leftCell, button, modeButton, bypass);
        }
        rightGrip.setVisible(true);
	}

    // setter getter for chain mode
    void setChainModeId(int id) {
        chainModeId = juce::jlimit(1, 4, id);
        modeButton.setButtonText(chaingID(chainModeId));
        modeButton.setEnabled(true);

        repaint();
    }

    int getChainModeId() const {  return chainModeId;  }

	//update chain mode button visuals
    void updateModeVisual() {
        juce::Colour bg;
        juce::String label;
        juce::String tooltipKey;

        switch (chainModeId) {
            case 1:  bg = Colors::accentTeal;       label = "D";         tooltipKey = "mode.down"; break;       // down         teal
            case 2:  bg = Colors::accentPink;       label = "S";         tooltipKey = "mode.split"; break;      // split        light pink 
            case 3:  bg = Colors::accentPurple;     label = "DD";        tooltipKey = "mode.double"; break;     // doubleDown   purple
            case 4:  bg = Colors::accentBlue;       label = "U";         tooltipKey = "mode.unite"; break;      // unite        blue
            default: bg = Colors::accent;           label = "M";         tooltipKey = "mode.unknown"; break;
        }
        modeButton.setButtonText(label);
        modeButton.setColour(juce::TextButton::buttonColourId, bg);
        modeButton.setColour(juce::TextButton::buttonOnColourId, bg);
        modeButton.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
        modeButton.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
        modeButton.getProperties().set("tooltipKey", tooltipKey);

        modeButton.repaint();
    }

	//for external bypass changes, changes button color if gobal bypassed
    void updateBypassVisual(bool state) {
        bypassed = state;

        const auto bg = state ? juce::Colours::hotpink : Colors::panel;
        bypass.setColour(juce::TextButton::buttonColourId, bg);
        bypass.setColour(juce::TextButton::buttonOnColourId, bg);
        bypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
        bypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText);

        bypass.repaint();
    }

	// drag and drop /////////////////////////////////

    void mouseDown(const juce::MouseEvent& e) override {
        //  check if parent DaisyChain is locked
        if (auto* parent = getParentComponent()) {
            // climb up component tree until it finds DaisyChain
            juce::Component* c = parent;
            while (c != nullptr) {
                if (c->getName() == "DaisyChain") {
                    if (c->getProperties().contains("ReorderLocked"))
                        if ((bool)c->getProperties()["ReorderLocked"])
                            return; // skip dragging
                    break;
                }
                c = c->getParentComponent();
            }
        }
        if (e.mods.isLeftButtonDown()) {
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
                auto snapshot = createComponentSnapshot(getLocalBounds());
                container->startDragging(getName(), this, snapshot, true);
            }
        }
    }

	// on mouse up, notify of interaction
    void mouseUp(const juce::MouseEvent& e) override {
        juce::ignoreUnused(e);
		if (onAnyInteraction) onAnyInteraction();  // notify of interaction
    }

	// drag and drop target overrides 
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override {
        juce::ignoreUnused(details);
        return true;
    }

	// when item is dragged into this component
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override {
        juce::ignoreUnused(details);
    }

	// when item is dragged out of this component
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override {
        setAlpha(1.0f);
        showDropAbove = false;
        showDropRight = false;
        repaint();
    }

	// while item is being dragged over this component
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& details) override {
        auto local = details.localPosition;
		// halfway point 
        showDropAbove = (local.getY() < getHeight() / 2);
        // right/left halfway point
        bool overRightHalf = (local.getX() > getWidth() / 2);
        showDropRight = (overRightHalf && !hasRight);
        if (showDropRight) showDropAbove = false;
        repaint();
    }

	// when item is dropped on this component
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override { 
        if (onReorder) {
            const juce::String dragName = details.description.toString();

            if (showDropRight && !hasRight) {
				// kind - 2 > right side drop (add to right)
                onReorder(-2, dragName, myIndex);
            } else {
                int targetRow = myIndex;
                if (!showDropAbove) targetRow++; // bottom half inserts after
                onReorder(-1, dragName, targetRow);
            }
        }
        showDropRight = false;
        showDropAbove = false;
        repaint();
    }

    //line showing where u dragging///////////////////////////
    void paint(juce::Graphics& g) override {
		    //top line for drop above
            if (showDropAbove) {
                g.setColour(juce::Colours::pink); // pink
                g.fillRect(0, 0, getWidth(), 3);  // line at top
            }
			// right half highlight for drop right
			if (showDropRight && !hasRight) {
                g.setColour(juce::Colours::pink.withAlpha(0.35f));
                auto area = getLocalBounds().reduced(4);
                auto afterGrip = area.removeFromLeft(area.getWidth() - (area.getWidth() / 2));
                juce::ignoreUnused(afterGrip); // just to keep calc consistent
                auto rightHalf = getLocalBounds().reduced(6);
                rightHalf.removeFromLeft(rightHalf.getWidth() / 2);
                g.fillRect(rightHalf);
                g.setColour(juce::Colours::pink);
                g.drawRect(rightHalf, 2);
            }
            // draw grip dots
            g.setColour(juce::Colours::grey);

            // left dots
            auto l = grip.getBounds().toFloat();
            for (int y = 0; y < 3; ++y)
                g.fillEllipse(l.getX() + l.getWidth() / 2 - 2, l.getY() + 4 + y * 8, 4, 4);
            // right
            if (hasRight) {
                auto r = rightGrip.getBounds().toFloat();
                for (int y = 0; y < 3; ++y)
                    g.fillEllipse(r.getX() + r.getWidth() / 2 - 2, r.getY() + 4 + y * 8, 4, 4);
            }

        }

	//  second effect button for double rows /////////////////////////
    void setSecondaryEffect(const juce::String& effectName) {
        isDoubleRow = true;
        hasRight = true;
        rightEffectName = effectName;

        // right drag handle
        rightGrip.setVisible(true);

        rightButton.setButtonText(effectName);
        rightButton.setVisible(true);

        rightMode.setButtonText(chaingID(/*DoubleDown*/ 3));
        rightMode.setEnabled(false);
        rightMode.setVisible(true);

        rightBypass.setVisible(true);
        addAndMakeVisible(rightButton);
        addAndMakeVisible(rightMode);
        addAndMakeVisible(rightBypass);

        rightGrip.toFront(false);

		// color the right mode button
        updateRightModeVisual();

        resized();
        repaint();
    }

	// clear second effect
    void clearSecondaryEffect() {
        isDoubleRow = false;
        hasRight = false;
        rightEffectName.clear();

        rightButton.setVisible(false);
        rightMode.setVisible(false);
        rightBypass.setVisible(false);

        resized();
        repaint();
    }

	// update right bypass visual
    void updateSecondaryBypassVisual(bool state) {
        rightBypassed = state;
        const auto bg = state ? juce::Colours::hotpink : Colors::panel;
        rightBypass.setColour(juce::TextButton::buttonColourId, bg);
        rightBypass.setColour(juce::TextButton::buttonOnColourId, bg);
        rightBypass.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
        rightBypass.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
        rightBypass.repaint();
    }

	// update right mode visual
    void updateRightModeVisual() {
        juce::Colour bg = juce::Colour(0xffae66ed); 
        rightMode.setColour(juce::TextButton::buttonColourId, bg);
        rightMode.setColour(juce::TextButton::buttonOnColourId, bg);
        rightMode.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
        rightMode.setColour(juce::TextButton::textColourOnId, Colors::buttonText);
        rightMode.repaint();
    }

    /////////////////////////////////////////////////////////////////////////////// callbacks
	
	std::function<void(int, juce::String, int)> onReorder;  // kind (-1=above, -2=right), dragged name, target index

	bool isDoubleRow = false;       // is this a double row
	bool bypassed = false;          // left bypass state

	// getters
    int getIndex() const { return myIndex; }
    std::function<void(int, bool)> onBypassChanged;     //row index, bypass
	std::function<void(int, int)> onModeChanged;        //row index, mode id
    std::function<void(int, bool)> onSecondaryBypassChanged;

	juce::String rightEffectName;   // name of right effect if double

	// left cell widgets / single row widgets
    juce::Label      grip;
    juce::TextButton button;
    juce::TextButton modeButton;
    juce::TextButton bypass;


    // right cell widgets (only shown when double)
    juce::TextButton rightButton;
    juce::TextButton rightMode;
    juce::TextButton rightBypass;
    juce::Label rightGrip;

    // state
	int   myIndex = -1;             // row index in daisy chain
	int   chainModeId = 1;          // ids for chain modes 1-4
	bool  leftBypassed = false;     // for single or double rows
	bool  rightBypassed = false;    // for double rows
	bool  hasRight = false;         // is there a right effect slot
    bool onEffectSelected = false;  // currently active effect

    // hover state
    bool showDropAbove = false;
    bool showDropRight = false;

	// topbar buttons interaction callbacks
    std::function<void()> onRequestClosePanels;
    std::function<void()> onRequestUnlockChain;
    std::function<void()> onAnyInteraction;   // notify daisychain of interaction
    std::function<bool()> canDrag;            // check if drag allowed

	// helper to get chain mode string from id
    static juce::String chaingID(int id) {
        switch (id) {
        case 1: return "D";   // Down
        case 2: return "S";   // Split
        case 3: return "DD";  // Double
        case 4: return "U";   // Unite
        default: return "?";
        }
    }

private:
    // horizontal layout for normal rows
    static void layoutNormalCell(juce::Rectangle<int> bounds, juce::TextButton& nameBtn, juce::TextButton& modeBadgeBtn, juce::TextButton& bypassBtn)  {
        auto dragGap = 6;
        auto modeW = 23;
        auto bypassW = 23;
        auto spacing = 4;

        auto nameArea = bounds.removeFromLeft(bounds.getWidth() - (modeW + bypassW + spacing * 2));
        nameBtn.setBounds(nameArea);

        bounds.removeFromLeft(spacing);
        auto modeArea = bounds.removeFromLeft(modeW);
        modeBadgeBtn.setBounds(modeArea);

        bounds.removeFromLeft(spacing);
        auto bypassArea = bounds.removeFromLeft(bypassW);
        bypassBtn.setBounds(bypassArea);
    }

    // layout one condensed cell inside given bounds
    static void layoutCondensedCell(juce::Rectangle<int> bounds, juce::TextButton& nameBtn, juce::TextButton& modeBadgeBtn, juce::TextButton& bypassBtn) {
		auto top = bounds.removeFromTop(bounds.getHeight() / 2);    // top half
        nameBtn.setBounds(top);

        auto bottom = bounds;
        auto modeW = 36;
        auto gap = 6;
		//mode
        auto modeArea = bottom.removeFromLeft(modeW);
        modeBadgeBtn.setBounds(modeArea);
		//bypass
        bottom.removeFromLeft(gap);
        bypassBtn.setBounds(bottom);
    }
};

