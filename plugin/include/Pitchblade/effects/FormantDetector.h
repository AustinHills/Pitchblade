#pragma once
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include <JuceHeader.h>

/*
  FormantDetector class
  --------------------
  Detects dominant resonances (formants) in an audio signal in real-time.
  Uses FFT to find spectral peaks and can provide frequencies in Hz.
  
  Author: Huda Noor
*/
class FormantDetector
{
public:
    // Constructor: fftOrder determines FFT size (fftSize = 2^fftOrder)
    FormantDetector(int fftOrder = 11); // default: 2048-point FFT

    // Prepare the detector for a given sample rate
    void prepare(double sampleRateIn);

    // Process a block of audio and update formants
    void processBlock(const juce::AudioBuffer<float>& buffer);

    // Get detected formants as FFT bin indices
    std::vector<float> getFormants() const;

    // Get detected formants in Hertz (frequency)
    std::vector<float> getFormantFrequencies() const;

private:
    int fftOrder;                 // log2 of FFT size
    int fftSize;                  // actual FFT size
    juce::dsp::FFT fft;           // JUCE FFT object

    std::vector<float> window;    // Hann window for framing
    std::vector<float> fftData;   // Buffer for FFT input/output
    std::vector<float> formants;  // Detected formant bins

    double sampleRate = 44100.0;  // Sample rate used for frequency conversion

    // Internal helper: compute FFT of audio block
    void computeFFT(const juce::AudioBuffer<float>& buffer);

    // Internal helper: find spectral peaks in FFT data
    void findFormantPeaks();

};
