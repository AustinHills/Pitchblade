// Written by Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include <atomic>

class SaturationProcessor
{
private:
    // Drive refers to the input gain applied before the saturation curve, effectively controlling the amount of distortion.
    // Defined in decibels.
    float drive = 0.0f;
    
    // Mix controls the balance between the dry (unprocessed) and wet (saturated) signal.
    // Range: 0.0 (Dry) to 1.0 (Wet).
    float mix = 1.0f;

    // The sample rate is stored for potential future use (e.g. oversampling), though not strictly needed for basic tanh.
    double sampleRate = 44100.0;

public:
    // Constructor
    SaturationProcessor();

    // Called before processing to prepare with the current sample rate
    void prepare(const double sRate);

    // Setters for parameters
    void setDrive(float driveInDB);
    void setMix(float mixValue);

    // Processes the input audio buffer to apply saturation
    void process(juce::AudioBuffer<float>& buffer);

    // Store the latest output level in dB for the visualizer
    std::atomic<float> currentOutputLevelDb {-100.0f};

    std::atomic<float> priorOutputLevelDb {-100.0f};
};
