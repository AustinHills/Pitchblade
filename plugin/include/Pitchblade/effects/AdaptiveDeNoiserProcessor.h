//Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>

//This is needed to store various audio information
#include <vector>

//Defining the class that handles denoiser logic
class AdaptiveDeNoiserProcessor{
private:
    //User defined parameters
    //This determines the intensity by which the noise reduction algorithm takes away from various frequencies. It ranges from 0.0 to 1.0
    float reductionAmount = 0.5f;

    //User defined threshold in dB
    float threshold = -60.0f;

    //How fast the profile adapts
    const float learningRate = 0.05f;

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

    //Buffers for overlap add
    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    int inputBufferPos = 0;
    int outputBufferPos = 0;

    //Buffers for processing
    std::vector<float> fftData;
    std::vector<float> noiseProfile;

    //Main processing for a single frame
    void processFrame();

    //Storage for visualizer data
    juce::CriticalSection dataMutex;
    std::vector<juce::Point<float>> currentSpectrumData;
    std::vector<juce::Point<float>> noiseProfileData;
public:
    //Constructor
    AdaptiveDeNoiserProcessor();

    //Called to prepare the denoiser with the current sample rate
    void prepare(const double sRate);

    //Setters for parameters
    void setReduction(float reduction);
    void setThreshold(float thresholdDB);

    //Processes the input audio buffer to apply denoising
    void process(juce::AudioBuffer<float>& buffer);

    //Getters for visualizer data
    std::vector<juce::Point<float>> getSpectrumData();
    std::vector<juce::Point<float>> getNoiseProfileData();
};