// Written by Austin Hills

#include "Pitchblade/effects/CompressorProcessor.h"
#include <juce_core/juce_core.h>

//Constructor
CompressorProcessor::CompressorProcessor(){}

// Prepare the processor with the current sample rate
void CompressorProcessor::prepare(const double sRate){
    sampleRate = sRate;
    updateAttackAndRelease();
}

// Setters for user controlled parameters
// This one in particular does not convert the value from decibels like the noise gate did, as it will be useful to have it in dB later on
void CompressorProcessor::setThreshold(float thresholdInDB){
    thresholdDB = thresholdInDB;
}

// For the ratio, it is assumed that the ratio value will be to 1, meaning that no values below 1 are accepted because those will work in the opposite direction
void CompressorProcessor::setRatio(float ratioValue){
    if(ratioValue < 1.0f){
        ratio = 1.0f;
    }else{
        ratio = ratioValue;
    }
}

void CompressorProcessor::setAttack(float attackInMS){
    attackTime = attackInMS;
    updateAttackAndRelease();
}

void CompressorProcessor::setRelease(float releaseInMS){
    releaseTime = releaseInMS;
    updateAttackAndRelease();
}

// Calculates the smoothing coefficients for the envelope detector. See NoiseGateProcessor.cpp for a clearer explanation, as this reuses the code from there
void CompressorProcessor::updateAttackAndRelease(){
    attackCoeff = exp(-3.5f / (0.001f * attackTime * sampleRate + 0.0000001f));
    releaseCoeff = exp(-0.9f / (0.001f * releaseTime * sampleRate + 0.0000001f));
}

// The main processing loop. This operates very similarly to the noise gate
void CompressorProcessor::process(juce::AudioBuffer<float>& buffer){

    juce::ScopedNoDenormals noDenormals;

    // Gathering information on the buffer
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    //This function iterates through the samples, checking the channel with the maximum volume in the given sample
    //That value is stored and then used to determine what the state of the envelope should be
    for(int i = 0; i < numSamples; i++){
        float maxSample = 0.0f;
        for(int j = 0;j < numChannels; j++){
            maxSample = std::max(maxSample, abs(buffer.getSample(j, i)));
        }

        //Adjust the envelope with respect to the maximum volume
        //This uses the same calculation as the noise gate does. See that for a deeper explanation of the envelope's curve
        //The only difference is that it is multiplied by the maxSample rather than a targetEnvelope
        //The goal is to track the maxSample on a curve to then use it in calculations of gain to apply
        if(maxSample > envelope){
            //The envelope moves toward the louder sample
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * maxSample;
        }else{
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * maxSample;
        }

        //Calculating the gain multiplier based on the envelope value
        //Initializing
        float gainReductionDB = 0.0f;

        //converting the envelope decibels into gain. This uses -100 as a floor value, representing silence
        float envelopeDB = juce::Decibels::gainToDecibels(envelope,-100.0f);

        //If the envelope is above the threshold, calculate the gain reduction
        if(envelopeDB > thresholdDB){
            //This function utilizes the threshold and envelope values in decibels to see the difference
            //Then it utilizes the ratio defined by the user to calculate how much to reduce the gain by
            gainReductionDB = (envelopeDB - thresholdDB) * (1.0f - (1.0f / ratio));
        }

        //Convert the dB reduction into a linear gain multiplier
        //The gainReductionDB is negative because the sound needs to be reduced
        float gainMultiplier = juce::Decibels::decibelsToGain(-gainReductionDB);

        //Now, the gain is actually applied to all channels for the current sample
        for(int j = 0;j < numChannels; j++){
            float* channelData = buffer.getWritePointer(j);
            channelData[i] *= gainMultiplier;
        }
    }

}