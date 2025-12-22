//Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>

//This is needed to store various audio information
#include <vector>

//Defining the class that handles denoiser logic
class DeNoiserProcessor{
private:
    //User defined parameters
    //This determines the intensity by which the noise reduction algorithm takes away from various frequencies. It ranges from 0.0 to 1.0
    float reductionAmount = 0.5f;

    //This determines if the processor is currently learning
    bool isLearning = false;

    //Sample rate
    double sampleRate = 44100.0;

    //This increases the power of the reduction even further. I found that 2.0x wasn't enough, so I made a constant that can be altered in this header to fine-tune it
    const float POWER_MULTIPLIER = 4.0f;

    // FFT and Overlap add parameters
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 2048;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int overlap = fftSize - hopSize;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    // Struct to hold state for each channel
    struct ChannelState {
        //Buffers for overlap add
        std::vector<float> inputBuffer;
        std::vector<float> outputBuffer;
        int inputBufferPos = 0;
        int outputBufferPos = 0;

        //Buffers for processing
        std::vector<float> fftData;
        std::vector<float> noiseProfile;

        //Internal counters
        int noiseProfileSamples = 0;

        // Visualizer snapshots specific to this channel
        std::vector<juce::Point<float>> latestSpectrumSnapshot;
        std::vector<juce::Point<float>> latestNoiseSnapshot;

        ChannelState() {
            inputBuffer.resize(fftSize, 0.0f);
            outputBuffer.resize(fftSize, 0.0f);
            fftData.resize(fftSize * 2, 0.0f);
            noiseProfile.resize(fftSize / 2 + 1, 0.0f);
            latestSpectrumSnapshot.resize(fftSize / 2 + 1);
            latestNoiseSnapshot.resize(fftSize / 2 + 1);
        }
    };

    // Vector of channel states
    std::vector<ChannelState> channels;

    //Main processing for a single frame
    void processFrame(ChannelState& state);

    //Storage for visualizer data
    juce::CriticalSection dataMutex;
    std::vector<juce::Point<float>> currentSpectrumData;
    std::vector<juce::Point<float>> noiseProfileData;

    //Helper to average visualizer data from all channels
    void updateVisualizer();

public:
    //Constructor
    DeNoiserProcessor();

    //Called to prepare the denoiser with the current sample rate
    void prepare(const double sRate);

    //Setters for parameters
    void setReduction(float reduction);
    void setLearning(bool learning);

    //Processes the input audio buffer to apply denoising
    void process(juce::AudioBuffer<float>& buffer);

    //Getters for visualizer data
    std::vector<juce::Point<float>> getSpectrumData();
    std::vector<juce::Point<float>> getNoiseProfileData();
};