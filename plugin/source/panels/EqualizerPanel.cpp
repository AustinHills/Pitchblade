// Huda and reyna
#include "Pitchblade/panels/EqualizerPanel.h"
#include <JuceHeader.h>
#include "Pitchblade/ui/ColorPalette.h"
#include <BinaryData.h>
#include "Pitchblade/ui/CustomLookAndFeel.h"

// ===================== EqualizerPanel =====================
EqualizerPanel::EqualizerPanel (AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
    : processor(proc), localState(state), panelTitle(nodeTitle) {
    //label names for dials - reyna
    lowFreq.setName("Low Freq");
    lowGain.setName("Low Gain");
    midFreq.setName("Mid Freq");
    midGain.setName("Mid Gain");
    highFreq.setName("High Freq");
    highGain.setName("High Gain");

    // Ranges mirror Equalizer.cpp limits
    setupKnob (lowFreq,  lowFreqLabel,  "Low Freq (Hz)",  20.0,   1000.0,  1.0,  false);
    setupKnob (lowGain,  lowGainLabel,  "Low Gain (dB)",  -24.0,     24.0, 0.1,  true);
    setupKnob (midFreq,  midFreqLabel,  "Mid Freq (Hz)",  200.0,   6000.0, 1.0,  false);
    setupKnob (midGain,  midGainLabel,  "Mid Gain (dB)",  -24.0,     24.0, 0.1,  true);
    setupKnob (highFreq, highFreqLabel, "High Freq (Hz)", 1000.0, 18000.0, 1.0,  false);
    setupKnob (highGain, highGainLabel, "High Gain (dB)", -24.0,     24.0, 0.1,  true);

    auto getProp = [&](const juce::String& key, float def) { return (float)localState.getProperty(key, def); };

    lowFreq.setValue(getProp("LowFreq", 100.0f), juce::dontSendNotification);
    lowGain.setValue(getProp("LowGain", 0.0f), juce::dontSendNotification);
    midFreq.setValue(getProp("MidFreq", 1000.0f), juce::dontSendNotification);
    midGain.setValue(getProp("MidGain", 0.0f), juce::dontSendNotification);
    highFreq.setValue(getProp("HighFreq", 5000.0f), juce::dontSendNotification);
    highGain.setValue(getProp("HighGain", 0.0f), juce::dontSendNotification);

    //panel label
    equalizerLabel.setText(panelTitle, juce::dontSendNotification);
    equalizerLabel.setName("NodeTitle");
    addAndMakeVisible(equalizerLabel);

    // make small dials for bottom row - reyna
    static SmallDialLookAndFeel smallDialLF;
    lowGain.setLookAndFeel(&smallDialLF);
    midGain.setLookAndFeel(&smallDialLF);
    highGain.setLookAndFeel(&smallDialLF);

    addAndMakeVisible (lowFreq);   
    addAndMakeVisible (lowGain);   
    addAndMakeVisible (midFreq);   
    addAndMakeVisible (midGain);  
    addAndMakeVisible (highFreq);  
    addAndMakeVisible (highGain); 

    auto updateTree = [this](juce::Slider& s, const juce::String& key) {
        s.onValueChange = [this, &s, key]() {
            // Update local state for persistence/serialization
            localState.setProperty(key, (float)s.getValue(), nullptr);

            // Also push the value directly to the audio DSP via thread-safe setters
            // avoid reading/writing the ValueTree from the audio thread.
            if (key == "LowFreq")   processor.getEqualizer().setLowFreq((float)s.getValue());
            else if (key == "LowGain")   processor.getEqualizer().setLowGainDb((float)s.getValue());
            else if (key == "MidFreq")   processor.getEqualizer().setMidFreq((float)s.getValue());
            else if (key == "MidGain")   processor.getEqualizer().setMidGainDb((float)s.getValue());
            else if (key == "HighFreq")  processor.getEqualizer().setHighFreq((float)s.getValue());
            else if (key == "HighGain")  processor.getEqualizer().setHighGainDb((float)s.getValue());
        };
    };
    updateTree(lowFreq, "LowFreq");
    updateTree(lowGain, "LowGain");
    updateTree(midFreq, "MidFreq");
    updateTree(midGain, "MidGain");
    updateTree(highFreq, "HighFreq");
    updateTree(highGain, "HighGain");

    // Push initial slider values into the DSP so the Equalizer reflects stored node state immediately.
    processor.getEqualizer().setLowFreq((float)lowFreq.getValue());
    processor.getEqualizer().setLowGainDb((float)lowGain.getValue());
    processor.getEqualizer().setMidFreq((float)midFreq.getValue());
    processor.getEqualizer().setMidGainDb((float)midGain.getValue());
    processor.getEqualizer().setHighFreq((float)highFreq.getValue());
    processor.getEqualizer().setHighGainDb((float)highGain.getValue());

    localState.addListener(this);
}

void EqualizerPanel::resized()
{
    auto area = getLocalBounds();
    equalizerLabel.setBounds(area.removeFromTop(30));

    auto r = getLocalBounds().reduced (60,3); // (side,top/bot)

    //r.removeFromTop(5);
    auto top = r.removeFromTop (r.getHeight() * 0.58f).reduced (0.8);       //top row
    auto bot = r.reduced (0.1);                                             //bottom    

    auto wTop = (top.getWidth() * 1.0f) / 3;        //top row dial width
    auto wBot = (bot.getWidth() * 1.0f) / 3;     // bottom row

    auto place = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l, bool textAbove)
    {
        //auto knob = area.removeFromTop (area.getHeight() - 5);  //knob
        auto knob = area.reduced(3, 3);
        s.setBounds (knob.reduced (1));
        l.setBounds (area);

        if (textAbove) {
            area.removeFromTop(10);
            s.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 60, 15);
        } else {
            area.removeFromBottom(10);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
        }
    };

    // Top row: Frequency knobs
    auto c1 = top.removeFromLeft (wTop);
    auto c2 = top.removeFromLeft (wTop);
    auto c3 = top;
    place (c1, lowFreq,  lowFreqLabel, true);
    place (c2, midFreq,  midFreqLabel, true);
    place (c3, highFreq, highFreqLabel, true);

    // Bottom row: Gain knobs
    auto botWidth = bot.getWidth();
    auto knobWidth = botWidth / 5;
    auto spacing = knobWidth / 8;
    auto totalWidth = (knobWidth * 3) + (spacing * 2);
    auto startX = bot.getX() + (botWidth - totalWidth) / 2;
    auto y = bot.getY();

    juce::Rectangle<int> d1(startX, y, knobWidth, bot.getHeight());
    juce::Rectangle<int> d2(d1.getRight() + spacing, y, knobWidth, bot.getHeight());
    juce::Rectangle<int> d3(d2.getRight() + spacing, y, knobWidth, bot.getHeight());

    place (d1, lowGain,  lowGainLabel, false);
    place (d2, midGain,  midGainLabel, false);
    place (d3, highGain, highGainLabel, false);
}

void EqualizerPanel::paint(juce::Graphics& g) {
    g.drawRect(getLocalBounds(), 2);

    //horizontal line
    g.setColour(Colors::panel.withAlpha(0.2f));
    int midY = getHeight() * 0.58;
    g.drawLine(10.0f, (float)midY, (float)getWidth() - 10.0f, (float)midY, 2.0f);
}

void EqualizerPanel::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text,
                                double min, double max, double step, bool isGain)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    s.setRange (min, max, step);
    s.setPopupMenuEnabled (true);
    s.setNumDecimalPlacesToDisplay (isGain ? 1 : 0);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
}

// Reyna 
//deconstructor
EqualizerPanel::~EqualizerPanel() {
    if (localState.isValid())
        localState.removeListener(this);
}

void EqualizerPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree != localState) return;
    if (property == juce::Identifier("LowFreq"))   { auto v = (float)tree.getProperty("LowFreq");  lowFreq.setValue(v, juce::dontSendNotification);  processor.getEqualizer().setLowFreq(v); }
    if (property == juce::Identifier("LowGain"))   { auto v = (float)tree.getProperty("LowGain");  lowGain.setValue(v, juce::dontSendNotification);  processor.getEqualizer().setLowGainDb(v); }
    if (property == juce::Identifier("MidFreq"))   { auto v = (float)tree.getProperty("MidFreq");  midFreq.setValue(v, juce::dontSendNotification);  processor.getEqualizer().setMidFreq(v); }
    if (property == juce::Identifier("MidGain"))   { auto v = (float)tree.getProperty("MidGain");  midGain.setValue(v, juce::dontSendNotification);  processor.getEqualizer().setMidGainDb(v); }
    if (property == juce::Identifier("HighFreq"))  { auto v = (float)tree.getProperty("HighFreq"); highFreq.setValue(v, juce::dontSendNotification); processor.getEqualizer().setHighFreq(v); }
    if (property == juce::Identifier("HighGain"))  { auto v = (float)tree.getProperty("HighGain"); highGain.setValue(v, juce::dontSendNotification); processor.getEqualizer().setHighGainDb(v); }

}
