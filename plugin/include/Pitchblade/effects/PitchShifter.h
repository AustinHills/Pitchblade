/**
 * Author: Hayley Spellicy-Ryan
 * PitchShifter class
 * -------------------
 * Rubberband processor
 */

#pragma once
#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>

/**
 * Public interface class for testing
 */
class IPitchShifter{
public:
    virtual ~IPitchShifter() = default;
    virtual void prepare(double, int) = 0;
    virtual void setPitchShiftRatio(float) = 0;
    virtual void processBlock(juce::AudioBuffer<float>&) = 0;
};

class PitchShifter : public IPitchShifter{
    public:
        PitchShifter() = default;
        void prepare(double, int) override;
        void setPitchShiftRatio(float) override;
        float getPitchShiftRatio() { return pitchRatio.load(); }
        void processBlock(juce::AudioBuffer<float>&) override;
    private:
        void processRubberBand(int);
        int getAvailableSamples() const;

        double sampleRate;     // Typical sample rates at 44100 or 48000
        int maxBlockSize;
        std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
        int bufferSize;

        juce::AudioBuffer<float> inputBuffer; // Rubber Band stretches audio and thus changes the buffer size, hence two buffers
        juce::AudioBuffer<float> outputBuffer;

        //indexes
        int writeIn = 0;
        int writeOut = 0;
        int readIn = 0;
        int readOut = 0;

        std::atomic<float> pitchRatio { 1.0f }; // thread safe
};