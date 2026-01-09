// Written by Austin Hills

#include "Pitchblade/panels/BandPassCompressorPanel.h"
#include "Pitchblade/ui/CustomLookAndFeel.h"
#include "Pitchblade/ui/ColorPalette.h"
#include <algorithm>
#include <cmath>

// --- Visualizer Implementation ---

BandPassCompressorVisualizer::BandPassCompressorVisualizer(AudioPluginAudioProcessor& proc, BandPassCompressorNode& nodeRef, juce::ValueTree& state)
    : FrequencyGraphVisualizer(proc.apvts, 5, 1), // Mode 1 enabled x/y threshold lines
      node(nodeRef),
      localState(state),
      processor(proc)
{
    localState.addListener(this);
    updateThresholds();
}

BandPassCompressorVisualizer::~BandPassCompressorVisualizer()
{
    if (localState.isValid())
        localState.removeListener(this);
}

void BandPassCompressorVisualizer::updateThresholds()
{
    // Mode 1 of FrequencyGraphVisualizer uses xThreshold and yThreshold.
    // User requested: xThreshold at lower bound (Min Freq), yThreshold at Threshold Level.
    float minFreq = (float)localState.getProperty("BandPassMinFreq", 200.0f);
    float threshDb = (float)localState.getProperty("BandPassThreshold", 0.0f);
    
    // xThreshold = Min Frequency
    // yThreshold = Threshold DB
    setThreshold(minFreq, threshDb); 
}

void BandPassCompressorVisualizer::timerCallback()
{
    // Get spectrum data from DSP
    auto data = node.getDSP().getSpectrumData();
    updateSpectrumData(data);
    
    FrequencyGraphVisualizer::timerCallback();
}

void BandPassCompressorVisualizer::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == localState)
    {
        if (property == juce::Identifier("BandPassMinFreq") || 
            property == juce::Identifier("BandPassMaxFreq") ||
            property == juce::Identifier("BandPassThreshold"))
        {
            updateThresholds();
            repaint();
        }
    }
}

void BandPassCompressorVisualizer::paint(juce::Graphics& g)
{
    // Draw base visualizer (this will draw the spectrum and the first "xThreshold" line at minFreq)
    FrequencyGraphVisualizer::paint(g);

    // Now draw the custom second threshold line at BandPassMaxFreq
    float maxFreq = (float)localState.getProperty("BandPassMaxFreq", 2000.0f);

    // Calculate X position using Log10 mapping
    // Matches FrequencyGraphVisualizer::mapFreqToX logic
    const float logStart = std::log10(20.0f);
    const float logEnd = std::log10(20000.0f);
    const float logFreq = std::log10(std::max(20.0f, std::min(20000.0f, maxFreq)));
    
    float xNorm = (logFreq - logStart) / (logEnd - logStart);
    float xPos = xNorm * (float)getWidth();

    g.setColour(Colors::accent); // Use same color as other threshold lines
    // FrequencyGraphVisualizer uses dotted lines (dashes array {4.0f, 4.0f})
    
    float dashes[] = {4.0f, 4.0f};
    g.drawDashedLine(juce::Line<float>((float)xPos, 0.0f, (float)xPos, (float)getHeight()), dashes, 2, 1.0f);
}


// --- Panel Implementation ---

BandPassCompressorPanel::BandPassCompressorPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, const juce::String& nodeTitle)
    : processor(proc), localState(state), panelTitle(nodeTitle)
{
    titleLabel.setText(panelTitle, juce::dontSendNotification);
    titleLabel.setName("NodeTitle");
    addAndMakeVisible(titleLabel);

    // Setup LookAndFeel
    static SmallDialLookAndFeel smallDialLF;

    auto setupSlider = [&](juce::Slider& s, const juce::String& name, const juce::String& suffix)
    {
        s.setName(name);
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        s.setNumDecimalPlacesToDisplay(1);
        s.setTextValueSuffix(suffix);
        s.setLookAndFeel(&smallDialLF);
        addAndMakeVisible(s);
    };

    setupSlider(thresholdSlider, "Threshold", " dB");
    setupSlider(ratioSlider, "Ratio", ":1");
    setupSlider(attackSlider, "Attack", " ms");
    setupSlider(releaseSlider, "Release", " ms");
    setupSlider(minFreqSlider, "Min Freq", " Hz");
    setupSlider(maxFreqSlider, "Max Freq", " Hz");

    listenButton.setButtonText("Listen");
    listenButton.setClickingTogglesState(true);
    addAndMakeVisible(listenButton);

    // Ranges & Values
    thresholdSlider.setRange(-100.0f, 0.0f, 0.1f);
    ratioSlider.setRange(1.0f, 20.0f, 0.1f);
    attackSlider.setRange(1.0f, 200.0f, 1.0f);
    releaseSlider.setRange(10.0f, 1000.0f, 1.0f);
    minFreqSlider.setRange(20.0f, 20000.0f, 1.0f);
    minFreqSlider.setSkewFactorFromMidPoint(600.0f); // Log-ish feel
    maxFreqSlider.setRange(20.0f, 20000.0f, 1.0f);
    maxFreqSlider.setSkewFactorFromMidPoint(2000.0f);

    // Load initial values
    auto getVal = [&](const juce::String& id, float def) { return (float)localState.getProperty(id, def); };
    
    thresholdSlider.setValue(getVal("BandPassThreshold", 0.0f), juce::dontSendNotification);
    ratioSlider.setValue(getVal("BandPassRatio", 3.0f), juce::dontSendNotification);
    attackSlider.setValue(getVal("BandPassAttack", 10.0f), juce::dontSendNotification);
    releaseSlider.setValue(getVal("BandPassRelease", 100.0f), juce::dontSendNotification);
    minFreqSlider.setValue(getVal("BandPassMinFreq", 200.0f), juce::dontSendNotification);
    maxFreqSlider.setValue(getVal("BandPassMaxFreq", 2000.0f), juce::dontSendNotification);
    listenButton.setToggleState((bool)localState.getProperty("BandPassListen", false), juce::dontSendNotification);

    // Callbacks
    auto bindWrap = [this](juce::Slider& s, const juce::String& prop) {
        s.onValueChange = [this, &s, prop]() {
            localState.setProperty(prop, (float)s.getValue(), &processor.undoManager);
        };
        s.onDragStart = [this]() { processor.undoManager.beginNewTransaction(); };
    };

    bindWrap(thresholdSlider, "BandPassThreshold");
    bindWrap(ratioSlider, "BandPassRatio");
    bindWrap(attackSlider, "BandPassAttack");
    bindWrap(releaseSlider, "BandPassRelease");
    bindWrap(minFreqSlider, "BandPassMinFreq");
    bindWrap(maxFreqSlider, "BandPassMaxFreq");

    listenButton.onClick = [this]() {
        localState.setProperty("BandPassListen", listenButton.getToggleState(), nullptr);
    };

    localState.addListener(this);
}

BandPassCompressorPanel::~BandPassCompressorPanel()
{
    if (localState.isValid())
        localState.removeListener(this);
    
    thresholdSlider.setLookAndFeel(nullptr);
    ratioSlider.setLookAndFeel(nullptr);
    attackSlider.setLookAndFeel(nullptr);
    releaseSlider.setLookAndFeel(nullptr);
    minFreqSlider.setLookAndFeel(nullptr);
    maxFreqSlider.setLookAndFeel(nullptr);
}

// place method removed
// void BandPassCompressorPanel::place(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label) { ... }

void BandPassCompressorPanel::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(30));

    auto topRow = area.removeFromTop(area.getHeight() / 2);
    auto bottomRow = area;

    // Top Row: MinFreq, MaxFreq, Listen (Right)
    // Actually, let's group logically.
    // Freqs on left, Dynamics on right? Or Freqs on Top?
    // Let's do:
    // Top: Min Freq, Max Freq, Threshold
    // Bottom: Ratio, Attack, Release, Listen
    
    int w = topRow.getWidth();
    int h = topRow.getHeight();
    int thirdW = w / 3;
    int quadW = w / 4;

    // Use setBounds directly now
    
    // Helper to set bounds with reduced padding
    auto setS = [](juce::Rectangle<int> area, juce::Slider& s) { s.setBounds(area.reduced(5)); };

    setS(topRow.removeFromLeft(thirdW), minFreqSlider);
    setS(topRow.removeFromLeft(thirdW), maxFreqSlider);
    setS(topRow, thresholdSlider);

    setS(bottomRow.removeFromLeft(quadW), ratioSlider);
    setS(bottomRow.removeFromLeft(quadW), attackSlider);
    setS(bottomRow.removeFromLeft(quadW), releaseSlider);
    
    // Listen button take remaining space
    listenButton.setBounds(bottomRow.reduced(10));
}

void BandPassCompressorPanel::paint(juce::Graphics& g)
{
    g.drawRect(getLocalBounds(), 2);
}

void BandPassCompressorPanel::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property)
{
    if (tree == localState)
    {
        auto updateSlider = [&](juce::Slider& s, const juce::String& id) {
             if (property == juce::Identifier(id)) 
                 s.setValue((float)tree.getProperty(id), juce::dontSendNotification);
        };

        updateSlider(thresholdSlider, "BandPassThreshold");
        updateSlider(ratioSlider, "BandPassRatio");
        updateSlider(attackSlider, "BandPassAttack");
        updateSlider(releaseSlider, "BandPassRelease");
        updateSlider(minFreqSlider, "BandPassMinFreq");
        updateSlider(maxFreqSlider, "BandPassMaxFreq");

        if (property == juce::Identifier("BandPassListen"))
            listenButton.setToggleState((bool)tree.getProperty("BandPassListen"), juce::dontSendNotification);
    }
}


// --- Node Implementation ---

BandPassCompressorNode::BandPassCompressorNode(AudioPluginAudioProcessor& proc)
    : EffectNode(proc, "BandPassCompressorNode", "BandPass Comp"), processor(proc)
{
    auto& s = getMutableNodeState();
    if (!s.hasProperty("BandPassThreshold")) s.setProperty("BandPassThreshold", 0.0f, nullptr);
    if (!s.hasProperty("BandPassRatio")) s.setProperty("BandPassRatio", 3.0f, nullptr);
    if (!s.hasProperty("BandPassAttack")) s.setProperty("BandPassAttack", 10.0f, nullptr);
    if (!s.hasProperty("BandPassRelease")) s.setProperty("BandPassRelease", 100.0f, nullptr);
    if (!s.hasProperty("BandPassMinFreq")) s.setProperty("BandPassMinFreq", 200.0f, nullptr);
    if (!s.hasProperty("BandPassMaxFreq")) s.setProperty("BandPassMaxFreq", 2000.0f, nullptr);
    if (!s.hasProperty("BandPassListen")) s.setProperty("BandPassListen", false, nullptr);
    
    if (!processor.apvts.state.hasType("EffectNodes"))
        processor.apvts.state = juce::ValueTree("EffectNodes");

    dsp.prepare(proc.getSampleRate(), proc.getCurrentBlockSize());
}

BandPassCompressorNode::BandPassCompressorNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)
    : EffectNode(proc, existingState), processor(proc)
{
    dsp.prepare(proc.getSampleRate(), proc.getCurrentBlockSize());
}

void BandPassCompressorNode::process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer)
{
    juce::ignoreUnused(proc);
    
    // Lazy initialization if sample rate changes (or was 0)
    if (proc.getSampleRate() > 0.0 && proc.getSampleRate() != lastSampleRate)
    {
        lastSampleRate = proc.getSampleRate();
        dsp.prepare(lastSampleRate, proc.getCurrentBlockSize());
    }

    
    auto& s = getNodeState();
    dsp.setThreshold((float)s.getProperty("BandPassThreshold", 0.0f));
    dsp.setRatio((float)s.getProperty("BandPassRatio", 3.0f));
    dsp.setAttack((float)s.getProperty("BandPassAttack", 10.0f));
    dsp.setRelease((float)s.getProperty("BandPassRelease", 100.0f));
    dsp.setMinFrequency((float)s.getProperty("BandPassMinFreq", 200.0f));
    dsp.setMaxFrequency((float)s.getProperty("BandPassMaxFreq", 2000.0f));
    dsp.setListen((bool)s.getProperty("BandPassListen", false));

    dsp.process(buffer);
}

std::unique_ptr<juce::Component> BandPassCompressorNode::createPanel(AudioPluginAudioProcessor& proc)
{
    return std::make_unique<BandPassCompressorPanel>(proc, getMutableNodeState(), effectName);
}

std::unique_ptr<juce::Component> BandPassCompressorNode::createVisualizer(AudioPluginAudioProcessor& proc) 
{
    return std::make_unique<BandPassCompressorVisualizer>(proc, *this, getMutableNodeState());
}

std::shared_ptr<EffectNode> BandPassCompressorNode::clone() const 
{
    auto copiedTree = getNodeState().createCopy();
    copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr);

    auto* self = const_cast<BandPassCompressorNode*>(this);
    auto clonePtr = std::make_shared<BandPassCompressorNode>(self->processor);
    clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);

    self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
    clonePtr->setDisplayName(effectName);
    return clonePtr;
}

std::unique_ptr<juce::XmlElement> BandPassCompressorNode::toXml() const 
{
    auto xml = std::make_unique<juce::XmlElement>("BandPassCompressorNode");
    xml->setAttribute("name", effectName);
    auto& s = getNodeState();
    xml->setAttribute("BandPassThreshold", (float)s.getProperty("BandPassThreshold"));
    xml->setAttribute("BandPassRatio", (float)s.getProperty("BandPassRatio"));
    xml->setAttribute("BandPassAttack", (float)s.getProperty("BandPassAttack"));
    xml->setAttribute("BandPassRelease", (float)s.getProperty("BandPassRelease"));
    xml->setAttribute("BandPassMinFreq", (float)s.getProperty("BandPassMinFreq"));
    xml->setAttribute("BandPassMaxFreq", (float)s.getProperty("BandPassMaxFreq"));
    xml->setAttribute("BandPassListen", (int)s.getProperty("BandPassListen"));
    return xml;
}

void BandPassCompressorNode::loadFromXml(const juce::XmlElement& xml) 
{
    auto& s = getMutableNodeState();
    s.setProperty("BandPassThreshold", (float)xml.getDoubleAttribute("BandPassThreshold"), nullptr);
    s.setProperty("BandPassRatio", (float)xml.getDoubleAttribute("BandPassRatio"), nullptr);
    s.setProperty("BandPassAttack", (float)xml.getDoubleAttribute("BandPassAttack"), nullptr);
    s.setProperty("BandPassRelease", (float)xml.getDoubleAttribute("BandPassRelease"), nullptr);
    s.setProperty("BandPassMinFreq", (float)xml.getDoubleAttribute("BandPassMinFreq"), nullptr);
    s.setProperty("BandPassMaxFreq", (float)xml.getDoubleAttribute("BandPassMaxFreq"), nullptr);
    s.setProperty("BandPassListen", (int)xml.getIntAttribute("BandPassListen"), nullptr);
}
