// reyna 
/*
    The EffectNode class is the base object for every effect in the Pitchblade
    chain.

    It stores the ValueTree state for the effect, owns the effect's routing
    connections, and defines how audio flows through the chain. EffectNode
    forwards processed audio to its children based on the current mode

    Each node provides its own DSP process function, creates its own UI panel
    and visualizer, and can serialize and load its parameters through XML.

    EffectNode also tracks bypass state, manages parent and child links,
    merges audio from parents when needed, and exposes cloning functions so
    nodes can be duplicated inside the DaisyChain.
*/

#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>

// chaining modes 
enum class ChainMode {
    Down = 1,
    Split = 2,
    DoubleDown = 3, // Now strictly "Right Double"
    Unite = 4,
    LeftDouble = 5  // New: "Left Double"
};

class AudioPluginAudioProcessor;    // forward declaration

//base class for all effects in daisychain
// nodes defined in each individual effectPanel.h 
class EffectNode : public std::enable_shared_from_this<EffectNode>, public juce::Component, private juce::ValueTree::Listener {
public:

	// constructor for new node
    EffectNode(AudioPluginAudioProcessor& proc, const juce::String& type, const juce::String& displayName);

	// constructor from existing state
    EffectNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState);

    ~EffectNode() override { nodeState.removeListener(this); }   // destructor

    // processing functions > connect to outputs based on chain mode
    virtual void process(AudioPluginAudioProcessor& proc,juce::AudioBuffer<float>& buffer) = 0;         // process the incoming buffer 
    void processAndForward(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer);
    void mergeParentBuffers(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer);

    virtual std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) = 0;         // ui panel creation 

	virtual std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) {       //visualizer creation
         juce::ignoreUnused(proc);
         return nullptr; 
    }

	virtual std::shared_ptr<EffectNode> clone() const = 0;      // duplicate node

    // XML serialization
    virtual std::unique_ptr<juce::XmlElement> toXml() const = 0;
    virtual void loadFromXml(const juce::XmlElement& xml) = 0;

    ///////////////////////////// Accessors

	const juce::String& getNodeType()  const { return nodeType; }           // type of node
	const juce::ValueTree& getNodeState() const { return nodeState; }       // state of node
	juce::ValueTree& getMutableNodeState() { return nodeState; }            //  mutable state of node

	const juce::ValueTree& getNodeStateConst() const { return nodeState; }  // const state of node
	juce::ValueTree& getNodeStateRef() { return nodeState; }                // reference to state of node
	const juce::String& getNodeTypeConst() const { return nodeType; }       // const type of node

	// display name
    void setDisplayName(const juce::String& newName) {
        effectName = newName;
        nodeState.setProperty("name", effectName, nullptr);
    }

    juce::String effectName;
    bool bypassed = false; //int chainMode = 1; // 1 = down, 2 = split, 3 = double, 4 = unite
    
    ///////////////////////////// chaining mode 
    
    ChainMode chainMode = ChainMode::Down;
	std::vector<std::shared_ptr<EffectNode>> children;      // allows multiple inputs
    std::vector<std::weak_ptr<EffectNode>> parents;
    
    // allows multiple outputs
    void connectTo(std::shared_ptr<EffectNode> next);

    // resets all parent/child links
    void clearConnections() {
        children.clear();
        parents.clear();
    }

    // debug print 
    void printNodeInfo() const {
        juce::Logger::outputDebugString("Node: " + effectName + " | Mode: " + juce::String(static_cast<int>(chainMode)));
    }

    virtual void flushStateToValueTree() {}
    
/////////////////////////////
protected:
    std::vector<std::shared_ptr<EffectNode>> outputs; 

	AudioPluginAudioProcessor& processor;   // reference to main processor
	juce::String nodeType;                  // type of effect node
	juce::ValueTree nodeState;              // state of effect node

	juce::AudioBuffer<float> uniteMixBuffer;    // buffer for unite mode
	int uniteAccumulated = 0;                   // number of inputs accumulated

	// valuetree listener callback
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override {
        juce::ignoreUnused(tree, property);
    }
};
