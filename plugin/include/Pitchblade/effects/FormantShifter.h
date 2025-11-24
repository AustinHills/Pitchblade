#pragma once
#include <JuceHeader.h>
#include "rubberband/RubberBandStretcher.h" // TEST BUG FIX: corrected relative path so tests can include RubberBandStretcher

/*
==============================================================================
    FormantShifter – Rubber Band Implementation
    uses the Rubber Band Library for real-time formant
    shifting while preserving pitch. 
    - Pitch scale is fixed at 1.0 (so that there is no actual pitch change).
    - Formant scale is controlled via setFormantScale(ratio):
          ratio > 1.0 shifts formants up (brighter / more nasal)
          ratio < 1.0 shifts formants down (darker / deeper)
    Rubber Band handles all spectral analysis, resynthesis, and latency
    internally, giving high-quality, low-latency formant
    control suitable for real-time plugin use.

    Author: Huda Noor
==============================================================================
*/

class FormantShifter
{
public:
    FormantShifter() = default;
    ~FormantShifter() noexcept = default;

    // Call in prepareToPlay
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    // Call on transport reset / seek, or when SR/channels change
    void reset();

    // amount in [-50 .. +50], 0 = neutral
    // negative -> darker/deeper, positive -> brighter/chipmunky
    void setShiftAmount (float amount);

    // Main audio processing. In-place.
    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    // For global latency accounting
    int getLatencySamples() const noexcept { return latencySamples; }
    float getShiftAmount() const noexcept { return shiftAmount; }   // TEST BUG FIX: expose clamped amount for non-invasive testing
    float getFormantRatio() const noexcept { return formantRatio; } // TEST BUG FIX: expose mapped ratio for non-invasive testing

private:
    using RB = RubberBand::RubberBandStretcher;

    std::unique_ptr<RB> stretcher;

    double sr = 48000.0;
    int nCh = 1;
    float shiftAmount  = 0.0f;  // [-50..50]
    float formantRatio = 1.0f;

    int latencySamples = 0;

    // FIFO for aligning RubberBand’s variable output to host block size
    juce::AudioBuffer<float> fifo;
    int fifoSize  = 0;
    int fifoWrite = 0;
    int fifoRead  = 0;
    int fifoFill  = 0; // how many valid samples are in FIFO

    // Temporary buffer used when retrieving from RubberBand
    juce::AudioBuffer<float> temp;
    int tempCapacity = 0; // max frames per retrieve chunk

    static float amountToRatio (float amount);

    void writeToFifo (const juce::AudioBuffer<float>& src, int numSamples);
    void readFromFifo (juce::AudioBuffer<float>& dst, int numSamples);
};
