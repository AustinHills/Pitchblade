// Written by Austin Hills

#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

class DeEsserProcessor{
private:
    //The level in dB that de-essing will start
    float threshold = 0.0f;
    //The amount of gain reduction
    float ratio = 4.0f;
    //Attack time in ms
    float attackTime = 5.0f;
    //Release time in ms
    float releaseTime = 5.0f;
    //The center frequency of the sibilance detector. Main de-esser control
    float frequency = 6000.0f;

    //Internal variables. See compressor.h for more information
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
    double sampleRate = 44100.0;

    //For TC-17, I am creating this to act as a bridge across the gap of waveform peaks so that attack and release work as intended
    //This tracks peaks rather than averages
    float rippleEnvelope = 0.0f;

    //A stereo IIR filter to detect sibilant frequencies in the sidechain
    //Use an array to hold one filter per channel
    std::array<juce::dsp::IIR::Filter<float>, 2> sidechainFilters;

    //Updates attack and release coefficients
    void updateAttackAndRelease();
    //Updates the sidechain filter coefficients when frequency or sample rate changes
    void updateFilter();

    //Visualizer stuff
    // FFT and Overlap add parameters for visualizer
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 2048;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int overlap = fftSize - hopSize;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    //Buffers for visualizer FFT
    std::vector<float> fftInputBuffer;
    int fftInputBufferPos = 0;

    //Buffers for processing
    std::vector<float> fftData;

    //Main processing for a single visualizer frame
    void processVisualizerFrame();

    //Storage for visualizer data
    juce::CriticalSection dataMutex;
    std::vector<juce::Point<float>> currentSpectrumData;

public:
    //Constructor
    DeEsserProcessor();

    //Called before processing to prepare the de-esser with the current sample rate
    void prepare(double sRate, int samplesPerBlock);

    //Setters
    void setThreshold(float thresholdDB);
    void setRatio(float ratioValue);
    void setAttack(float attackMs);
    void setRelease(float releaseMs);
    void setFrequency(float frequencyInHz);

    //Processes the input audio buffer to apply de-essing
    void process(juce::AudioBuffer<float>& buffer);

    //For visualizer
    std::vector<juce::Point<float>> getSpectrumData();
};