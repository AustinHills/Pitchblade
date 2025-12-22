//Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include <vector>
#include <deque>

class DeClickerProcessor {
private:
    // User parameters
    int historyLength = 5;      // How many frames back to look (1-50)
    int lookAheadDepth = 4;     // How many frames forward to look (Latency)
    float sensitivity = 0.5f;   // 0.0 to 1.0 (Threshold multiplier)

    // Constants
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 2048;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int overlap = fftSize - hopSize;

    double sampleRate = 44100.0;

    // static constexpr int overlap = fftSize - hopSize; // Already defined above
    // double sampleRate = 44100.0; // Already defined above

    // Per-channel state for stereo independence
    struct ChannelState {
        // Buffers for overlap-add
        std::vector<float> inputBuffer;
        std::vector<float> outputBuffer; 
        int inputBufferPos = 0;
        int outputBufferPos = 0;

        // History of spectral magnitudes [frame_index][bin_index]
        std::deque<std::vector<float>> spectralHistory;
        
        // Future frames for Sustain Verification
        std::deque<std::vector<float>> lookAheadBuffer;

        // Stuff for processing
        std::vector<float> fftData; // Scratch buffer for this channel
        
        // Independent FFT/Window for thread-safety/state-isolation
        std::unique_ptr<juce::dsp::FFT> forwardFFT;
        std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
        
        // For visualizers
        std::vector<float> currentSpectrum; // The raw input spectrum of the latest frame
        std::vector<float> processedSpectrum; // The output spectrum of the latest frame
    };

    std::vector<ChannelState> channels;

    // Internal processing
    void processFrame(ChannelState& channel);

    // Visualizer data sync
    juce::CriticalSection dataMutex;
    std::vector<juce::Point<float>> visualizerInputData;
    std::vector<juce::Point<float>> visualizerOutputData;

public:
    DeClickerProcessor();

    void prepare(double sRate, int numChannels);
    void process(juce::AudioBuffer<float>& buffer);

    // parameter setters
    void setHistoryLength(int length);
    void setSensitivity(float sens);

    // Visualizer getters (returns averaged stereo data)
    std::vector<juce::Point<float>> getInputSpectrumData();
    std::vector<juce::Point<float>> getProcessedSpectrumData();
};
