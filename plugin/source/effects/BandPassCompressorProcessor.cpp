// Written by Austin Hills

#include "Pitchblade/effects/BandPassCompressorProcessor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

BandPassCompressorProcessor::BandPassCompressorProcessor()
{
    // Initialize buffers
    fftInputBuffer.resize(fftSize, 0.0f);
    fftData.resize(fftSize * 2, 0.0f);
    currentSpectrumData.resize(fftSize / 2 + 1);
}

void BandPassCompressorProcessor::prepare(const double sRate, int samplesPerBlock)
{
    sampleRate = sRate;
    updateAttackAndRelease();

    // Prepare filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; // We process channels individually in the loop

    for (int i = 0; i < 2; ++i)
    {
        lowBandFilters[i].prepare(spec);
        midBandLowFilters[i].prepare(spec);
        midBandHighFilters[i].prepare(spec);
        highBandFilters[i].prepare(spec);
        
        lowBandFilters[i].reset();
        midBandLowFilters[i].reset();
        midBandHighFilters[i].reset();
        highBandFilters[i].reset();
    }

    // Visualizer reset
    std::fill(fftInputBuffer.begin(), fftInputBuffer.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    fftInputBufferPos = 0;
    currentSpectrumData.resize(fftSize / 2 + 1);

    updateFilters();
    envelope = 0.0f;
}

void BandPassCompressorProcessor::setThreshold(float thresholdInDB)
{
    thresholdDB = thresholdInDB;
}

void BandPassCompressorProcessor::setRatio(float ratioValue)
{
    ratio = ratioValue;
}

void BandPassCompressorProcessor::setAttack(float attackInMS)
{
    attackTime = attackInMS;
    updateAttackAndRelease();
}

void BandPassCompressorProcessor::setRelease(float releaseInMS)
{
    releaseTime = releaseInMS;
    updateAttackAndRelease();
}

void BandPassCompressorProcessor::setMinFrequency(float freqHz)
{
    minFrequency = freqHz;
    updateFilters();
}

void BandPassCompressorProcessor::setMaxFrequency(float freqHz)
{
    maxFrequency = freqHz;
    updateFilters();
}

void BandPassCompressorProcessor::setListen(bool shouldListen)
{
    listenMode = shouldListen;
}

void BandPassCompressorProcessor::updateAttackAndRelease()
{
    // Formula derived from CompressorProcessor/DeEsserProcessor
    // Preventing division by zero with small epsilon
    attackCoeff = std::exp(-1.0f / (0.001f * attackTime * sampleRate + 0.00001f));
    releaseCoeff = std::exp(-1.0f / (0.001f * releaseTime * sampleRate + 0.00001f));
}

void BandPassCompressorProcessor::updateFilters()
{
    // Ensure min < max to avoid weird behavior
    float actualMin = std::min(minFrequency, maxFrequency - 1.0f);
    float actualMax = std::max(maxFrequency, minFrequency + 1.0f);
    
    // We use Butterworth 2nd order (12dB/oct) for splits.
    // Ideally, for a perfect crossover, we'd use Linkwitz-Riley 4th order, but standard IIR is a good start.

    auto lowPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, actualMin);
    auto highPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, actualMax);
    
    auto midLowPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, actualMin);
    auto midHighPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, actualMax);

    for (int i = 0; i < 2; ++i)
    {
        // Apply coeffs to both filters in the chain to create 4th order (Cascaded)
        *lowBandFilters[i].get<0>().coefficients = *lowPassCoeffs;
        *lowBandFilters[i].get<1>().coefficients = *lowPassCoeffs;
        
        *highBandFilters[i].get<0>().coefficients = *highPassCoeffs;
        *highBandFilters[i].get<1>().coefficients = *highPassCoeffs;
        
        *midBandLowFilters[i].get<0>().coefficients = *midLowPassCoeffs;
        *midBandLowFilters[i].get<1>().coefficients = *midLowPassCoeffs;
        
        *midBandHighFilters[i].get<0>().coefficients = *midHighPassCoeffs;
        *midBandHighFilters[i].get<1>().coefficients = *midHighPassCoeffs;
    }
}

void BandPassCompressorProcessor::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float maxLevel = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo processing variables
        float rawMidSum = 0.0f; // For envelope detection
        
        // We need to process each channel to get the bands
        // Then we compress the MID band based on the loudest MID content (or average)
        
        // To handle stereo linking correctly, we usually detect max envelope across channels
        
        // Temporary storage for band samples for this time step
        // We can't easily store all bands in temporary buffers without looping twice or allocating.
        // Since we process sample-by-sample, we can use stack variables.
        
        float lowSample[2] = {0.0f, 0.0f};
        float midSample[2] = {0.0f, 0.0f};
        float highSample[2] = {0.0f, 0.0f};

        float envelopeInput = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float in = buffer.getSample(ch, i);
            
            // Apply filtering
            // Note: processSample updates the filter state, so order matters if cascading.
            // But here the filters are parallel/independent in logic, but sequential in code.
            
            // Path 1: Low Band (< Min)
        // lowSample[ch] = lowBandFilters[ch].processSample(in);
        // ProcessorChain uses process(context), but for single sample loop we can use a helper or just context
        // But ProcessorChain doesn't have processSample(). We must use context.
        // Actually, for IIR filters, we can just call processSample on each manually or wrap it.
        // Let's use get<0>().processSample(get<1>().processSample(in))
        
        float low = lowBandFilters[ch].get<0>().processSample(lowBandFilters[ch].get<1>().processSample(in));
        lowSample[ch] = low;
        
        // Path 2: Mid Band (Min <-> Max)
        // HighPass(Min) -> LowPass(Max)
        // Filter Order: Chain(HP) -> Chain(LP)
        float midTmp = midBandLowFilters[ch].get<0>().processSample(midBandLowFilters[ch].get<1>().processSample(in));
        midSample[ch] = midBandHighFilters[ch].get<0>().processSample(midBandHighFilters[ch].get<1>().processSample(midTmp));
        
        // Path 3: High Band (> Max)
        highSample[ch] = highBandFilters[ch].get<0>().processSample(highBandFilters[ch].get<1>().processSample(in));
            
            // Track max magnitude for envelope
            float absMid = std::abs(midSample[ch]);
            if (absMid > envelopeInput)
                envelopeInput = absMid;
        }

        // --- Dynamics Processing (Sidechain = Mid Band) ---
        
        if (envelopeInput > envelope)
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * envelopeInput;
        else
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * envelopeInput;

        float envelopeDB = juce::Decibels::gainToDecibels(envelope, -100.0f);
        float gainReductionDB = 0.0f;

        if (envelopeDB > thresholdDB)
        {
            gainReductionDB = (envelopeDB - thresholdDB) * (1.0f - 1.0f / ratio);
        }

        float gain = juce::Decibels::decibelsToGain(-gainReductionDB);
        
        // --- Output Reconstruction ---
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float processedMid = midSample[ch] * gain;
            float out = 0.0f;

            if (listenMode)
            {
                // User wants to hear the band being processed
                out = processedMid; 
            }
            else
            {
                // Recombine
                out = lowSample[ch] + processedMid + highSample[ch];
            }

            buffer.setSample(ch, i, out);
            
            float absOut = std::abs(out);
            if (absOut > maxLevel)
                maxLevel = absOut;
        }

        // Visualizer buffering (Mono sum)
        float monoSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            monoSample += buffer.getSample(ch, i);
        if (numChannels > 0) monoSample /= (float)numChannels;

        fftInputBuffer[fftInputBufferPos++] = monoSample;
        if (fftInputBufferPos == fftSize)
        {
            processVisualizerFrame();
            std::memmove(fftInputBuffer.data(), fftInputBuffer.data() + hopSize, overlap * sizeof(float));
            std::fill(fftInputBuffer.data() + overlap, fftInputBuffer.data() + fftSize, 0.0f);
            fftInputBufferPos = overlap;
        }
    }

    // Update level for visualizer
    currentOutputLevelDb = juce::Decibels::gainToDecibels(maxLevel, -100.0f);
}

void BandPassCompressorProcessor::processVisualizerFrame()
{
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

    {
        float magnitude0 = fftData[0];
        float magnitude1024 = fftData[1];
        spectrumSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(magnitude0,-100.0f));
        spectrumSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude1024,-100.0f));
    }

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

    {
        juce::ScopedLock lock(dataMutex);
        currentSpectrumData = std::move(smoothedSpectrum);
    }
}

std::vector<juce::Point<float>> BandPassCompressorProcessor::getSpectrumData()
{
    juce::ScopedLock lock(dataMutex);
    return currentSpectrumData;
}
