// Written by Austin Hills

#include "Pitchblade/effects/SaturationProcessor.h"
#include <juce_core/juce_core.h>

// Constructor
SaturationProcessor::SaturationProcessor() {}

// Prepare the processor with the current sample rate
void SaturationProcessor::prepare(const double sRate){
    sampleRate = sRate;
}

// Setters for user controlled parameters
void SaturationProcessor::setDrive(float driveInDB){
    drive = driveInDB;
}

void SaturationProcessor::setMix(float mixValue){
    // Clamp mix between 0 and 1
    mix = juce::jlimit(0.0f, 1.0f, mixValue);
}

// The main processing loop
void SaturationProcessor::process(juce::AudioBuffer<float>& buffer){

    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Calculate linear gain from drive dB once per block (or per sample if smoothed, but block is standard here based on other files)
    float driveGain = juce::Decibels::decibelsToGain(drive);

    for(int i = 0; i < numSamples; i++){
        for(int j = 0; j < numChannels; j++){
            // Get original sample
            float in = buffer.getSample(j, i);

            // Apply Drive
            float driven = in * driveGain;

            // Apply Saturation (Soft Clipping with tanh)
            float wet = std::tanh(driven);

            // Apply Output Compensation
            // As drive increases, we scale down the result so purely linear signals (low level) return to unity,
            // and high signals are just saturated but not massively louder.
            // Using 1.0f / driveGain ensures that if 'in' was small, 'wet' would be approx 'driven', 
            // and 'driven / driveGain' returns us to roughly 'in'.
            // However, we want to allow SOME volume increase (perceived loudness), so often we don't compensate 100%.
            // But strict 1/driveGain is the cleanest 'Drive' implementation where 0dB = Unity.
            
            if (driveGain > 1.0f)
                wet /= driveGain;

            // Apply Mix (Linear Interpolation)
            float out = in * (1.0f - mix) + wet * mix;

            // Write back to buffer
            buffer.setSample(j, i, out);
        }
    }
}
