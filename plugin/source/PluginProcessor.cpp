#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/PluginEditor.h"

#include "Pitchblade/panels/GainPanel.h"
#include "Pitchblade/panels/NoiseGatePanel.h"
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/panels/PitchPanel.h"
#include "Pitchblade/panels/CompressorPanel.h"
#include "Pitchblade/panels/DeEsserPanel.h"
#include "Pitchblade/panels/DeNoiserPanel.h"
#include "Pitchblade/panels/EffectNode.h"

#include "Pitchblade/panels/EqualizerPanel.h"
//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), 
                       pitchProcessor(pitchDetector, pitchShifter),

	//apvts contructor: attachs this to processor
    // AudioProcessorValueTreeState used to managea ValueTree that is used to manage an AudioProcessor's entire state
    apvts(*this, nullptr, "Parameters", createParameterLayout()) {
	        // check if effectNodes tree exists
        if (!apvts.state.hasType("EffectNodes")) {
            apvts.state = juce::ValueTree("EffectNodes");   
        }
    }

AudioPluginAudioProcessor::~AudioPluginAudioProcessor(){
    suspendProcessing(true);
}

//============================================================================== reyna
// ui stuff: global APVTS perameter layout 
// no longer shared controls works as a template
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

    // Formant Shifter : huda
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_THRESHOLD", "DeEsser Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_RATIO", "DeEsser Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_ATTACK", "DeEsser Attack", juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESSER_RELEASE", "DeEsser Release", juce::NormalisableRange<float>(1.0f, 300.0f, 0.1f), 5.0f));
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

    //De-Noiser : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DENOISER_REDUCTION", "DeNoiser Reduction", juce::NormalisableRange<float>(0.0f,1.0f,0.01f),0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "DENOISER_LEARN", "DeNoiser Learn", false));

    //Settings Panel: austin
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "GLOBAL_FRAMERATE", "Global Framerate", 1, 4, 3));

    //Pitch Shifter: hayley
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_RETUNE_SPEED", "Pitch Retune Speed", juce::NormalisableRange<float>(0.0f, 1.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_CORRECTION_RATIO", "Pitch Correction Ratio", juce::NormalisableRange<float>(0.0f, 1.0f, 0.05f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_WAVER", "Pitch Waver", juce::NormalisableRange<float>(0.0f, 20.0f, 1.0f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "PITCH_TRANSITION", "Pitch Note Transition", juce::NormalisableRange<float>(0.0f, 50.0f, 1.0f), 20.0f));

    return { params.begin(), params.end() };

}

//==============================================================================
// layout request from UI thread 
// stores new rows and applies on audio thread - reyna
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
    for (auto& n : old) if (n) {
		n->clearConnections(); n->chainMode = ChainMode::Down;  // reset connections and mode
    }

	auto getRowNodes = [&](const Row& r) -> std::pair<std::shared_ptr<EffectNode>, std::shared_ptr<EffectNode>> {   // helper to get nodes by row
        return { findByName(old, r.left), r.right.isNotEmpty() ? findByName(old, r.right) : nullptr };
        };

	std::shared_ptr<EffectNode> prevL = nullptr, prevR = nullptr;   // previous left/right nodes 
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
    for (auto& r : pendingRows) { 	                                                                 // add nodes in order of rows
		if (auto n = findByName(old, r.left)) { newList.push_back(n); }	                            // add left node
        if (!r.right.isEmpty()) {
            if (auto m = findByName(old, r.right)) { newList.push_back(m); }    // add right node if exists
        }
    }

	// update effect node list
    if (!newList.empty()) {
        effectNodes = std::move(newList);
        activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
        rootNode = effectNodes.front();
    }

        // rebuild UI safely without capturing a raw this
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
    {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
    }

}

//==============================================================================

// preset save/load - reyna
void AudioPluginAudioProcessor::savePresetToFile(const juce::File& file) {
	std::lock_guard<std::recursive_mutex> lock(audioMutex);             // lock mutex for thread safety

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

        auto& vt = node->getMutableNodeState(); // node API from EffectNode
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

        for (int i = 0; i + 1 < (int)effectNodes.size(); ++i)
            effectNodes[i]->connectTo(effectNodes[i + 1]);

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
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);

    juce::ignoreUnused (sampleRate, samplesPerBlock);
    
    currentBlockSize = samplesPerBlock; // Austin

	//intialize dsp processors
    formantDetector.prepare(sampleRate);                        //Initialization for FormantDetector for real-time processing - huda
    pitchProcessor.prepare(sampleRate, samplesPerBlock);        //hayley
    formantShifter.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels()); //huda 
    equalizer.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels()); //huda

    std::lock_guard<std::recursive_mutex> lock(audioMutex);

	//effect node building - reyna
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

    for (size_t i = 0; i + 1 < effectNodes.size(); ++i) {
        effectNodes[i]->connectTo(effectNodes[i + 1]);
    }

	activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);  // shared pointer to active nodes for audio thread
    rootNode = effectNodes.front();

    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
    {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
            });
    }
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
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
void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {   
    //real time processor to update everything
    
    // MOVED ALL PROCESS BLOCK STUFF INTO EFFECTNODE AGAINNN DX - reyna
	// all individual processors are called in their respective effect nodes (in their panel.h)
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    applyPendingLayout();
    //applyPendingReorder();

    // indivdual bypass checker reyna
	// process and forward only if root node exists and plugin is not globally bypassed
    //  for future chaining effects theyre all the same rn
    if (!isBypassed() && activeNodes && !activeNodes->empty()) {
		auto chain = activeNodes; // copy shared
		auto root = chain->front(); //  get root node
        if (root)
            root->processAndForward(*this, buffer);
    } else {
        //Austin - this was causing absolutely no audio to go through when there was a global bypass, which is not intended functionality. I commented it out
        //buffer.clear();
    }

    //juce boilerplate
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
// looks like save/load stuff that we can use later - reyna
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // juce::ignoreUnused (destData);

	// reyna: use APVTS to save state
	// create an XML representation of our state
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    // juce::ignoreUnused (data, sizeInBytes);

    //reyna
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

    for (int i = 0; i + 1 < (int)effectNodes.size(); ++i)
        effectNodes[i]->connectTo(effectNodes[i + 1]);

    activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
    rootNode = effectNodes.front();

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
        //r.right = {};
        pendingRows.push_back(r);
    }
    layoutRequested.store(true);      
}

//for empty daisychain preset
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
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
    {
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
bool AudioPluginAudioProcessor::isBypassed() const
{
    return bypassed;
}

void AudioPluginAudioProcessor::setBypassed(bool newState)
{
    bypassed = newState;
}
