#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/PluginEditor.h"
//austin
#include "Pitchblade/panels/GainPanel.h"
#include "Pitchblade/panels/NoiseGatePanel.h"
#include "Pitchblade/panels/CompressorPanel.h"
#include "Pitchblade/panels/DeEsserPanel.h"
#include "Pitchblade/panels/DeNoiserPanel.h"
#include "Pitchblade/panels/EffectNode.h"
//huda
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/panels/EqualizerPanel.h"
//hayley
#include "Pitchblade/panels/PitchPanel.h"

//==============================================================================
// Constructor: sets up the plugin's audio input/output, creates all parameter definitions,
// and initializes the ValueTree used to store the effect chain state for saving/loading 
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), 
                       // Create the internal pitch correction processor
                       pitchProcessor(pitchDetector, pitchShifter), 

    // Create the AudioProcessorValueTreeState that stores all parameters.
    // It owns every parameter defined in createParameterLayout and handles
    // automation and preset saving
    apvts(*this, nullptr, "Parameters", createParameterLayout()) {
	    // check if effectNodes tree exists
        // branch stores the layout of the DaisyChain - effect ordering,
        // unique IDs, and all ValueTrees belonging to each EffectNode
        if (!apvts.state.hasType("EffectNodes")) {
            apvts.state = juce::ValueTree("EffectNodes");   
        }
    }

// Destructor: ensures processor is suspended when the its deleted
AudioPluginAudioProcessor::~AudioPluginAudioProcessor(){ suspendProcessing(true); }

//============================================================================== reyna
// global APVTS parameter layout
// no longer shared controls - works as a template
// defines all default perameters, all effects creates local copies into their valuetree
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gain : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "GAIN", "Gain", juce::NormalisableRange<float>(-48.0f, 48.0f, 0.1f), 0.0f));

    // Noise gate : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "GATE_THRESHOLD", "Gate Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "GATE_ATTACK", "Gate Attack", juce::NormalisableRange<float>(1.0f, 200.0f, 1.0f), 25.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "GATE_RELEASE", "Gate Release", juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f), 100.0f));

    // Compressor : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "COMP_THRESHOLD", "Compressor Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "COMP_RATIO", "Compressor Ratio", juce::NormalisableRange<float>(1.0f, 10.0f, 0.1f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "COMP_ATTACK", "Compressor Attack", juce::NormalisableRange<float>(1.0f, 300.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "COMP_RELEASE", "Compressor Release", juce::NormalisableRange<float>(1.0f, 300.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "COMP_LIMITER_MODE", "Compressor Limiter Mode", "False"));

	// De-Esser : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_THRESHOLD", "DeEsser Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_RATIO", "DeEsser Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_ATTACK", "DeEsser Attack", juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_RELEASE", "DeEsser Release", juce::NormalisableRange<float>(1.0f, 300.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_FREQUENCY", "DeEsser Frequency", juce::NormalisableRange<float>(2000.0f, 12000.0f, 10.0f), 6000.0f));

    //De-Noiser : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DENOISER_REDUCTION", "DeNoiser Reduction", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "DENOISER_LEARN", "DeNoiser Learn", false));

	// Formant Shifter : huda
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_FORMANT_SHIFT, "Formant",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.01f, 1.0f), 0.0f));

    //  Equalizer : huda
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_LOW_FREQ", "EQ Low Freq", juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f), 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_LOW_GAIN", "EQ Low Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_MID_FREQ", "EQ Mid Freq", juce::NormalisableRange<float>(200.0f, 6000.0f, 1.0f), 1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_MID_GAIN", "EQ Mid Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_HIGH_FREQ", "EQ High Freq", juce::NormalisableRange<float>(1000.0f, 18000.0f, 1.0f), 4000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "EQ_HIGH_GAIN", "EQ High Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_FORMANT_MIX, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f, 1.0f), 1.0f)); // full wet by default for obviousness

    //Pitch Shifter: hayley
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_RETUNE_SPEED", "Pitch Retune Speed", juce::NormalisableRange<float>(0.0f, 1.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_CORRECTION_RATIO", "Pitch Correction Ratio", juce::NormalisableRange<float>(0.0f, 1.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_WAVER", "Pitch Waver", juce::NormalisableRange<float>(0.0f, 20.0f, 1.0f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_TRANSITION", "Pitch Note Transition", juce::NormalisableRange<float>(0.0f, 50.0f, 1.0f), 20.0f));

    //Settings Panel: austin
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "GLOBAL_FRAMERATE", "Global Framerate", 1, 4, 3));

    return { params.begin(), params.end() };
}

//============================================================================== layout request from UI thread  - reyna
// stores new rows and applies on audio thread
void AudioPluginAudioProcessor::requestLayout(const std::vector<Row>& newRows) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    pendingRows = newRows;
    layoutRequested.store(true);
}

// helper to find node by name in list
static std::shared_ptr<EffectNode> findByName(const std::vector<std::shared_ptr<EffectNode>>& list, const juce::String& name) {
    for (auto& n : list) if (n && n->effectName == name) return n;
    return {};
}

// get current pending layout rows
std::vector<AudioPluginAudioProcessor::Row> AudioPluginAudioProcessor::getCurrentLayoutRows() {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    return pendingRows;
}

// apply pending layout on audio thread
// reconnect effect nodes based on pending rows
void AudioPluginAudioProcessor::applyPendingLayout() {
	std::scoped_lock lock(audioMutex);   // lock mutex for thread safety
    if (!layoutRequested.exchange(false))
        return;

	std::vector<std::shared_ptr<EffectNode>> old = effectNodes; // copy of current list

    // reset connections and mode
    for (auto& n : old) if (n) {
		n->clearConnections(); n->chainMode = ChainMode::Down;  
    }

    // helper to get nodes by row
	auto getRowNodes = [&](const Row& r) -> std::pair<std::shared_ptr<EffectNode>, std::shared_ptr<EffectNode>> {   
        return { findByName(old, r.left), r.right.isNotEmpty() ? findByName(old, r.right) : nullptr };
        };

    // previous left/right nodes
	std::shared_ptr<EffectNode> prevL = nullptr, prevR = nullptr;   
    bool prevWasDouble = false;

	for (int i = 0; i < (int)pendingRows.size(); ++i) { // iterate rows
        const auto& r = pendingRows[i];
        auto [L, R] = getRowNodes(r);
        const bool currDouble = (bool)R;
        const bool nextDouble = (i + 1 < (int)pendingRows.size()) && pendingRows[i + 1].right.isNotEmpty();

		// connect based on current/previous row types //////
		if (currDouble) { // if double row
            if (L) L->chainMode = ChainMode::DoubleDown;
            if (R) R->chainMode = ChainMode::DoubleDown;
            // connect continuation from previous double
            if (prevWasDouble) {
                if (prevL && L) prevL->connectTo(L);
                if (prevR && R) prevR->connectTo(R);
            }
            // if previous was single, it must have been a Split
            else if (prevL) {
                prevL->chainMode = ChainMode::Split;
                if (L) prevL->connectTo(L);
                if (R) prevL->connectTo(R);
            }
            prevL = L; prevR = R; prevWasDouble = true;
        }
        else { // single row
            if (prevWasDouble) {
                // this row merges two lanes
                if (L) L->chainMode = ChainMode::Unite;
                if (prevL && L) prevL->connectTo(L);
                if (prevR && L) prevR->connectTo(L);
            } else {
                // serial Down
                if (prevL && L) prevL->connectTo(L);
                if (L) L->chainMode = nextDouble ? ChainMode::Split : ChainMode::Down;
            }
			prevL = L; prevR = nullptr; prevWasDouble = false;  // reset right
        }
    }

	// rebuild new effect node list
    std::vector<std::shared_ptr<EffectNode>> newList;
    newList.reserve(pendingRows.size() * 2);
    for (auto& r : pendingRows) { 	                                            // add nodes in order of rows
		if (auto n = findByName(old, r.left)) { newList.push_back(n); }	        // add left node
        if (!r.right.isEmpty()) {
            if (auto m = findByName(old, r.right)) { newList.push_back(m); }    // add right node if exists
        }
    }

	// update effect node list and root
    if (!newList.empty()) {
        effectNodes = std::move(newList);
        activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
        rootNode = effectNodes.front();
    }

    // rebuild UI safely 
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
    }

}

//============================================================================== preset save/load - reyna
// saving presets to file
void AudioPluginAudioProcessor::savePresetToFile(const juce::File& file) {
	std::lock_guard<std::recursive_mutex> lock(audioMutex);    // lock mutex for thread safety

	// create XML root
    juce::XmlElement presetRoot("PitchbladePreset");
    presetRoot.setAttribute("version", 1.0);

    applyPendingLayout();

    // save each active node explicitly
    juce::XmlElement* nodes = new juce::XmlElement("EffectNodes");
    for (auto& node : effectNodes) {
        if (!node) continue;

		auto nodeXml = node->toXml(); //effectnode subclass toXml
        if (nodeXml != nullptr) {
            // save bypass state
            nodeXml->setAttribute("bypass", node->bypassed);
            // chaining mode (1-4)
            nodeXml->setAttribute("chainMode", (int)node->chainMode);

            nodes->addChildElement(nodeXml.release());
        }
    }

	// add nodes to root
    presetRoot.addChildElement(nodes);

    //store layout rows
    juce::XmlElement* layout = new juce::XmlElement("ChainLayout");
    auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor());
    if (ed != nullptr) {
        for (auto& r : ed->getDaisyChain().getCurrentLayout()) {
            juce::XmlElement* row = new juce::XmlElement("Row");
            row->setAttribute("left", r.left);
            if (r.right.isNotEmpty())
                row->setAttribute("right", r.right);
            layout->addChildElement(row);
        }
    }

    // also store global params
    presetRoot.addChildElement(layout);
    juce::XmlElement* globals = new juce::XmlElement("GlobalParameters");
    globals->setAttribute("GLOBAL_FRAMERATE",
        (int)*apvts.getRawParameterValue("GLOBAL_FRAMERATE"));
    presetRoot.addChildElement(globals);

    file.getParentDirectory().createDirectory();
    presetRoot.writeTo(file);
    juce::Logger::outputDebugString("Saved preset to: " + file.getFullPathName());
}

// loading presets from file
void AudioPluginAudioProcessor::loadPresetFromFile(const juce::File& file) {
	std::lock_guard<std::recursive_mutex> lock(audioMutex);                 // lock mutex for thread safety
	std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));  // parse XML from file
    if (!xml) return;
	// load global params
    auto* nodes = xml->getChildByName("EffectNodes");
    if (!nodes) return;

    // clear existing nodes before rebuilding
    effectNodes.clear();

	// load each node
    forEachXmlChildElement(*nodes, nodeXml) {
        auto name = nodeXml->getTagName();
        std::shared_ptr<EffectNode> node;

        if      (name == "GainNode")        node = std::make_shared<GainNode>(*this);
        else if (name == "NoiseGateNode")   node = std::make_shared<NoiseGateNode>(*this);
        else if (name == "CompressorNode")  node = std::make_shared<CompressorNode>(*this);
        else if (name == "DeEsserNode")     node = std::make_shared<DeEsserNode>(*this);
        else if (name == "DeNoiserNode")    node = std::make_shared<DeNoiserNode>(*this);
        else if (name == "EqualizerNode")   node = std::make_shared<EqualizerNode>(*this);  
        else if (name == "PitchNode")       node = std::make_shared<PitchNode>(*this);
        else if (name == "FormantNode")     node = std::make_shared<FormantNode>(*this);
        else continue;

        //load node list
        node->loadFromXml(*nodeXml);
        //load bypass state
        if (nodeXml->hasAttribute("bypass"))
            node->bypassed = nodeXml->getBoolAttribute("bypass");
        // restore chaining mode
        if (nodeXml->hasAttribute("chainMode"))
            node->chainMode = (ChainMode)nodeXml->getIntAttribute("chainMode");

        // node API from EffectNode
        auto& vt = node->getMutableNodeState(); 
        vt.setProperty("uuid", juce::Uuid().toString(), nullptr);

        // give node back its unique name
        if(nodeXml->hasAttribute("name"))
            node->effectName = nodeXml->getStringAttribute("name");

        effectNodes.push_back(node);
    }
    // read chaining layout
    auto* layout = xml->getChildByName("ChainLayout");
    if (layout != nullptr) {
        std::vector<Row> loadedRows;
		// read each row
        forEachXmlChildElement(*layout, rowXml) {
            AudioPluginAudioProcessor::Row r;
            r.left = rowXml->getStringAttribute("left");
            r.right = rowXml->getStringAttribute("right");
            loadedRows.push_back(r);
        }

        // tell the audio thread to rebuild connections from these rows
        requestLayout(loadedRows);
        // keep these rows as the current layout for the UI
        pendingRows = loadedRows;
        activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
        rootNode = effectNodes.empty() ? nullptr : effectNodes.front();
    } else {
        // no chainLayout in the preset: fall back to simple linear routing
        for (auto& node : effectNodes)
            if (node) node->clearConnections();

		// simple linear connect
        for (int i = 0; i + 1 < (int)effectNodes.size(); ++i)
            effectNodes[i]->connectTo(effectNodes[i + 1]);

		// update active nodes and root
        activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
        rootNode = effectNodes.empty() ? nullptr : effectNodes.front();

        // give that simple layout to the UI
        pendingRows.clear();
        for (auto& node : effectNodes) {
            if (!node) continue;
            Row r;
            r.left = node->effectName;
            r.right = {};
            pendingRows.push_back(r);
        }
        // layout already applied by the direct connectTo calls above
        layoutRequested.store(false);
    }

    // update ui 
    juce::MessageManager::callAsync([this]() {
        if (auto* editor = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
            editor->rebuildAndSyncUI();
        });
    juce::Logger::outputDebugString("loaded preset from: " + file.getFullPathName());
}

//============================================================================== 
// The following methods implement the basic behavior of the plugin processor.

const juce::String AudioPluginAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const {
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms() {
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() {
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index) {
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index) {
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName) {
    juce::ignoreUnused (index, newName);
}

//==============================================================================
// where you prepare the processor to play
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);

    juce::ignoreUnused (sampleRate, samplesPerBlock);
    currentBlockSize = samplesPerBlock; // Austin

	//intialize dsp processors
    formantDetector.prepare(sampleRate);                        //Initialization for FormantDetector for real-time processing - huda
    pitchProcessor.prepare(sampleRate, samplesPerBlock);        //hayley
    formantShifter.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels()); //huda 
    equalizer.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels()); //huda

	// lock mutex for thread safety - reyna
    std::lock_guard<std::recursive_mutex> lock(audioMutex);

	//effect node building - reyna
	// create all default effect nodes and store in effectNodes vector
    effectNodes.clear();
    effectNodes.push_back(std::make_shared<GainNode>(*this));
    effectNodes.push_back(std::make_shared<NoiseGateNode>(*this));
    effectNodes.push_back(std::make_shared<CompressorNode>(*this));
    effectNodes.push_back(std::make_shared<DeEsserNode>(*this));
    effectNodes.push_back(std::make_shared<DeNoiserNode>(*this));
    effectNodes.push_back(std::make_shared<FormantNode>(*this));
    effectNodes.push_back(std::make_shared<PitchNode>(*this));
    effectNodes.push_back(std::make_shared<EqualizerNode>(*this));

    //connect chain
	// set up default chain: Gain > Noise gate > formant > Pitch
	for (auto& n : effectNodes) 
        if (n) n->clearConnections();   //clear any existing connections

	// simple linear connect 
    for (size_t i = 0; i + 1 < effectNodes.size(); ++i) {
        effectNodes[i]->connectTo(effectNodes[i + 1]);
    }

    // shared pointer to active nodes for audio thread
	activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes); 
    rootNode = effectNodes.front();

	// rebuild UI safely
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
    }
}

void AudioPluginAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
//==============================================================================
//real time processor to update everything
// main audio processing block
void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {   
	// all individual processors are called in their respective effect nodes (in their panel.h) - reyna
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    applyPendingLayout();

	// process audio through daisy chain - reyna
    if (!isBypassed() && activeNodes && !activeNodes->empty()) {
		auto chain = activeNodes;   // copy shared
		auto root = chain->front(); //  get root node
        if (root) root->processAndForward(*this, buffer);
    } 

    //juce boilerplate
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() {
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
// State saving/loading - reyna
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData) {
	auto xml = apvts.copyState().createXml();   // get ValueTree as XML
	copyXmlToBinary(*xml, destData);            // copy to binary blob
}

// loading state from binary blob - reyna
void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
	// parse XML from binary blob
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}

//==============================================================================
//==============================================================================

// Default preset loader - reyna
void AudioPluginAudioProcessor::loadDefaultPreset(const juce::String& type) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);

    juce::Logger::outputDebugString("Loading default preset type: " + type);

	// reset the current effectNodes vector to default
    effectNodes.clear();

    // Add the default effects in their intended order
    effectNodes.push_back(std::make_shared<GainNode>(*this));
    effectNodes.push_back(std::make_shared<NoiseGateNode>(*this));
    effectNodes.push_back(std::make_shared<CompressorNode>(*this));
    effectNodes.push_back(std::make_shared<DeEsserNode>(*this));
    effectNodes.push_back(std::make_shared<DeNoiserNode>(*this));
    effectNodes.push_back(std::make_shared<FormantNode>(*this));
    effectNodes.push_back(std::make_shared<PitchNode>(*this));
    effectNodes.push_back(std::make_shared<EqualizerNode>(*this));

    // connect in order gain > noiseGate > compressor > deEsser > fFormant > pitch
    for (auto& node : effectNodes)
        if (node) node->clearConnections();

	// simple linear connect
    for (int i = 0; i + 1 < (int)effectNodes.size(); ++i)
        effectNodes[i]->connectTo(effectNodes[i + 1]);

	// update active nodes and root
    activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
    rootNode = effectNodes.front();

	// rebuild UI safely
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
        juce::Logger::outputDebugString("default preset loaded ");
    }

    // reset pending layout rows
    pendingRows.clear();
    for (auto& node : effectNodes) {
        if (!node) continue;
        Row r;
        r.left = node->effectName;
        pendingRows.push_back(r);
    }
    layoutRequested.store(true);      
}

// empty daisychain preset
void AudioPluginAudioProcessor::clearAllNodes() {
    const std::lock_guard<std::recursive_mutex> lock(getMutex());
    // clear dsp
    effectNodes.clear();
    // clear layout rows
    pendingRows.clear();

    // notify audio thread to apply layout
    layoutRequested.store(true);

    // thread safe empty graph
    rootNode = nullptr;
    activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>();
    // rebuild UI
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
        auto& dc = ed->getDaisyChain();
        dc.clearRows();
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
    }
}

//==============================================================================
//Entire plugin bypass functionality - Austin
bool AudioPluginAudioProcessor::isBypassed() const {
    return bypassed;
}

void AudioPluginAudioProcessor::setBypassed(bool newState) {
    bypassed = newState;
}
