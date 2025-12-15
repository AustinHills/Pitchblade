// Written by Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

class BandPassCompressorProcessor
{
private:
    // Parameters (User defined)
    float thresholdDB = 0.0f;
    float ratio = 1.0f;
    float attackTime = 10.0f; // ms
    float releaseTime = 100.0f; // ms
    float minFrequency = 200.0f; // Hz
    float maxFrequency = 2000.0f; // Hz
    bool listenMode = false;

    // Internal dynamics variables
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
    double sampleRate = 44100.0;

    // Filters for splitting the bands
    // Filter Logic:
    // We use Linkwitz-Riley 4th Order (24dB/oct) crossovers to ensure flat magnitude summing.
    // LR-4 is created by cascading two Butterworth 2nd Order filters.
    using FilterType = juce::dsp::IIR::Filter<float>;
    using FilterChain = juce::dsp::ProcessorChain<FilterType, FilterType>;

    // LowPass at MinFrequency (For the Low Band)
    std::array<FilterChain, 2> lowBandFilters;
    
    // HighPass at MinFrequency (Start of Mid Band)
    std::array<FilterChain, 2> midBandLowFilters;
    
    // LowPass at MaxFrequency (End of Mid Band)
    std::array<FilterChain, 2> midBandHighFilters;

    // HighPass at MaxFrequency (For the High Band)
    std::array<FilterChain, 2> highBandFilters;

    // Updates internal coefficients
    void updateAttackAndRelease();
    void updateFilters();

    // Visualizer stuff (FFT)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 2048;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int overlap = fftSize - hopSize;

    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::vector<float> fftInputBuffer;
    int fftInputBufferPos = 0;
    std::vector<float> fftData;

    // Storage for visualizer data
    juce::CriticalSection dataMutex;
    std::vector<juce::Point<float>> currentSpectrumData;

    void processVisualizerFrame();

public:
    BandPassCompressorProcessor();

    void prepare(const double sRate, int samplesPerBlock);

    void setThreshold(float thresholdInDB);
    void setRatio(float ratioValue);
    void setAttack(float attackInMS);
    void setRelease(float releaseInMS);
    void setMinFrequency(float freqHz);
    void setMaxFrequency(float freqHz);
    void setListen(bool shouldListen);

    void process(juce::AudioBuffer<float>& buffer);

    // Monitoring
    std::atomic<float> currentOutputLevelDb { -100.0f };

    // Visualizer Data Getter
    std::vector<juce::Point<float>> getSpectrumData();
};
