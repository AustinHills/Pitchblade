#include "Pitchblade/effects/FormantDetector.h"
#include <algorithm>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

//Author Huda

FormantDetector::FormantDetector(int order)
    : fftOrder(order),
      fftSize(1 << fftOrder), // fftSize = 2^fftOrder
      fft(fftOrder),
      window(fftSize, 0.0f),
      fftData(2 * fftSize, 0.0f)
{
    // Pre-compute Hann window once to avoid recalculating per block
    // Initialize Hann window: smooths edges to reduce spectral leakage
    for (int i = 0; i < fftSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
}

void FormantDetector::prepare(double sampleRateIn)
{
    // Reset state when SR changes so bin->Hz mapping stays valid
    // Prepare internal buffers and set sample rate
    sampleRate = sampleRateIn;
    formants.clear();
}

void FormantDetector::processBlock(const juce::AudioBuffer<float>& buffer)
{
    // Gate silence and update formant candidates for the current block
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
    {
        formants.clear();
        return;
    }

    //Compute average RMS over all channels
    float totalRms = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        totalRms += buffer.getRMSLevel(ch, 0, numSamples);

    totalRms /= (float) numChannels;

    //Gate on level: treat very low-level signal as silence
    // Adjust this if needed; ~1e-3 is about -60 dBFS, 1e-4 is about -80 dBFS.
    constexpr float rmsSilenceThreshold = 1e-3f;

    if (totalRms < rmsSilenceThreshold)
    {
        formants.clear();
        return;
    }

    //Compute FFT on incoming audio block
    computeFFT(buffer);

    //Find spectral peaks representing formants
    findFormantPeaks();
}


void FormantDetector::computeFFT(const juce::AudioBuffer<float>& buffer)
{
    // Window and zero-pad before in-place real FFT
    // Copy first channel into fftData with windowing applied
    int numSamples = std::min(buffer.getNumSamples(), fftSize);
    auto* channelData = buffer.getReadPointer(0);

    for (int i = 0; i < numSamples; ++i)
        fftData[i] = channelData[i] * window[i];

    // Zero-pad if buffer is smaller than FFT size
    for (int i = numSamples; i < fftSize; ++i)
        fftData[i] = 0.0f;

    // Perform FFT (in-place, complex numbers interleaved as real/imag)
    fft.performRealOnlyForwardTransform(fftData.data());
}

void FormantDetector::findFormantPeaks()
{
    // Convert FFT bins to magnitudes and pick the strongest vocal-band peaks
    formants.clear();

    // Magnitude spectrum
    const int halfSize = fftSize / 2;
    std::vector<float> magnitude(halfSize);

    for (int i = 0; i < halfSize; ++i)
    {
        float real = fftData[2 * i];
        float imag = fftData[2 * i + 1];
        magnitude[i] = std::sqrt(real * real + imag * imag);
    }

    constexpr float fMinHz = 300.0f;
    constexpr float fMaxHz = 5000.0f;

    const int minBin = (int) std::ceil (fMinHz * fftSize / sampleRate);
    const int maxBin = (int) std::floor(fMaxHz * fftSize / sampleRate);

    if (minBin >= maxBin || maxBin >= halfSize)
        return;

    //Find the maximum magnitude in this band
    float maxMag = 0.0f;
    for (int i = minBin; i <= maxBin; ++i)
        maxMag = std::max(maxMag, magnitude[i]);

    // If everything is tiny, treat as silence / no formants
    constexpr float absMagFloor = 1e-6f;
    if (maxMag < absMagFloor)
        return;

    //Relative threshold: only keep peaks above some fraction of max
    constexpr float relativePeakThreshold = 0.30f;   // 30% of max
    const float peakThreshold = maxMag * relativePeakThreshold;

    //Collect local maxima that exceed the threshold
    std::vector<std::pair<float, int>> candidates; // (mag, bin)
    for (int i = minBin + 1; i < maxBin; ++i)
    {
        float mPrev = magnitude[i - 1];
        float mCurr = magnitude[i];
        float mNext = magnitude[i + 1];

        const bool isLocalMax = (mCurr > mPrev) && (mCurr > mNext);
        const bool isStrong   = (mCurr >= peakThreshold);

        if (isLocalMax && isStrong)
            candidates.emplace_back(mCurr, i);
    }

    if (candidates.empty())
        return;

    //Sort by magnitude (strongest first) and keep up to 3–4 formants
    std::sort(candidates.begin(), candidates.end(),
              [] (auto& a, auto& b) { return a.first > b.first; });

    const int maxFormants = 3; // or 4 maybe??
    for (int i = 0; i < (int)candidates.size() && i < maxFormants; ++i)
        formants.push_back((float) candidates[i].second);
}


std::vector<float> FormantDetector::getFormants() const
{
    // Return latest detected FFT bins; caller may map to Hz
    return formants;
}

std::vector<float> FormantDetector::getFormantFrequencies() const
{
    // Convert stored bins to Hz based on current sample rate
    std::vector<float> freqs;
    // Convert FFT bin indices to frequency in Hz
    for (auto bin : formants)
        freqs.push_back(bin * static_cast<float>(sampleRate) / static_cast<float>(fftSize));

    return freqs;
}

