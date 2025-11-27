// Written by Austin Hills

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>

//This is needed to pass the real-time audio level to the UI for the graph
#include <atomic>

//Defining the class that handles the compressor logic
class CompressorProcessor
{
private:
    //threshold refers to the level at which the compression will start. This is defined by the user
    float thresholdDB = 0.0f;
    //ratio refers to the amount of gain reduction by a ratio. For example, if the ratio is 4:1, then for every 4 dB the signal goes over the threshold, the output will only be allowed to rise by 1 dB. This is defined by the user
    float ratio = 1.0f;
    //attack time controls how quickly the compressor starts reducing the volume after the signal crosses the threshold. It is measured in milliseconds, and it is defined by the user.
    float attackTime = 10.0f;
    //release time controls how quickly the compressor stops reducing the volume after the signal falls below the threshold. It is measured in milliseconds, and it is defined by the user.
    float releaseTime = 100.0f;

    //The following two variables are internal parameters that determine the speed at which the compressor's envelope state changes
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    //The envelope is the current state of the compressor. Unlike the noise gate, it does not control the audio directly.
    //Its role is more to track the amplitude of the loudest channel in a given sample, which is then used to calculate
    //the amount to reduce the gain by.
    float envelope = 0.0f;

    //The sample rate will help to convert the unit in milliseconds to something that translates more closely to the audio signal
    double sampleRate = 44100.0;

    //Updates the attack and release coefficients based on user settings and sample rate
    void updateAttackAndRelease();

public:
    //Constructor
    CompressorProcessor();

    // Called before processing to prepare the compressor with the current sample rate
    void prepare(const double sRate);

    //Setters for parameters
    void setThreshold(float thresholdInDB);
    void setRatio(float ratioValue);
    void setAttack(float attackInMS);
    void setRelease(float releaseInMS);

    //processes the input audio buffer to apply compression
    void process(juce::AudioBuffer<float>& buffer);

    //Store the latest output level in dB for the visualizer
    std::atomic<float> currentOutputLevelDb {-100.0f};

    std::atomic<float> priorOutputLevelDb {-100.0f};

};