/*
    PluginProcessor class handles all audio related logic for Pitchblade.

    It creates and manages the DSP processors, owns the AudioProcessorValueTreeState,
    builds the effect node chain, updates routing when the user changes the DaisyChain,
    and processes audio for every block. 

    It also manages preset loading and saving, layout updates, global bypass, and 
    synchronization between the UI thread and the audio thread
*/

#pragma once
#include <vector>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
//Austin
#include "Pitchblade/effects/GainProcessor.h"       
#include "Pitchblade/effects/CompressorProcessor.h" 
#include "Pitchblade/effects/DeEsserProcessor.h"    
#include "Pitchblade/effects/DeNoiserProcessor.h"   
#include "Pitchblade/effects/NoiseGateProcessor.h"  
//huda
#include "Pitchblade/effects/FormantDetector.h"     
#include "Pitchblade/effects/FormantShifter.h"      
#include "Pitchblade/effects/Equalizer.h"           
//hayley
#include "Pitchblade/effects/PitchCorrector.h"      
//reyna
#include "Pitchblade/panels/EffectNode.h"           
class EffectNode;   // forward declaration for effectNode order 

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor {
public:
    //==============================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

	//============================== dsp
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //============================== editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //============================== info
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

	//============================== programs
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

	//============================== state
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	//============================== global bypass - reyna
    bool isBypassed() const;
    void setBypassed(bool newState);

    //============================== stored parameters for daisy chain - reyna
	juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

	std::vector<std::shared_ptr<EffectNode>>& getEffectNodes() { return effectNodes; }
	std::recursive_mutex& getMutex() { return audioMutex; } 

    //============================================================================== DSP processors

    // austin
    GainProcessor& getGainProcessor() { return gainProcessor; }
    NoiseGateProcessor& getNoiseGateProcessor() { return noiseGateProcessor; }
    CompressorProcessor& getCompressorProcessor() { return compressorProcessor; }
    DeEsserProcessor& getDeEsserProcessor() { return deEsserProcessor; }
    DeNoiserProcessor& getDeNoiserProcessor() { return deNoiserProcessor; }

    int getCurrentBlockSize() const {return currentBlockSize;}; // Austin - Was having an issue initializing de-esser

    // huda
    FormantDetector& getFormantDetector() { return formantDetector; }
    void setLatestFormants(const std::vector<float>& freqs) { latestFormants = freqs; }
    const std::vector<float>& getLatestFormants() { return latestFormants; }
    FormantShifter& getFormantShifter() { return formantShifter; }
    Equalizer& getEqualizer() { return equalizer; }

    //hayley
    PitchCorrector& getPitchCorrector() { return pitchProcessor; }

    //reyna 
	// effect node chain management
	void setRootNode(std::shared_ptr<EffectNode> node) { rootNode = std::move(node); }  // set root node for processing chain
	struct Row { juce::String left, right; };                                           // processing chain row
	void requestLayout(const std::vector<Row>& newRows);                                // request new layout for processing chain 
    std::vector<Row> getCurrentLayoutRows();                                            //getter for current layout of rows for ui 

	// preset management
    void savePresetToFile(const juce::File& file);
    void loadPresetFromFile(const juce::File& file);
    void loadDefaultPreset(const juce::String& type);
    void clearAllNodes();  

private:
    //============================================================================== 
    //processors
    
    //austin
    GainProcessor gainProcessor;            
    NoiseGateProcessor noiseGateProcessor;
    CompressorProcessor compressorProcessor; 
    DeEsserProcessor deEsserProcessor;      
    DeNoiserProcessor deNoiserProcessor;  

    int currentBlockSize = 512;

    // huda
    Equalizer equalizer;                    
    FormantDetector formantDetector;        // To handle detection
    std::vector<float> latestFormants;      // Vector to store formants 
    FormantShifter formantShifter;   

    //hayley                
    PitchDetector pitchDetector;            
    PitchShifter pitchShifter;
    PitchCorrector  pitchProcessor;         // To correct pitch

	// reyna 
    // global bypass
    bool bypassed = false;
    
	// effect nodes for the processing chain
	std::vector<std::shared_ptr<EffectNode>> effectNodes;                   // all available effect nodes
	std::shared_ptr<std::vector<std::shared_ptr<EffectNode>>> activeNodes;  // copy of current active nodes for reordering
	std::shared_ptr<EffectNode> rootNode;                                   // root node of chain for processing

    //reorder queue
	std::recursive_mutex audioMutex;                    // mutex for audio thread safety
	std::atomic<bool> reorderRequested{ false };        // flag for reorder request

	//layout  rows
	std::vector<Row> pendingRows;                   // new layout to apply
	std::atomic<bool> layoutRequested{ false };     // flag for layout request
    void applyPendingLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};