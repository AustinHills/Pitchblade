//reyna 
/* custom LookAndFeel for Pitchblade GUI to make custom buttons, sliders, tooltips, and popup menus */
#pragma once
#include <JuceHeader.h>
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "Pitchblade/ui/ColorPalette.h"

struct CustomLookAndFeel : public juce::LookAndFeel_V4
{
    CustomLookAndFeel()
    {
        // base colours
        setColour(juce::ResizableWindow::backgroundColourId, Colors::background);
        setColour(juce::TextButton::buttonColourId, Colors::button);
        setColour(juce::TextButton::buttonOnColourId, Colors::button);
        setColour(juce::TextButton::textColourOffId, Colors::buttonText);
        setColour(juce::TextButton::textColourOnId, Colors::buttonActive);

        // sliders
        setColour(juce::Slider::backgroundColourId, Colors::button);      // track background
        setColour(juce::Slider::trackColourId, Colors::accent);           // filled track pink
        setColour(juce::Slider::thumbColourId, Colors::accent);           // knob pink

        setColour(juce::Slider::textBoxTextColourId, Colors::buttonText); // text
        setColour(juce::Slider::textBoxBackgroundColourId, Colors::panel);// textbox bg

        setColour(juce::Slider::textBoxOutlineColourId, Colors::accent);  // value textbox 
        setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

        // popup menu
        //setColour(juce::PopupMenu::backgroundColourId, Colors::background);
    }

    void drawPanelBackground(juce::Graphics& g, juce::Component& comp) { 
        g.fillAll(Colors::panel);
        g.drawRect(comp.getLocalBounds(), 2);
    }
    // custom buttons //////////////////////////////////////////
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto base = backgroundColour;

        //highlighted 
        if (shouldDrawButtonAsDown) base = base.darker();
        else if (shouldDrawButtonAsHighlighted) base = base.interpolatedWith(Colors::accentPink, 0.5f);

        juce::ColourGradient grad(base, bounds.getCentreX(), bounds.getY(),
                                    base.interpolatedWith(Colors::accentPink, 0.10f), bounds.getCentreX(), bounds.getBottom(), false );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 6.0f);

        // draw accent outline
        g.setColour(Colors::accent);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    }

    //custom tool tips //////////////////////////////////////////
    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override {
        // clear background
		g.fillAll(juce::Colours::transparentBlack); 
		// background
        g.setColour(Colors::panel.withAlpha(0.75f));
        g.fillRoundedRectangle(0.0f, 0.0f, (float)width, (float)height, 6.0f);
		// outline
        g.setColour(Colors::panel);
        g.drawRoundedRectangle(0.5f, 0.5f, (float)width - 1.0f, (float)height - 1.0f, 6.0f, 1.5f);
        //text
        g.setColour(Colors::buttonText);
        g.setFont(juce::Font(14.0f));
        g.drawFittedText(text, 8, 4, width - 16, height - 8, juce::Justification::centredLeft, 2);
    }

    //custom drop down menus //////////////////////////////////////////
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override {
        const float radius = 6.0f;
        juce::Rectangle<float> area(0.0f, 0.0f, (float)width, (float)height);
        
        g.setColour(Colors::background.darker(0.5));
        g.fillRoundedRectangle(area, radius);

        // gradient background 
        juce::ColourGradient grad(
            Colors::panel.withAlpha(0.25f), area.getCentreX(), area.getY(),  
            Colors::panel.darker(0.3f), area.getCentreX(), area.getBottom(), false );

        g.setGradientFill(grad);
        g.fillRoundedRectangle(area, radius);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
        bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
        bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
        const juce::Drawable* icon, const juce::Colour* const textColourToUse) override
    {
        if (isSeparator) {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.fillRect(area.reduced(10, area.getHeight() / 2 - 1).withHeight(2));
            return;
        }

        juce::Rectangle<float> bounds(area.toFloat().reduced(4));

        // pink overlay when highlighted
        if (isHighlighted) {
            g.setColour(Colors::accentPink.withAlpha(0.25f));
            g.fillRoundedRectangle(bounds, 6.0f);
        }

        // text
        g.setColour(isActive ? Colors::buttonText : Colors::buttonText.withAlpha(0.5f));
        g.setFont(14.0f);
        g.drawFittedText(text, area.reduced(12, 0), juce::Justification::centredLeft, 1);
        
        //dividers
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.fillRect(bounds.removeFromBottom(1));
    }

    //int getPopupMenuBorderSize() override { return 0; }

    //custom text boxes ////////////////////////////////////////////////////////////////////////
    void drawLabel(juce::Graphics& g, juce::Label& label) override {
        // custom dial values textbox //////////////////
        // check if a slider text box
        if (auto* parentSlider = dynamic_cast<juce::Slider*>(label.getParentComponent())) {
            auto area = label.getLocalBounds().toFloat();
            float radius = 6.0f;

            //hover over button
            const bool hovered = label.isMouseOver(true);
            const bool isOn = parentSlider->isMouseButtonDown();
            const bool editing = label.isBeingEdited();

            auto base = Colors::panel;
            if (isOn)
                base = base.overlaidWith(Colors::accent.withAlpha(0.2f).darker(0.5f));
            /*else if (editing) {
                setColour(juce::TextEditor::backgroundColourId, Colors::accentPink);
                base = base.overlaidWith(Colors::accent.withAlpha(0.2f).darker(0.5f));
            }*/
            else if (hovered)
                base = base.overlaidWith(Colors::accentPink.withAlpha(0.25f));

            // gradient background 
            juce::ColourGradient grad(
                base.brighter(0.10f).withAlpha(0.30f), area.getCentreX(), area.getY(),
                base.darker(0.22f), area.getCentreX(), area.getBottom(),
                false
            );
            g.setGradientFill(grad);
            g.fillRoundedRectangle(area, radius);

            // draw value text
            g.setColour(label.findColour(juce::Label::textColourId));
            g.setFont(label.getFont());
            g.drawFittedText(label.getText(), label.getLocalBounds().reduced(4),
                juce::Justification::centred, 1);
            return;
        } 

        // label for panel names ///////////
        if (label.getName().containsIgnoreCase("NodeTitle") || label.getText() == label.getParentComponent()->getName()) {
            auto area = label.getLocalBounds().toFloat();

            // padding for nicer spacing
            float padX = 10.0f;
            float padY = 8.0f;
            auto padded = area.reduced(padX, padY);

            // draw title text
            g.setColour(Colors::buttonText);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            g.drawFittedText(label.getText(), padded.toNearestInt(), juce::Justification::topLeft, 1);

            // underline
            float underlineY = padded.getY() + 20.0f;
            float underlineWidth = padded.getWidth() * 0.10f;
            float underlineX = padded.getX();

            g.drawLine(underlineX, underlineY, underlineX + underlineWidth, underlineY, 4.0f);
            return;
        }

        // normal draw label
        juce::LookAndFeel_V4::drawLabel(g, label);
    }

    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override {
        auto r = juce::Rectangle<float>(0, 0, (float)width, (float)height);
        auto area = textEditor.getLocalBounds().toFloat();
        float radius = 6.0f;
        juce::ColourGradient grad( Colors::panel.brighter(0.25f), area.getCentreX(), area.getY(),
                                    Colors::panel.darker(0.35f), area.getCentreX(), area.getBottom(), false );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(area, radius);
        g.fillRoundedRectangle(r, 6.0f);
    }

    void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override { 
    }

    // custom drawComboBox //////////////////////////////////////
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
        int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<int>(0, 0, width, height);
        auto base = Colors::button; 
        
        //hover over button
        const bool hovered = box.isMouseOver();
        const bool active = box.isPopupActive();
        if (active) {
            base = Colors::accent;
        }
        else if (hovered) {
            base = base.overlaidWith(Colors::accent.withAlpha(0.25f));
        }

        // background
        juce::ColourGradient grad(
            base, bounds.getCentreX(), bounds.getY(),            
            base.darker(0.25f).overlaidWith(Colors::accent.withAlpha(0.25f)),
            bounds.getCentreX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

        // outline
        juce::Colour outline = active ? Colors::accentPink : Colors::accent;
        g.setColour(outline);
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 6.0f, 1.5f);

        // text
        g.setColour(Colors::buttonText);
        g.setFont(14.0f);
        //g.drawFittedText(box.getText(), bounds.reduced(8), juce::Justification::centredLeft, 1);

        // small triangle arrow
        juce::Path arrow;
        float arrowX = width - 15.0f;
        float arrowY = height / 2.0f;
        arrow.addTriangle(arrowX - 4, arrowY - 2, arrowX + 4, arrowY - 2, arrowX, arrowY + 3);
        g.setColour(Colors::buttonText);
        g.fillPath(arrow);
    }


    // custom toggle ////////////////////////////////////////
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        auto area = button.getLocalBounds().toFloat().reduced(2.0f);
        auto isOn = button.getToggleState();

        //hover over button
        const bool hovered = button.isMouseOver(true);
        auto base = Colors::panel;
        if (isOn) {
            base = base.overlaidWith(Colors::accentBlue.withAlpha(0.5f).darker(0.7f));
        } else if (hovered) {
            base = base.overlaidWith(Colors::accentPink.withAlpha(0.2f));
        }

        // background
        juce::ColourGradient bg( base.brighter(0.15f), area.getCentreX(), area.getY(),
                                base.darker(0.25f), area.getCentreX(), area.getBottom(), false);
        g.setGradientFill(bg);
        g.fillRoundedRectangle(area, 12.0f);

        // outline
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(area, 12.0f, 2.0f);

        // circle side switch 
        float circleDiameter = area.getHeight() * 0.7f;
        float circleX = isOn ? (area.getRight() - circleDiameter - 6.0f) : (area.getX() + 6.0f);
        juce::Rectangle<float> circle(circleX, area.getY() + (area.getHeight() - circleDiameter) / 2.0f, circleDiameter, circleDiameter);

        // shadow
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(circle.expanded(1.5f));

        // circle colors
        juce::Colour baseColor = isOn ? Colors::accentTeal : Colors::accentPink;
        juce::ColourGradient grad(baseColor.brighter(0.3f),
            circle.getCentreX(), circle.getY(), baseColor.darker(0.5f),
            circle.getCentreX(), circle.getBottom(),
            false);

        g.setGradientFill(grad);
        g.fillEllipse(circle);

        // label text
        g.setColour(Colors::buttonText);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), area.toNearestInt(), juce::Justification::centred, 1);
    }

    // custom slider //////////////////////////////////////////////////////////////////
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override {

        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(1.0f);
        const float trackHeight = juce::jmax(4.0f, bounds.getHeight() * 0.90f);

        // track
        juce::Rectangle<float> track(bounds.getX(), bounds.getCentreY() - trackHeight * 0.5f, bounds.getWidth(), trackHeight);

        // background track
        juce::ColourGradient trackGrad(
            Colors::panel.brighter(0.15f), track.getCentreX(), track.getY(),
            Colors::panel.darker(0.35f), track.getCentreX(), track.getBottom(),
            false
        );
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(track, trackHeight * 0.5f);

        // gradient track 
        const float localPos = juce::jmap((float)slider.getValue(), (float)slider.getMinimum(), (float)slider.getMaximum(), track.getX(), track.getRight());
        float filledWidth = juce::jlimit(0.0f, track.getWidth(), localPos - track.getX());

        if (filledWidth > 0.0f) {
            const float minFill = trackHeight * 0.6f;  // safe rounded size
            float filledWidth = juce::jlimit(0.0f, track.getWidth(), localPos - track.getX());

            if (filledWidth > 0.0f && filledWidth < minFill)
                filledWidth = minFill;

            juce::Rectangle<float> filled(track.getX(), track.getY(), filledWidth, trackHeight);
            juce::ColourGradient grad(
                Colors::accentPink, filled.getX(), filled.getY(),
                Colors::accentTeal, filled.getRight(), filled.getBottom(),
                false
            );
            g.setGradientFill(grad);
            g.fillRoundedRectangle(filled, trackHeight * 0.5f);
        }

        // track outline
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(track, trackHeight * 0.5f, 2.0f);

        // thumb
        const float thumbSize = trackHeight * 0.9f;

        // clamp thumb inside track
        const float edgePadding = thumbSize * 0.5f;
        float thumbX = juce::jlimit( track.getX() + edgePadding, track.getRight() - edgePadding, localPos);
        juce::Rectangle<float> thumbRect( thumbX - thumbSize * 0.5f, track.getCentreY() - thumbSize * 0.5f, thumbSize, thumbSize );

        // thumb color
        juce::ColourGradient thumbGrad(
            juce::Colours::white , thumbRect.getCentreX(), thumbRect.getY(),
            juce::Colours::white.darker(0.4f), thumbRect.getCentreX(), thumbRect.getBottom(),
            false
        );
        g.setGradientFill(thumbGrad);
        g.fillEllipse(thumbRect);

        // thumb outline
        g.setColour(juce::Colours::black);
        g.drawEllipse(thumbRect, 1.5f);
    }

    //custom dial //////////////////////////////////////////////////////////////
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider& slider) override {
        // square bounds
        const float side = (float)juce::jmin(width, height);
        juce::Rectangle<float> dial = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)
            .withSizeKeepingCentre(side, side)
            .reduced(side * 0.13f);           // outer padding

        const float radius = dial.getWidth() * 0.5f;
        const auto  centre = dial.getCentre();

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // thicknesses
        const float ringThick = juce::jmax(10.0f, radius * 0.18f);       // value ring
        const float outlineThick = juce::jmax(5.0f, radius * 0.02f);    // decorative outline
        const float innerDial = radius * 0.3f;                          //  dial
        const float baseThick = radius * 0.58f;                         // black base thickness

        // base ring
        g.setColour(juce::Colours::black);
        juce::Rectangle<float> base = dial.reduced(radius * 0.2f);
        g.setColour(juce::Colours::black);
        g.fillEllipse(base);

            // pink overlay
            juce::ColourGradient baseGrad(
                Colors::accentPink.withAlpha(0.2f),     // top 
                base.getCentreX(), base.getY(),
                Colors::accentPink.withAlpha(0.05f),    // bottom 
                base.getCentreX(), base.getBottom(),
                false
            );
            g.setGradientFill(baseGrad);
            g.fillEllipse(base);

        // thin black outline
        /*g.setColour(juce::Colours::black);
        g.drawEllipse(base.expanded(outlineThick * 1.5f), outlineThick);*/

        // inner dial
        juce::Rectangle<float> inner = dial.reduced(innerDial);
        g.setColour(Colors::panel);
        g.fillEllipse(inner);
        //gradient
            juce::ColourGradient innerDialGrad(
                Colors::panel,                          // top
                inner.getCentreX(), inner.getY(),
                juce::Colours::black.withAlpha(0.6f),   // bottom
                inner.getCentreX(), inner.getBottom(),
                false
            );
            g.setGradientFill(innerDialGrad);
            g.fillEllipse(inner);

        //inner dial dark overlay
            const float innerOutlineThickness = juce::jmax(0.5f, radius * 0.15f);
            juce::Rectangle<float> innerOutline = inner.reduced(innerOutlineThickness);
            // gradient
            juce::ColourGradient innerGrad(
                juce::Colours::black.withAlpha(0.15f),    // top
                innerOutline.getCentreX(), innerOutline.getY(),
                juce::Colours::black.withAlpha(1.0f),     // bottom 
                innerOutline.getCentreX(), innerOutline.getBottom(),
                false
            );
            g.setGradientFill(innerGrad);
            g.fillEllipse(innerOutline);

        // arrow 
        g.setColour(juce::Colours::white);
        juce::Path pointer;
        float arrowLength = radius * 0.30f;
        float arrowWidth = radius * 0.20f;
        pointer.addTriangle(0.0f, -(radius * 0.70f),-arrowWidth * 0.5f, -(radius * 0.70f - arrowLength), arrowWidth * 0.5f, -(radius * 0.70f - arrowLength) );
        // rotate 
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.fillPath(pointer);

            // inner edge outline  pink gradient
            const float innerEdgeThickness = juce::jmax(0.6f, radius * 0.02f);
            juce::Rectangle<float> innerEdge = innerOutline.reduced(innerOutlineThickness * 1.0f);

            // ring area
            juce::Path pinkBand;
            pinkBand.addEllipse(innerEdge);
            pinkBand.addEllipse(innerEdge.reduced(innerEdgeThickness * 3.0f));
            pinkBand.setUsingNonZeroWinding(false);

            // vertical gradient
            juce::ColourGradient pinkVert(
                Colors::accentPink.withAlpha(0.5f), innerEdge.getCentreX(), innerEdge.getY(),      // top
                Colors::accentPink.withAlpha(0.0f), innerEdge.getCentreX(), innerEdge.getBottom(), // bottom
                false
            );
            g.setGradientFill(pinkVert);
            g.fillPath(pinkBand);

            // pink outline
            g.setColour(Colors::accentPink.withAlpha(0.5f));
            g.drawEllipse(innerEdge, innerEdgeThickness);


        //underneath value ring
        juce::Path baseArc;
        baseArc.addArc(dial.getX(), dial.getY(), dial.getWidth(), dial.getHeight(),rotaryStartAngle, rotaryEndAngle, true);

        g.setColour(juce::Colours::black.withAlpha(0.2f));
        g.strokePath(baseArc, juce::PathStrokeType(ringThick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // value ring
        juce::ColourGradient grad(Colors::accentPink, dial.getX(), dial.getY(), Colors::accentTeal, dial.getRight(), dial.getBottom(), false);

        juce::Path valueArc;
        valueArc.addArc(dial.getX(), dial.getY(), dial.getWidth(), dial.getHeight(), rotaryStartAngle, angle, true);

        g.setGradientFill(grad);
        g.strokePath(valueArc, juce::PathStrokeType(ringThick, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // tick marks
            g.setColour(juce::Colours::white);
            const int ticks = 30;
            for (int i = 0; i <= ticks; ++i) {
                const float t = (float)i / (float)ticks;
                const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
                const bool majorTick = (i % 5 == 0); // every 5 tick is larger

                // position
                const float ringOffset = (ringThick / radius) * 0.3f; 
                const float innerScale = majorTick ? 1.15f + ringOffset : 1.18f + ringOffset;
                const float outerScale = majorTick ? 1.30f + ringOffset : 1.25f + ringOffset;
                const auto p1 = centre.getPointOnCircumference(radius * innerScale, a);
                const auto p2 = centre.getPointOnCircumference(radius * outerScale, a);
                // line thickness
                const float thickness = majorTick ? juce::jmax(2.5f, radius * 0.018f)
                    : juce::jmax(1.3f, radius * 0.012f);

                g.drawLine({ p1, p2 }, thickness);
            }

            //white value ring caps
            g.setColour(juce::Colours::white);
            const float capThickness = juce::jmax(2.5f, radius * 0.018f); 
            const float capLength = juce::jmax(10.0f, ringThick * 0.5f);  
            // start cap
            auto startP1 = centre.getPointOnCircumference(radius * 0.92f, rotaryStartAngle);
            auto startP2 = centre.getPointOnCircumference(radius * 1.3f, rotaryStartAngle);
            g.drawLine({ startP1, startP2 }, capThickness);
            // end cap
            auto endP1 = centre.getPointOnCircumference(radius * 0.92f, angle);
            auto endP2 = centre.getPointOnCircumference(radius * 1.3f, angle);
            g.drawLine({ endP1, endP2 }, capThickness);

            //put dial name below black dial
            if (slider.getName().isNotEmpty())  {
                g.setColour(juce::Colours::white);
                //text
                g.setFont(juce::Font(radius * 0.25f, juce::Font::bold));
                //below dial
                const float labelY = centre.y + baseThick * 1.8f;
                g.drawFittedText(slider.getName(), juce::Rectangle<int>( (int)(centre.x - radius), (int)labelY, (int)(radius * 2),  10),
                    juce::Justification::centred, 1);
            }
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override  {
        auto layout = juce::LookAndFeel_V4::getSliderLayout(slider);

        // push textbox down
        //const int pushDown = juce::roundToInt(slider.getHeight() * 0.01f); 
        if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
            layout.textBoxBounds = layout.textBoxBounds.translated(0, -slider.getHeight() * 0.02f);

        //layout.sliderBounds.setBottom(layout.sliderBounds.getBottom() - pushDown);

        return layout;
    }

//custom small dial
    inline void drawSmallDial(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider& slider)
    {
        // square bounds
        const float side = (float)juce::jmin(width, height);
        const float reducedSide = side * 0.9f;
        juce::Rectangle<float> dial(x + (width - reducedSide) * 0.5f, y, reducedSide, reducedSide);

        const float radius = dial.getWidth() * 0.5f;
        const auto  centre = dial.getCentre();

        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // thicknesses
        const float ringThick = juce::jmax(10.0f, radius * 0.18f);       // value ring
        const float outlineThick = juce::jmax(5.0f, radius * 0.02f);    // decorative outline
        const float innerDial = radius * 0.3f;                          //  dial
        const float baseThick = radius * 0.58f;                         // black base thickness

        // base ring
        g.setColour(juce::Colours::black);
        juce::Rectangle<float> base = dial.reduced(radius * 0.2f);
        g.setColour(juce::Colours::black);
        g.fillEllipse(base);

        // pink overlay
        juce::ColourGradient baseGrad(
            Colors::accentPink.withAlpha(0.2f),     // top 
            base.getCentreX(), base.getY(),
            Colors::accentPink.withAlpha(0.05f),    // bottom 
            base.getCentreX(), base.getBottom(),
            false
        );
        g.setGradientFill(baseGrad);
        g.fillEllipse(base);

        // inner dial
        juce::Rectangle<float> inner = dial.reduced(innerDial);
        g.setColour(Colors::panel);
        g.fillEllipse(inner);
        //gradient
        juce::ColourGradient innerDialGrad(
            Colors::panel,                          // top
            inner.getCentreX(), inner.getY(),
            juce::Colours::black.withAlpha(0.6f),   // bottom
            inner.getCentreX(), inner.getBottom(),
            false
        );
        g.setGradientFill(innerDialGrad);
        g.fillEllipse(inner);

        //inner dial dark overlay
        const float innerOutlineThickness = juce::jmax(0.5f, radius * 0.15f);
        juce::Rectangle<float> innerOutline = inner.reduced(innerOutlineThickness);
        // gradient
        juce::ColourGradient innerGrad(
            juce::Colours::black.withAlpha(0.15f),    // top
            innerOutline.getCentreX(), innerOutline.getY(),
            juce::Colours::black.withAlpha(1.0f),     // bottom 
            innerOutline.getCentreX(), innerOutline.getBottom(),
            false
        );
        g.setGradientFill(innerGrad);
        g.fillEllipse(innerOutline);

        // arrow 
        g.setColour(juce::Colours::white);
        juce::Path pointer;
        float arrowLength = radius * 0.30f;
        float arrowWidth = radius * 0.20f;
        pointer.addTriangle(0.0f, -(radius * 0.70f), -arrowWidth * 0.5f, -(radius * 0.70f - arrowLength), arrowWidth * 0.5f, -(radius * 0.70f - arrowLength));
        // rotate 
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
        g.fillPath(pointer);

        // inner edge outline  pink gradient
        const float innerEdgeThickness = juce::jmax(0.6f, radius * 0.02f);
        juce::Rectangle<float> innerEdge = innerOutline.reduced(innerOutlineThickness * 1.0f);

        // ring area
        juce::Path pinkBand;
        pinkBand.addEllipse(innerEdge);
        pinkBand.addEllipse(innerEdge.reduced(innerEdgeThickness * 3.0f));
        pinkBand.setUsingNonZeroWinding(false);

        // vertical gradient
        juce::ColourGradient pinkVert(
            Colors::accentPink.withAlpha(0.5f), innerEdge.getCentreX(), innerEdge.getY(),      // top
            Colors::accentPink.withAlpha(0.0f), innerEdge.getCentreX(), innerEdge.getBottom(), // bottom
            false
        );
        g.setGradientFill(pinkVert);
        g.fillPath(pinkBand);

        // pink outline
        g.setColour(Colors::accentPink.withAlpha(0.5f));
        g.drawEllipse(innerEdge, innerEdgeThickness);

        //put dial name below black dial
        if (slider.getName().isNotEmpty()) {
            g.setColour(juce::Colours::white);
            //text
            g.setFont(juce::Font(radius * 0.25f, juce::Font::bold));
            //below dial
            const float labelY = centre.y + baseThick * 1.6f;
            g.drawFittedText(slider.getName(), juce::Rectangle<int>((int)(centre.x - radius), (int)labelY, (int)(radius * 2), 10),
                juce::Justification::centred, 1);
        }
    }

};

// LookAndFeel to draw smalldial
struct SmallDialLookAndFeel : public CustomLookAndFeel
{
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider& slider) override
    {
        drawSmallDial(g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider);
    }

};

