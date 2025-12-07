//Austin
//This header defines the VST3Node for DSP/Hosting, the VST3Panel for UI, and the Visualizer

#pragma once
#include <JuceHeader.h>
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/ui/RealTimeGraphVisualizer.h"
#include "Pitchblade/ui/FrequencyGraphVisualizer.h"

class VST3Node;

//This panel handles the UI for loading and scanning VST3s
//It uses a timer to poll the scanning thread progress
class VST3Panel : public juce::Component, private juce::Timer
{
public:
    VST3Panel(AudioPluginAudioProcessor& proc, VST3Node& node);
    ~VST3Panel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    
    //Timer callback to check scan progress
    void timerCallback() override;

private:
    AudioPluginAudioProcessor& processor;
    VST3Node& vstNode;

    //UI Components
    juce::Label titleLabel { "VST3 Loader" };
    juce::TextButton scanButton{ "Scan VST3 Folder" };
    juce::ComboBox pluginList;
    juce::TextButton openGuiButton{ "Open Plugin GUI" };
    juce::Label statusLabel;
    
    //Helper to refresh the combo box from the cached list
    void updatePluginListUI();
};

//Visualizer that switches between Time and Frequency domain
class VST3Visualizer : public juce::Component, private juce::Timer
{
public:
    VST3Visualizer(AudioPluginAudioProcessor& proc, VST3Node& node);
    void resized() override;
    
    //Called at framerate interval
    void timerCallback() override;

private:
    VST3Node& vstNode;

    //Visualizers
    std::unique_ptr<RealTimeGraphVisualizer> timeGraph;
    std::unique_ptr<FrequencyGraphVisualizer> freqGraph;
    
    //Toggle for switching modes
    juce::ToggleButton modeSwitch{ "Show Frequency Spectrum" };
    
    //Helper to build frequency data for the graph
    void updateFrequencyData();
};

//The main DSP node that hosts the VST3 plugin
class VST3Node : public EffectNode
{
public:
    explicit VST3Node(AudioPluginAudioProcessor& proc);
    ~VST3Node() override;

    VST3Node(AudioPluginAudioProcessor& proc, const juce::ValueTree& state);

    //Processing functions
    void process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock);

    //UI Creation
    std::unique_ptr<juce::Component> createPanel(AudioPluginAudioProcessor& proc) override;
    std::unique_ptr<juce::Component> createVisualizer(AudioPluginAudioProcessor& proc) override;
    std::shared_ptr<EffectNode> clone() const override;
    
    //XML Serialization
    std::unique_ptr<juce::XmlElement> toXml() const override;
    void loadFromXml(const juce::XmlElement& xml) override;

    //Public API stuff
    
    //Initializes the format manager (lazy load on Message Thread)
    void initializeHosting(); 
    
    //Syncs the internal list with the global APVTS cache
    void syncFromGlobalCache(); 
    
    //Saves a newly scanned list to the global APVTS cache
    void saveListToGlobalCache(const juce::ValueTree& list);

    //Starts the background scanning thread
    void scanStandardPlugins();
    
    //Status getters
    bool isScanning() const;
    float getScanProgress() const;
    
    //Plugin list access
    const juce::KnownPluginList& getPluginList() const;
    
    //Async load function
    void loadPluginById(const juce::String& pluginId);
    
    //Opens the editor window
    void openPluginEditor();
    
    //Display name getter
    juce::String getLoadedPluginName() const;

    //Analysis Data Accessors
    float getCurrentLevelDb() const { return currentLevelDb.load(); }
    bool getNextFFTBlock(std::vector<float>& dest);

private:
    //Hosting stuff
    std::unique_ptr<juce::AudioPluginFormatManager> formatManager;
    juce::KnownPluginList knownPluginList; 
    std::unique_ptr<juce::AudioPluginInstance> hostedPlugin;
    
    //Name string for display (separate from node ID)
    juce::String loadedPluginName; 

    //Scanning stuff
    class ScannerThread : public juce::Thread {
    public:
        ScannerThread(VST3Node& node) : Thread("VST3 Scanner"), owner(node) {}
        void run() override;
        VST3Node& owner;
        std::atomic<float> progress{ 0.0f };
    };
    std::unique_ptr<ScannerThread> scannerThread;

    //Analysis stuff
    std::atomic<float> currentLevelDb { -100.0f };
    
    //FFT stuff
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifo;
    std::vector<float> fftData;
    std::atomic<bool> nextFFTBlockReady { false };
    int fifoIndex = 0;

    //Window stuff
    //Using SafePointer to avoid dangling references if window closes itself
    juce::Component::SafePointer<juce::DocumentWindow> activeWindow;
    
    //Helper to finish loading on MessageThread
    void finishLoad(std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& errorMsg, const juce::String& preferredName = {});
    
    friend class ScannerThread;
};