//Written by Austin Hills

#include "Pitchblade/effects/DeEsserProcessor.h"

DeEsserProcessor::DeEsserProcessor()
    : forwardFFT(fftOrder),
        window(fftSize,juce::dsp::WindowingFunction<float>::hann)
{
    //Initialize buffers
    fftInputBuffer.resize(fftSize,0.0f);
    fftData.resize(fftSize * 2,0.0f);

    //Visualizer stuff
    currentSpectrumData.resize(fftSize / 2 + 1);
}

void DeEsserProcessor::prepare(double sRate, int samplesPerBlock){
    sampleRate = sRate;
    updateAttackAndRelease();
    updateFilter();

    //Reset envelopes
    envelope = 0.0f;
    rippleEnvelope = 0.0f;

    //Prepare the IIR filters with the process spec
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 2;

    for(auto& filter : sidechainFilters){
        filter.prepare(spec);
        filter.reset();
    }

    //Visualizer related
    std::fill(fftInputBuffer.begin(),fftInputBuffer.end(),0.0f);
    std::fill(fftData.begin(),fftData.end(),0.0f);
    fftInputBufferPos = 0;
    currentSpectrumData.resize(fftSize / 2 + 1);
}

//Setters for user controlled parameters
void DeEsserProcessor::setThreshold(float thresholdDB){
    threshold=thresholdDB;
}

void DeEsserProcessor::setRatio(float ratioValue){
    ratio=ratioValue;
}

void DeEsserProcessor::setAttack(float attackMs){
    attackTime=attackMs;
    updateAttackAndRelease();
}

void DeEsserProcessor::setRelease(float releaseMs){
    releaseTime=releaseMs;
    updateAttackAndRelease();
}

void DeEsserProcessor::setFrequency(float frequencyInHz){
    frequency = frequencyInHz;
    updateFilter();
}

//Calculates the smoothing coefficients for the envelope
void DeEsserProcessor::updateAttackAndRelease(){
    //For TC-15 and 17, I updated the attack to be more aggressive
    attackCoeff = exp(-7.0f / (0.001f * attackTime * sampleRate + 0.0000001f));
    releaseCoeff = exp(-1.0f / (0.001f * releaseTime * sampleRate + 0.0000001f));
}

//Calculates the coefficients for the sidechain band-pass filter
void DeEsserProcessor::updateFilter(){
    //Create band-pass filter coefficients centered around the target sibilant frequency
    // The Q value of 1.0f is a reasonable starting point, but it might be worthwhile to allow the user to control
    auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate,frequency,1.0f);

    //Assign the new coefficients to both stereo filters
    for(auto& filter : sidechainFilters){
        filter.coefficients = coefficients;
    }
}

//Main processing loop
void DeEsserProcessor::process(juce::AudioBuffer<float>& buffer){
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    //TC-15 and TC-17
    //Fixed release coefficient for ripple smoother
    const float rippleReleaseCoeff = std::exp(-1.0f / (0.001f * 10.0f * sampleRate));

    for(int i = 0;i < numSamples; i++){
        //Find loudest sibilant sample across all channels
        float sidechainSample = 0.0f;
        float monoInputSample = 0.0f; //Needed for visualizer
        for(int j = 0; j < numChannels; j++){
            //Get the unfiltered audio sample
            float originalSample = buffer.getSample(j,i);
            monoInputSample += originalSample;

            //Apply the sidechain filter to isolate sibilant frequencies
            //This does not affect the main audio
            float filteredSample = sidechainFilters[j].processSample(originalSample);

            //Find the peak
            sidechainSample = std::max(sidechainSample, std::abs(filteredSample));
        }

        //Average mono sample
        if(numChannels > 0){
            monoInputSample /= (float)numChannels;
        }

        //TC-15 and TC-17
        //Converts the sine wave into a stable peak value
        if (sidechainSample > rippleEnvelope) {
            rippleEnvelope = sidechainSample;
        } else {
            rippleEnvelope = rippleReleaseCoeff * rippleEnvelope + (1.0f - rippleReleaseCoeff) * sidechainSample;
        }

        //Envelope detection. The envelope tracks the loudness of the sibilance
        //TC-15 and TC-17
        //Changed this from sidechainSample to rippleEnvelope, since it's more stable
        if(rippleEnvelope > envelope){
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * rippleEnvelope;
        }else{
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * rippleEnvelope;
        }

        //Gain calculation (identical to compressor)
        float gainReductionDB = 0.0f;
        float envelopeDB = juce::Decibels::gainToDecibels(envelope,-100.0f);

        if(envelopeDB > threshold){
            gainReductionDB = (envelopeDB - threshold) * (1.0f - (1.0f / ratio));
        }

        float gainMultiplier = juce::Decibels::decibelsToGain(-gainReductionDB);

        //Apply gain
        for(int j = 0; j < numChannels; j++){
            float* channelData = buffer.getWritePointer(j);
            channelData[i] *= gainMultiplier;
        }

        //Apply the gain to the mono sample as well
        float processedMonoSample = monoInputSample * gainMultiplier;

        //Visualizer buffering
        //Overlap add input for visualizer
        fftInputBuffer[fftInputBufferPos++] = processedMonoSample;

        //Process frame
        if(fftInputBufferPos == fftSize){
            processVisualizerFrame();

            //Shift input buffer
            std::memmove(fftInputBuffer.data(),fftInputBuffer.data() + hopSize,overlap * sizeof(float));
            //Clear the end of the buffer
            std::fill(fftInputBuffer.data() + overlap,fftInputBuffer.data() + fftSize, 0.0f);
            //Reset the write pointer
            fftInputBufferPos = overlap;
        }

    }

}

//New visualizer functions
void DeEsserProcessor::processVisualizerFrame(){
    juce::ScopedNoDenormals noDenormals;
    //Window the input buffer
    std::copy(fftInputBuffer.begin(),fftInputBuffer.end(),fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(),fftSize);

    //Clear imaginary part
    std::fill(fftData.data() + fftSize,fftData.data() + fftSize * 2, 0.0f);

    //Perform forward FFT
    forwardFFT.performRealOnlyForwardTransform(fftData.data());

    const int numBins = fftSize / 2 + 1;
    std::vector<juce::Point<float>> spectrumSnapshot(numBins);

    // Handle first and last bins
    {
        float magnitude0 = fftData[0];
        float magnitude1024 = fftData[1];
        spectrumSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(magnitude0,-100.0f));
        spectrumSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude1024,-100.0f));
    }

    //Cycling through
    for(int i = 1; i < (numBins-1); i++){
        float real = fftData[i * 2];
        float imag = fftData[i * 2 + 1];
        float magnitude = std::sqrt(real * real + imag * imag);
        
        spectrumSnapshot[i].setXY((float)i * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude, -100.0f));
    }

    //Apply smoothing
    const int smoothingAmount = 3;
    std::vector<juce::Point<float>> smoothedSpectrum(numBins);
    for (int i = 0; i < numBins; ++i)
    {
        float spectrumSum = 0.0f;
        int numPoints = 0;
        for (int j = -smoothingAmount; j <= smoothingAmount; ++j)
        {
            int index = i + j;
            if (index >= 0 && index < numBins)
            {
                spectrumSum += spectrumSnapshot[index].getY();
                numPoints++;
            }
        }
        smoothedSpectrum[i].setXY(spectrumSnapshot[i].getX(), spectrumSum / (float)numPoints);
    }

    //Lock the mutex and swap the data
    {
        juce::ScopedLock lock(dataMutex);
        currentSpectrumData = std::move(smoothedSpectrum);
    }
}

//Getter for visualizer data
std::vector<juce::Point<float>> DeEsserProcessor::getSpectrumData()
{
    juce::ScopedLock lock(dataMutex);
    return currentSpectrumData;
}