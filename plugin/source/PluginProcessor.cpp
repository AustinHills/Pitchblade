#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/PluginEditor.h"
//austin
#include "Pitchblade/panels/GainPanel.h"
#include "Pitchblade/panels/NoiseGatePanel.h"
#include "Pitchblade/panels/CompressorPanel.h"
#include "Pitchblade/panels/BandPassCompressorPanel.h"
#include "Pitchblade/panels/DeEsserPanel.h"
#include "Pitchblade/panels/DeNoiserPanel.h"
#include "Pitchblade/panels/SaturationPanel.h"
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/panels/AutoGainPanel.h"
//huda
#include "Pitchblade/panels/FormantPanel.h"
#include "Pitchblade/panels/EqualizerPanel.h"
//hayley
#include "Pitchblade/panels/PitchPanel.h"
#include "Pitchblade/panels/AdaptiveDeNoiserPanel.h"

#include "Pitchblade/panels/VST3Panel.h"

#include <csignal>
#include <exception>

#include <chrono>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment(lib, "Dbghelp.lib")
#else
 #include <unistd.h>
#endif

// 1. The Core Reporter (Unchanged)
void handleCrash(const juce::String& source)
{
    
    // Quick copy for safety if you need it:
    int pid = 0;
    #if JUCE_WINDOWS
        pid = (int)GetCurrentProcessId();
    #else
        pid = (int)getpid();
    #endif

    juce::String report = "Pitchblade Crash Report [" + source + "]\n";
    report += "--------------------------------------------------\n";
    report += "Time: " + juce::Time::getCurrentTime().toString(true, true) + "\n";
    report += "OS: " + juce::SystemStats::getOperatingSystemName() + "\n";
    report += "Process ID: " + juce::String(pid) + "\n";
    report += "--------------------------------------------------\n";
    report += "Stack Trace:\n";
    report += juce::SystemStats::getStackBacktrace();

    try {
        juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File crashDir = docsDir.getChildFile("Pitchblade").getChildFile("Crash_Reports");
        if (crashDir.createDirectory().wasOk()) {
            juce::String filename = "Crash_" + juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S") + ".txt";
            crashDir.getChildFile(filename).replaceWithText(report);
        }
    } catch (...) {}

    juce::String msg = "Pitchblade has encountered a fatal error (" + source + ").\nA report has been saved.";
    #if JUCE_WINDOWS
        ::MessageBoxA(nullptr, msg.toRawUTF8(), "Pitchblade Crash Reporter", MB_OK | MB_ICONERROR);
    #else
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Pitchblade Crash", msg);
        juce::Thread::sleep(3000); 
    #endif
}

#if JUCE_WINDOWS
// --- VECTORED EXCEPTION HANDLER (The Workaround) ---
// This runs BEFORE any VST3 plugin can hide the crash.
LONG WINAPI VectoredCrashHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;

    // IGNORE these common non-fatal exceptions:
    // 0x406D1388: SetThreadName (Used by debuggers)
    // 0xE06D7363: C++ Exception (Used for normal control flow in plugins)
    // 0x40010006: OutputDebugString
    if (code == 0x406D1388 || code == 0xE06D7363 || code == 0x40010006) 
        return EXCEPTION_CONTINUE_SEARCH;

    // CATCH these fatal errors:
    // 0xC0000005: Access Violation (The big one)
    // 0xC00000FD: Stack Overflow (Common in audio plugins)
    // 0xC000001D: Illegal Instruction
    // 0xC0000094: Integer Divide by Zero
    if (code == 0xC0000005 || code == 0xC00000FD || code == 0xC000001D || code == 0xC0000094)
    {
        handleCrash("Vectored Handler - Code: 0x" + juce::String::toHexString((int)code));
        
        // We do NOT return EXCEPTION_EXECUTE_HANDLER here because that would 
        // stop the debugger from seeing it. We just log and let it die.
        return EXCEPTION_CONTINUE_SEARCH; 
    }

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void TerminateHandler() { 
    handleCrash("std::terminate"); 
    std::abort(); 
}

void StandardSignalHandler(int signum) {
    handleCrash("Signal " + juce::String(signum));
    std::_Exit(signum); 
}

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
    apvts(*this, &undoManager, "Parameters", createParameterLayout()) {

    std::set_terminate(TerminateHandler);

    if (juce::JUCEApplication::isStandaloneApp()) 
    {
        // Windows Vectored Handler
        #if JUCE_WINDOWS
            AddVectoredExceptionHandler(1, VectoredCrashHandler);
        #endif

        // Standard Signals
        std::set_terminate(TerminateHandler);
        signal(SIGABRT, StandardSignalHandler);
        signal(SIGFPE,  StandardSignalHandler);
        #if !JUCE_WINDOWS
            signal(SIGSEGV, StandardSignalHandler);
        #endif
    }

	    // check if effectNodes tree exists
    if (!apvts.state.getChildWithName("Chain").isValid()) {
        apvts.state.addChild(juce::ValueTree("Chain"), -1, nullptr);
    }
    
    // Add listener to the main state (or specifically the chain)
    apvts.state.addListener(this);

    // Initialize Monitor Buffer
    monitorBuffer.setSize(2, 48000);
    monitorBuffer.clear();

    // Initialize Monitor Device Manager (if Standalone) - scans for devices
    if (juce::JUCEApplication::isStandaloneApp()) {
        monitorDeviceManager.initialise(0, 2, nullptr, true);
        monitorDeviceManager.addAudioCallback(&monitorCallback);
    }
}

// Destructor: ensures processor is suspended when the its deleted
AudioPluginAudioProcessor::~AudioPluginAudioProcessor(){ 
    if (juce::JUCEApplication::isStandaloneApp()) {
        monitorDeviceManager.removeAudioCallback(&monitorCallback);
    }
    suspendProcessing(true); 
}

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

    // BandPassCompressor : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_THRESHOLD", "BandPass Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_RATIO", "BandPass Ratio", juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_ATTACK", "BandPass Attack", juce::NormalisableRange<float>(1.0f, 200.0f, 1.0f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_RELEASE", "BandPass Release", juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_MIN_FREQ", "BandPass Min Freq", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f), 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BANDPASS_MAX_FREQ", "BandPass Max Freq", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f), 2000.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "BANDPASS_LISTEN", "BandPass Listen", false));

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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DENOISER_THRESHOLD", "DeNoiser Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -100.0f));

    //Saturation : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SAT_DRIVE", "Saturation Drive", juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SAT_MIX", "Saturation Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    // AutoGain : austin
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "AUTOGAIN_TARGET", "AutoGain Target", juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "AUTOGAIN_THRESHOLD", "AutoGain Threshold", juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -40.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "AUTOGAIN_ATTACK", "AutoGain Attack", juce::NormalisableRange<float>(0.1f, 500.0f, 0.1f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "AUTOGAIN_RELEASE", "AutoGain Release", juce::NormalisableRange<float>(10.0f, 2000.0f, 0.1f), 200.0f));

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

// helper to find node by name in list
static std::shared_ptr<EffectNode> findByName(const std::vector<std::shared_ptr<EffectNode>>& list, const juce::String& name) {
    for (auto& n : list) if (n && n->effectName == name) return n;
    return {};
}

//============================================================================== preset save/load - reyna
// saving presets to file
void AudioPluginAudioProcessor::savePresetToFile(const juce::File& file) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);

    // This allows VST3 nodes to write their opaque parameter chunk into the tree
    for (auto& node : effectNodes) {
        if (node) node->flushStateToValueTree();
    }

    // apvts.state contains EVERYTHING: Params, Chain structure, Node UUIDs.
    auto xml = apvts.state.createXml();
    xml->setTagName("PitchbladePreset");
    xml->writeTo(file);
}

// loading presets from file
void AudioPluginAudioProcessor::loadPresetFromFile(const juce::File& file) {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (!xml) return;

    if (xml->hasTagName("PitchbladePreset")) {
        // Load the whole tree. The listeners will fire and rebuild everything.
        // We use CopyProperties to preserve the root, but replace children.
        juce::ValueTree newState = juce::ValueTree::fromXml(*xml);
        
        if (newState.isValid()) {
            apvts.replaceState(newState);
            // Ensure Chain child exists
            if (!apvts.state.getChildWithName("Chain").isValid())
                apvts.state.addChild(juce::ValueTree("Chain"), -1, nullptr);

            apvts.state.addListener(this);

            syncChainFromState();
        }

        triggerUIRebuild();
    }
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

	// Initial load: If the chain is empty (first run), populate defaults via ValueTree
    auto chainState = apvts.state.getChildWithName("Chain");
    
    if (chainState.getNumChildren() == 0) {
        // Load default preset logic, but via ValueTree transactions
        // Note: We don't use UndoManager here as this is initialization
        loadDefaultPreset("default"); 
        syncChainFromState();
    } else {
        // Just sync vector to existing state
        syncChainFromState();
    }
    
    // UI update
    if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
        juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safe(ed);
        juce::MessageManager::callAsync([safe]() {
            if (auto* e = safe.getComponent())
                e->rebuildAndSyncUI();
        });
    }

    // Reset the load measurer
    loadMeasurer.reset(sampleRate, samplesPerBlock);
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

    // Start measuring time for this block
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // This JUCE helper automatically calculates the % load for us
    juce::AudioProcessLoadMeasurer::ScopedTimer loadTimer(loadMeasurer);

	// process audio through daisy chain - reyna
    if (!isBypassed() && activeNodes && !activeNodes->empty()) {
		auto chain = activeNodes;   // copy shared
		auto root = chain->front(); //  get root node
        if (root) root->processAndForward(*this, buffer);
    } 

    // Stop measuring time
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // Calculate duration in milliseconds
    std::chrono::duration<float, std::milli> duration = endTime - startTime;
    
    // Update the atomic variables (UI will read these)
    processTimeMs.store(duration.count());
    cpuLoad.store(loadMeasurer.getLoadAsProportion());

    //juce boilerplate
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    // ============================================================
    // Standalone Monitor Output Logic
    // ============================================================
    if (juce::JUCEApplication::isStandaloneApp() && monitorDeviceManager.getCurrentAudioDevice() != nullptr) {
        // 1. Get Mixed Stereo Signal (taking main L/R)
        // We assume Main Output is channels 0 and 1.
        if (buffer.getNumChannels() >= 2) {
            float rawVol = monitorVolume.load();
            float vol = rawVol * rawVol; // Quadratic taper for balanced feels
            int numSamples = buffer.getNumSamples();

            // Prepare to write to FIFO
            // We want to write 'numSamples' into the ring buffer
            int start1, size1, start2, size2;
            monitorFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
            


            if (size1 > 0) {
                for (int ch = 0; ch < 2; ++ch) {
                    // Copy and Apply Volume
                    monitorBuffer.copyFrom(ch, start1, buffer, ch, 0, size1);
                    monitorBuffer.applyGain(ch, start1, size1, vol);
                }
            }
            if (size2 > 0) {
                for (int ch = 0; ch < 2; ++ch) {
                    monitorBuffer.copyFrom(ch, start2, buffer, ch, size1, size2);
                    monitorBuffer.applyGain(ch, start2, size2, vol);
                }
            }
            monitorFifo.finishedWrite(size1 + size2);
        }
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
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            
            // Re-attach listener as the underlying ValueTree object has changed
            apvts.state.addListener(this);
            
            // Force sync to ensure effectNodes match the loaded state
            syncChainFromState();
        }
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
    // Stop audio processing for a moment?
    
    apvts.state.removeAllChildren(&undoManager); // Clear everything
    
    // Create new Chain
    juce::ValueTree chain("Chain");
    
    // Add default nodes to the Chain ValueTree
    // We use a helper to create the state for a new node
    auto addNode = [&](const juce::String& type, const juce::String& name) {
        juce::ValueTree node(type);
        node.setProperty("name", name, nullptr);
        node.setProperty("uuid", juce::Uuid().toString(), nullptr);
        chain.addChild(node, -1, nullptr);
    };

    addNode("GainNode", "Gain");
    addNode("CompressorNode", "Compressor");
    addNode("FormantNode", "Formant");
    addNode("EqualizerNode", "Equalizer");
    
    // Attach chain to main state (will trigger listeners)
    apvts.state.addChild(chain, -1, &undoManager);
}

// empty daisychain preset
void AudioPluginAudioProcessor::clearAllNodes() {
    auto chain = apvts.state.getChildWithName("Chain");
    chain.removeAllChildren(&undoManager);
}

//==============================================================================
//Entire plugin bypass functionality - Austin
bool AudioPluginAudioProcessor::isBypassed() const {
    return bypassed;
}

void AudioPluginAudioProcessor::setBypassed(bool newState) {
    bypassed = newState;
}


//Helper functions
std::shared_ptr<EffectNode> AudioPluginAudioProcessor::createNodeFromState(const juce::ValueTree& state) {
    // Factory method
    juce::String type = state.getType().toString();
    std::shared_ptr<EffectNode> node;

    if      (type == "GainNode")        node = std::make_shared<GainNode>(*this, state);
    else if (type == "NoiseGateNode")   node = std::make_shared<NoiseGateNode>(*this, state);
    else if (type == "CompressorNode")  node = std::make_shared<CompressorNode>(*this, state);
    else if (type == "BandPassCompressorNode") node = std::make_shared<BandPassCompressorNode>(*this, state);
    else if (type == "DeEsserNode")     node = std::make_shared<DeEsserNode>(*this, state);
    else if (type == "DeNoiserNode")    node = std::make_shared<DeNoiserNode>(*this, state);
    else if (type == "AdaptiveDeNoiserNode")    node = std::make_shared<AdaptiveDeNoiserNode>(*this, state);
    else if (type == "SaturationNode")          node = std::make_shared<SaturationNode>(*this, state);
    else if (type == "AutoGainNode")            node = std::make_shared<AutoGainNode>(*this, state);
    else if (type == "EqualizerNode")   node = std::make_shared<EqualizerNode>(*this, state);  
    else if (type == "PitchNode")       node = std::make_shared<PitchNode>(*this, state);
    else if (type == "FormantNode")     node = std::make_shared<FormantNode>(*this, state);
    else if (type == "VST3Node")        node = std::make_shared<VST3Node>(*this, state);
    
    // Note: EffectNode constructor now takes (proc, ExistingValueTree)
    // You might need to adjust the EffectNode constructors slightly if they 
    // assume they are creating a FRESH ValueTree vs wrapping an existing one.
    // However, your EffectNode.h shows a constructor `EffectNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState)`
    // So this works perfectly.
    
    return node;
}

void AudioPluginAudioProcessor::syncChainFromState() {
    std::lock_guard<std::recursive_mutex> lock(audioMutex);
    
    auto chainState = apvts.state.getChildWithName("Chain");
    std::vector<std::shared_ptr<EffectNode>> newNodes;
    
    // 1. Sync Objects
    for (auto child : chainState) {
        auto it = std::find_if(effectNodes.begin(), effectNodes.end(), 
            [&](const std::shared_ptr<EffectNode>& n) { return n->getNodeStateConst() == child; });
            
        std::shared_ptr<EffectNode> node;
        if (it != effectNodes.end()) {
            node = *it;
        } else {
            node = createNodeFromState(child);
        }

        if (node) {
            node->clearConnections();
            // Reset to "Stored User Intent" (1=Down, 3=DoubleDown)
            // We use this to group them, then overwrite with DSP modes later.
            int storedMode = child.getProperty("chainMode", 1);
            node->chainMode = static_cast<ChainMode>(storedMode);
            newNodes.push_back(node);
        }
    }
    
    effectNodes = newNodes;

    // 2. Group into Rows
    struct Row {
        std::shared_ptr<EffectNode> left;
        std::shared_ptr<EffectNode> right;
        bool isDouble() const { return right != nullptr; }
    };
    std::vector<Row> rows;

    for (auto& node : effectNodes) {
        // If marked as DoubleDown (3) -> It is a Right Side Candidate
        if (node->chainMode == ChainMode::DoubleDown) {
            if (!rows.empty() && rows.back().right == nullptr) {
                rows.back().right = node;
            } else {
                // Orphaned right node? Treat as new Left.
                rows.push_back({ node, nullptr });
            }
        } 
        else {
            // Standard Node -> New Left Side
            rows.push_back({ node, nullptr });
        }
    }

    // 3. Apply Calculated DSP Modes & Connections
    for (size_t i = 0; i < rows.size(); ++i) {
        auto& current = rows[i];
        
        // -- Configure CURRENT Row Modes --
        if (current.isDouble()) {
            // Explicitly set Left -> LeftDouble (5) and Right -> DoubleDown (3)
            current.left->chainMode = ChainMode::LeftDouble;
            current.right->chainMode = ChainMode::DoubleDown;
        } else {
            // Single: Default to Down (1) ONLY if it wasn't already set to Unite (4)
            // by the previous row's logic.
            if (current.left->chainMode != ChainMode::Unite) {
                current.left->chainMode = ChainMode::Down;
            }
        }

        // -- Connect to NEXT Row --
        if (i + 1 < rows.size()) {
            auto& next = rows[i + 1];

            if (!current.isDouble() && !next.isDouble()) {
                // Single -> Single
                current.left->connectTo(next.left);
            }
            else if (!current.isDouble() && next.isDouble()) {
                // Single -> Double (SPLIT)
                current.left->connectTo(next.left);
                current.left->connectTo(next.right);
                current.left->chainMode = ChainMode::Split; // Force Split
            }
            else if (current.isDouble() && !next.isDouble()) {
                // Double -> Single (UNITE)
                current.left->connectTo(next.left);
                current.right->connectTo(next.left);
                next.left->chainMode = ChainMode::Unite;    // Force Next to Unite
            }
            else if (current.isDouble() && next.isDouble()) {
                // Double -> Double (PARALLEL)
                current.left->connectTo(next.left);
                current.right->connectTo(next.right);
                // Preserve LeftDouble/DoubleDown modes
            }
        }
    }

    activeNodes = std::make_shared<std::vector<std::shared_ptr<EffectNode>>>(effectNodes);
    rootNode = rows.empty() ? nullptr : rows.front().left;
}

//Listeners

void AudioPluginAudioProcessor::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child) {
    if (parent.hasType("Chain") || child.hasType("Chain")) {
        syncChainFromState();
        // Trigger UI rebuild
        if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
            juce::MessageManager::callAsync([ed]() { ed->rebuildAndSyncUI(); });
    }
}

void AudioPluginAudioProcessor::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int) {
    if (parent.hasType("Chain") || child.hasType("Chain")) {
        syncChainFromState();
        if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
            juce::MessageManager::callAsync([ed]() { ed->rebuildAndSyncUI(); });
    }
}

void AudioPluginAudioProcessor::valueTreeChildOrderChanged(juce::ValueTree& parent, int, int) {
    if (parent.hasType("Chain")) {
        syncChainFromState();
        if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor()))
            juce::MessageManager::callAsync([ed]() { ed->rebuildAndSyncUI(); });
    }
}

void AudioPluginAudioProcessor::triggerUIRebuild() {
        if (auto* ed = dynamic_cast<AudioPluginAudioProcessorEditor*>(getActiveEditor())) {
            juce::MessageManager::callAsync([ed]() { ed->rebuildAndSyncUI(); });
        }
    }

void AudioPluginAudioProcessor::valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) {
    // If the chain structure property "chainMode" changes, we must rebuild the DSP graph.
    // We also listen for "name" to update UI labels if needed.
    if (property.toString() == "chainMode" || property.toString() == "name") {
        syncChainFromState();
        triggerUIRebuild();
    }
}


//==============================================================================
// Standalone Monitor Implementation
//==============================================================================

void AudioPluginAudioProcessor::MonitorOutputCallback::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);
    
    // Safety check
    if (numOutputChannels == 0 || outputChannelData == nullptr) return;

    // Clear buffer first
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i])
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

    // Read from FIFO
    int start1, size1, start2, size2;
    owner.monitorFifo.prepareToRead(numSamples, start1, size1, start2, size2);

    // If we don't have enough data (Underrun), simply output silence (already cleared)
    // and DO NOT advance read pointer. Or we could advance up to available?
    // Standard ring buffer logic: if empty, play silence.
    int totalAvailable = size1 + size2;
    
    if (totalAvailable < numSamples) {
        // Buffer Underrun - Just play silence (we already cleared output)

        return; 
    }

    // Read Logic
    if (size1 > 0) {
        for (int i = 0; i < juce::jmin(2, numOutputChannels); ++i) {
             juce::FloatVectorOperations::copy(outputChannelData[i], 
                                             owner.monitorBuffer.getReadPointer(i, start1),
                                             size1);
        }
    }
    if (size2 > 0) {
        for (int i = 0; i < juce::jmin(2, numOutputChannels); ++i) {
              // Offset output ptr by size1
             juce::FloatVectorOperations::copy(outputChannelData[i] + size1, 
                                             owner.monitorBuffer.getReadPointer(i, start2),
                                             size2);
        }
    }

    owner.monitorFifo.finishedRead(size1 + size2);
}

void AudioPluginAudioProcessor::setMonitorDevice(const juce::String& deviceName) {
    if (deviceName.isEmpty()) {
        monitorDeviceManager.closeAudioDevice();
        return;
    }

    // Check if it's already current
    auto* current = monitorDeviceManager.getCurrentAudioDevice();
    if (current && current->getName() == deviceName) return;

    // Initialize with specific device
    // We want 0 inputs, 2 outputs.
    // NOTE: This usually needs to be done on Message Thread. This function is called from UI, so it is safe.
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    monitorDeviceManager.getAudioDeviceSetup(setup);
    
    setup.outputDeviceName = deviceName;
    setup.inputDeviceName = ""; // No input for monitor
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    
    // Try to init
    // initialise(numInputChannelsNeeded, numOutputChannelsNeeded, savedStateXml, selectDefaultDeviceOnFailure, preferredDefaultDeviceName, preferredSetupOptions)
    // We use setAudioDeviceSetup which is cleaner for switching
    
    // monitorDeviceManager.setAudioDeviceSetup(setup, true);
    juce::String err = monitorDeviceManager.setAudioDeviceSetup(setup, true);
    
    // reset fifo just in case
    monitorFifo.reset();
}