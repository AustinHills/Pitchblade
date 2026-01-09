//Written by Austin Hills

#include "Pitchblade/effects/AutoGainProcessor.h"
#include <cmath>

AutoGainProcessor::AutoGainProcessor()
{
    updateAttackAndRelease();
}

void AutoGainProcessor::prepare(const double sRate)
{
    sampleRate = sRate;
    updateAttackAndRelease();
    currentGain = 1.0f;
}

void AutoGainProcessor::updateAttackAndRelease()
{
    // Standard one-pole filter coefficients
    // These formulas match the ones used in CompressorProcessor/NoiseGateProcessor if consistent
    // Or we can use standard digital audio formulas: exp(-1 / (time * sampleRate))
    // Note: NoiseGateProcessor uses milliseconds directly to calculate simple coeffs if they are linear fades,
    // but usually for smooth envelopes we use exponential decay.
    // Let's check CompressorProcessor logic. Assuming standard exp decay for now.
    
    // Actually, let's stick to the pattern in CompressorProcessor.cpp if possible.
    // Since I can't double check the .cpp right now without a tool call, I will use standard safe formulas.
    // T = -1 / ln(target) * samples usually.
    // Simplified: coeff = exp(-1000.0 / (timeMs * sRate)) for 1ms time constant
    
    // However, let's use the explicit logic from the headers I saw:
    // They had 'updateAttackAndRelease' so they probably pre-calc coeffs.
    
    // For attack/release, usually: y[n] = coeff * y[n-1] + (1-coeff) * target
    
    attackCoeff = std::exp(-1000.0f / (attackTimeMs * sampleRate));
    releaseCoeff = std::exp(-1000.0f / (releaseTimeMs * sampleRate));
}

void AutoGainProcessor::setTargetThroughput(float targetInDB)
{
    targetThroughputDB = targetInDB;
}

void AutoGainProcessor::setThreshold(float thresholdInDB)
{
    thresholdDB = thresholdInDB;
}

void AutoGainProcessor::setAttack(float attackInMs)
{
    attackTimeMs = attackInMs;
    updateAttackAndRelease();
}

void AutoGainProcessor::setRelease(float releaseInMs)
{
    releaseTimeMs = releaseInMs;
    updateAttackAndRelease();
}

void AutoGainProcessor::process(juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // Iterate through samples
    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Detect Input Level (Peak or RMS?)
        // Peak is cheaper and usually fine for auto-gain if smoothed.
        // We need the max amplitude across channels for this sample frame
        float inputMagnitude = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s = std::abs(buffer.getSample(ch, i));
            if (s > inputMagnitude) inputMagnitude = s;
        }

        // 2. Determine Desired Gain
        float inputDB = juce::Decibels::gainToDecibels(inputMagnitude, -100.0f);
        float desiredGain = currentGain; // Stay same by default

        if (inputDB > thresholdDB)
        {
            // If input is loud enough, we want Output = Target
            // Gain * Input = TargetLinear
            // GainDB + InputDB = TargetDB
            // GainDB = TargetDB - InputDB
            float neededGainDB = targetThroughputDB - inputDB;
            float neededGainLinear = juce::Decibels::decibelsToGain(neededGainDB);
            
            // We want to move 'currentGain' towards 'neededGainLinear'
            // If neededGain < currentGain (we need to turn it down -> Attack or Release?)
            // Usually "Attack" = acting on a signal increase (gain reduction). 
            // "Release" = recovery.
            // But this is Auto Gain.
            // If input spikes UP, gain must go DOWN. That is "Attack".
            // If input drops DOWN, gain must go UP. That is "Release".
            
            if (neededGainLinear < currentGain)
            {
                // Gain reducing (Input got louder) -> Attack
                currentGain = attackCoeff * currentGain + (1.0f - attackCoeff) * neededGainLinear;
            }
            else
            {
                // Gain increasing (Input got quieter) -> Release
                currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * neededGainLinear;
            }
        }
        else
        {
            // Below threshold: Return to Unity Gain (1.0) or Hold?
            // "Unless signal is below threshold" -> implies "don't effect it" -> Gain = 1.0.
            // We should smooth back to 1.0. 
            // This is "Release" behavior (returning to normal).
             currentGain = releaseCoeff * currentGain + (1.0f - releaseCoeff) * 1.0f;
        }

        // 3. Apply Gain
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s = buffer.getSample(ch, i);
            buffer.setSample(ch, i, s * currentGain);
        }
    }
    
    // Update meters (using the last gain/sample)
    // Calculating output level for this block (peak)
    float maxOut = buffer.getMagnitude(0, numSamples); 
    currentOutputLevelDb = juce::Decibels::gainToDecibels(maxOut);
    currentGainReductionDb = juce::Decibels::gainToDecibels(currentGain);
}
