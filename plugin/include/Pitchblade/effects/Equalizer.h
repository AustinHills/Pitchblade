#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>

/*
==============================================================================
    Equalizer 
    - provides a simple 3-band voice EQ (low shelf, mid peak, high shelf).
    - Parameters are thread-safe atomics updated from the UI; audio thread applies
    smoothed gains and per-channel IIR filters. 
    - Exposes prepare/reset/process plus getters for UI/visualizers.

    Author: Huda Noor
==============================================================================
*/

class Equalizer
{
 public:
    Equalizer() = default;

    // Prepare DSP state and allocate filters for the current session format
    void prepare(double sampleRate, int maxBlockSize, int numChannels);

    // Clear internal filter state
    void reset();

// ============ param setters (safe from GUI thread)============

    // Set low-shelf cutoff in Hz (clamped to 20–1000)
    void setLowFreq(float hz);

    // Set low-shelf gain in dB (clamped to -24..24)
    void setLowGainDb(float dB);

    // Set mid-peak center frequency in Hz (clamped to 200–6000)
    void setMidFreq(float hz);
    
    // Set mid-peak gain in dB (clamped to -24..24)
    void setMidGainDb(float dB);

    // Set high-shelf cutoff in Hz (clamped to 1000–18000)
    void setHighFreq(float hz);
    // Set high-shelf gain in dB (clamped to -24..24)
    void setHighGainDb(float dB);

    // process in-place
    // Apply 3-band EQ to the provided buffer
    void processBlock(juce::AudioBuffer<float>& buffer) noexcept;

    // getters for UI
    float getLowFreq() const noexcept { return lowFreqHz;  }
    float getLowGainDb()const noexcept { return lowGainDb;  }
    float getMidFreq()  const noexcept { return midFreqHz;  }
    float getMidGainDb()const noexcept { return midGainDb;  }
    float getHighFreq() const noexcept { return highFreqHz; }
    float getHighGainDb() const noexcept { return highGainDb; }

private:
    void updateFilters();

    double sr = 44100.0;    // sample rate used for coefficient calc
    int channels = 2;       // max channels allocated
    bool isPrepared = false; // set after prepare() allocates filters

    // knobs
    std::atomic<float> lowFreqHz  { 200.0f }; // low-shelf cutoff
    std::atomic<float> lowGainDb  {0.0f };    // low-shelf gain in dB

    std::atomic<float> midFreqHz  { 1000.0f }; // mid-peak center
    std::atomic<float> midGainDb  { 0.0f };    // mid-peak gain in dB
    const float midQ = 1.0f; // fixed Q for mid band

    std::atomic<float> highFreqHz { 4000.0f }; // high-shelf cutoff
    std::atomic<float> highGainDb {0.0f };     // high-shelf gain in dB

    // smoothing for gain changes (applied on audio thread)
    float smoothingTimeSeconds = 0.02f; // 20 ms smoothing window
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowGainSmooth;  // smoothed low gain (dB)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> midGainSmooth;  // smoothed mid gain (dB)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highGainSmooth; // smoothed high gain (dB)

    // one ProcessorDuplicator per channel per band
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoeff = juce::dsp::IIR::Coefficients<float>;
    using IIRProc= juce::dsp::ProcessorDuplicator<IIRFilter, IIRCoeff>;

    struct Band
    {
        std::vector<IIRProc> filters; // per-channel filter instances
    };

    Band lowBand, midBand, highBand; // storage for each EQ band

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Equalizer)
};
