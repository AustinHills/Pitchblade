//Written by Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>
#include <atomic>

class AutoGainProcessor
{
private:
    // User parameters
    float targetThroughputDB = -6.0f; // Target output level in dB
    float thresholdDB = -40.0f;       // Below this input level, no gain is applied (or unity gain)
    float attackTimeMs = 20.0f;
    float releaseTimeMs = 200.0f;

    // Internal state
    float currentGain = 1.0f; // Linear gain multiplier
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    double sampleRate = 44100.0;

    void updateAttackAndRelease();

public:
    AutoGainProcessor();

    void prepare(const double sRate);

    // Setters
    void setTargetThroughput(float targetInDB);
    void setThreshold(float thresholdInDB);
    void setAttack(float attackInMs);
    void setRelease(float releaseInMs);

    void process(juce::AudioBuffer<float>& buffer);

    std::atomic<float> currentOutputLevelDb {-100.0f};
    std::atomic<float> currentGainReductionDb {0.0f}; // For potential visualization/metering
};
