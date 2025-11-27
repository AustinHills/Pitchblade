// Written by Austin Hills

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/LevelMeter.h"   //for volume meter - reyna

class CompressorNode;

// volume meter bar - reyna
class SimpleVolumeBar : public juce::Component, private juce::Timer {
public:
    SimpleVolumeBar(std::function<float()> postGetter, std::function<float()> preGetter) : getPost(std::move(postGetter)), getPre(std::move(preGetter)) {
        startTimerHz(30); // refresh about 30 FPS
    }
    void setThresholdDecibels(float newThreshold) { thresholdDb = newThreshold; }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        float cornerRadius = 6.0f;
        // background gradient 
        juce::ColourGradient bgGrad(
            Colors::panel.brighter(0.1f), bounds.getCentreX(), bounds.getY(),
            Colors::panel.darker(0.3f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds, cornerRadius);
        //for rounded corners
        juce::Path clipPath;
        clipPath.addRoundedRectangle(bounds, cornerRadius);
        g.reduceClipRegion(clipPath);

        //pre processing
        float preDb = getPre();
        float preNorm = juce::jmap(preDb, -100.0f, 0.0f, 0.0f, 1.0f);
        preNorm = juce::jlimit(0.0f, 1.0f, preNorm);

        float preHeight = bounds.getHeight() * preNorm;

        juce::Rectangle<float> preRect(
            bounds.withY(bounds.getBottom() - preHeight).withHeight(preHeight)
        );

        juce::ColourGradient preGrad(
            Colors::accentTeal.withAlpha(0.25f), preRect.getCentreX(), preRect.getBottom(),
            Colors::accentPink.withAlpha(0.25f), preRect.getCentreX(), preRect.getY(), false
        );

        g.setGradientFill(preGrad);
        g.fillRect(preRect);

        // post processing
        float postDb = getPost();
        float postNorm = juce::jmap(postDb, -100.0f, 0.0f, 0.0f, 1.0f);
        postNorm = juce::jlimit(0.0f, 1.0f, postNorm);

        float postHeight = bounds.getHeight() * postNorm;

        juce::Rectangle<float> postRect(
            bounds.withY(bounds.getBottom() - postHeight).withHeight(postHeight)
        );

        juce::ColourGradient postGrad(
            Colors::accentTeal.darker(0.3f), postRect.getCentreX(), postRect.getBottom(),
            Colors::accentPink.brighter(0.4f), postRect.getCentreX(), postRect.getY(), false
        );

        g.setGradientFill(postGrad);
        g.fillRect(postRect);

        // threshold line
        float y = juce::jmap(thresholdDb, -100.0f, 0.0f, bounds.getBottom(), bounds.getY());
        g.setColour(Colors::accent);
        g.drawLine(bounds.getX(), y - 1.0f, bounds.getRight(), y - 1.0f, 4.0f);

        // outline
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(bounds, cornerRadius, 2.0f);
    }
private:
    std::function<float()> getPost;
    std::function<float()> getPre;

    float thresholdDb = -20.0f;

    void timerCallback() override { repaint(); }
};

//Defines the UI panel /////////////////////////////////////////////
class CompressorPanel : public juce::Component, public juce::ValueTree::Listener
{
public:
    ~CompressorPanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

    //volume meter - reyna
    explicit CompressorPanel(AudioPluginAudioProcessor& proc, juce::ValueTree& state, CompressorNode* nodePtr = nullptr); //reference to active compressor node 

private:
    // Reference back to main processor
    AudioPluginAudioProcessor& processor;

    //Sliders
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider;

    //Button for mode switching
    juce::ToggleButton modeButton {"Limiter Mode"};

    //Labels for sliders
    juce::Label compressorLabel, thresholdLabel, ratioLabel, attackLabel, releaseLabel;

    //volume meter - reyna
    static void place(juce::Rectangle<int> area, juce::Slider& slider, juce::Label& label, bool useCustomLF);
    std::unique_ptr<SimpleVolumeBar> volumeMeter;
    CompressorNode* node = nullptr;  // store pointer to node instance

    juce::ValueTree localState;

    //Update visibility function
    void updateSliderVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPanel)
};

// Creating visualizer node for compressor
#include "Pitchblade/ui/VisualizerPanel.h"
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"

class CompressorNode;

//Visualizer inherits from the graph, listens to state changes, and uses the timer to poll for new data
class CompressorVisualizer : public RealTimeGraphVisualizer, public juce::ValueTree::Listener{
private:
    AudioPluginAudioProcessor& processor;
    CompressorNode& compressorNode;
    juce::ValueTree localState;
public:
    explicit CompressorVisualizer(AudioPluginAudioProcessor& proc, CompressorNode& node, juce::ValueTree& state)
        : RealTimeGraphVisualizer(proc.apvts, "dB", {-100.0f, 0.0f},false,6),
            processor(proc),
            compressorNode(node),
            localState(state)
    {
        //Set initial threshold
        float initialThreshold = (float)localState.getProperty("CompThreshold", 0.0f);
        setThreshold(initialThreshold, true);

        //Listen for changes
        localState.addListener(this);
    }

    ~CompressorVisualizer() override;

    //Update the graph
    void timerCallback() override;

    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;

};

//Austin (copying reyna's formatting)
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/effects/CompressorProcessor.h"

class CompressorNode : public EffectNode 
{
public:
    //Create node with name and reference to main processor
    explicit CompressorNode(AudioPluginAudioProcessor& proc) : EffectNode(proc, "CompressorNode", "Compressor"), processor(proc) { 
        // initialize default properties
        if (!getMutableNodeState().hasProperty("CompThreshold"))
            getMutableNodeState().setProperty("CompThreshold", 0.0f, nullptr);
        if (!getMutableNodeState().hasProperty("CompRatio"))
            getMutableNodeState().setProperty("CompRatio", 3.0f, nullptr);
        if (!getMutableNodeState().hasProperty("CompAttack"))
            getMutableNodeState().setProperty("CompAttack", 50.0f, nullptr);
        if (!getMutableNodeState().hasProperty("CompRelease"))
            getMutableNodeState().setProperty("CompRelease", 250.0f, nullptr);
        if (!getMutableNodeState().hasProperty("CompLimiterMode"))
            getMutableNodeState().setProperty("CompLimiterMode", false, nullptr);

        //ensure EffectNodes tree exists
        if (!processor.apvts.state.hasType("EffectNodes"))
            processor.apvts.state = juce::ValueTree("EffectNodes");

        //add this node to processor state tree
        processor.apvts.state.addChild(getMutableNodeState(), -1, nullptr);

        //Preparing the compressor
        compressorDSP.prepare(proc.getSampleRate());
    }

    // dsp processing step for compressor
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override {

        juce::ignoreUnused(proc);

        //Calculate output level before processing and store it
        float peakAmplitude = buffer.getMagnitude(0,0,buffer.getNumSamples());
        float levelDb = juce::Decibels::gainToDecibels(peakAmplitude,-100.0f);
        compressorDSP.priorOutputLevelDb.store(levelDb);

        const float threshold = (float)getNodeState().getProperty("CompThreshold", 0.0f);
        const float ratio = (float)getNodeState().getProperty("CompRatio", 3.0f);
        const float attack = (float)getNodeState().getProperty("CompAttack", 50.0f);
        const float release = (float)getNodeState().getProperty("CompRelease", 250.0f);
        const bool isLimiterMode = (bool)getNodeState().getProperty("CompLimiterMode", false);
        
        //Check if the limiter mode is active
        if (isLimiterMode)
        {
            // In limiter mode, use a high fixed ratio and fast attack
            compressorDSP.setThreshold(threshold);
            compressorDSP.setRatio(2000.0f); // High, fixed ratio
            compressorDSP.setAttack(1.0f);   // Very fast attack
            compressorDSP.setRelease(release);
        }
        else
        {
            // In simple compressor mode, use the values from the sliders
            compressorDSP.setThreshold(threshold);
            compressorDSP.setRatio(ratio);
            compressorDSP.setAttack(attack);
            compressorDSP.setRelease(release);
        }

        // Process the audio buffer with the updated settings
        compressorDSP.process(buffer);

        //Calculate output level after processing and store it
        peakAmplitude = buffer.getMagnitude(0,0,buffer.getNumSamples());
        levelDb = juce::Decibels::gainToDecibels(peakAmplitude,-100.0f);
        compressorDSP.currentOutputLevelDb.store(levelDb);
    }

    // return UI panel linked to node
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<CompressorPanel>(proc, getMutableNodeState(), this);
    }

    // return visualizer 
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override {
        return std::make_unique<CompressorVisualizer>(proc, *this, getMutableNodeState());
    }

    //Allows visualizer to get the shared value
    std::atomic<float>& getOutputLevelAtomic(){
        return compressorDSP.currentOutputLevelDb;
    }

    //Allows volume meter to get before value
    std::atomic<float>& getPriorLevelAtomic(){
        return compressorDSP.priorOutputLevelDb;
    }

    ////////////////////////////////////////////////////////////  reyna

    // clone node
    std::shared_ptr<EffectNode> clone() const override {
        auto copiedTree = getNodeState().createCopy();                    // Copy ValueTree state
        copiedTree.setProperty("uuid", juce::Uuid().toString(), nullptr); // new uuid for clone

        auto* self = const_cast<CompressorNode*>(this);                                        // to access processor ref
        auto clonePtr = std::make_shared<CompressorNode>(self->processor);                     // create new CompressorNode
        clonePtr->getMutableNodeState().copyPropertiesAndChildrenFrom(copiedTree, nullptr);    // copy state

        // Keep clone in processor state tree
        self->processor.apvts.state.addChild(clonePtr->getMutableNodeState(), -1, nullptr);
        clonePtr->setDisplayName(effectName); // name will be made unique in daisychain
        return clonePtr;
    }

    // XML serialization for saving/loading
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

private:
	//nodes own dsp processor + reference to main processor for param access
    AudioPluginAudioProcessor& processor;
    CompressorProcessor compressorDSP;
};