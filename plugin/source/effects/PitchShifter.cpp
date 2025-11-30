#include "Pitchblade/effects/PitchShifter.h"

/**
 * @brief Prepare block to initialize pitch shifter
 */
void PitchShifter::prepare(double sampleRate, int maxBlockSize){
    this->sampleRate = sampleRate;             //parameter: how many samples per cycle. 44100KHz default. Stay by that or 44800 for good results
    this->maxBlockSize = maxBlockSize;         //parameter: max size that will ever be passed to buffer

    // Options optimized for real time high quality processing
    RubberBand::RubberBandStretcher::Options options = 
        RubberBand::RubberBandStretcher::Option::OptionProcessRealTime|
        RubberBand::RubberBandStretcher::Option::OptionFormantPreserved|
        RubberBand::RubberBandStretcher::Option::OptionPitchHighConsistency;

    stretcher = std::make_unique<RubberBand::RubberBandStretcher> (sampleRate, 1, options, 1.0, 1.0);

    bufferSize = 4096;                  // Worth looking for smaller buffers to decrease lag.
    inputBuffer.setSize(1, bufferSize);
    outputBuffer.setSize(1, bufferSize);

    writeIn = readIn = writeOut = 0;
    pitchRatio.store(1.0f);
}

/**
 * @brief Update pitch atomic with new value
 * @param ratio value from 0.5 to 2.0 
 */
void PitchShifter::setPitchShiftRatio(float ratio){
    ratio = juce::jlimit(0.5f, 2.0f, ratio);    // Can move from half to twice
    pitchRatio.store(ratio);
}

/**
 * @brief Handle pitch shifting on an incoming buffer
 * @param buffer Juce audio buffer holding audio data with effects applied
 * @details  Accumulate samples to send to processor, process with rubber band,
 * and send final processed mono data to stereo channels
 */
void PitchShifter::processBlock(juce::AudioBuffer<float>& buffer){
    const int numSamples = buffer.getNumSamples();
    const float* input = buffer.getReadPointer(0);

    // Fill input buffer with new samples
    float* inPtr = inputBuffer.getWritePointer(0);
    float* outPtr = outputBuffer.getWritePointer(0);

    for(int i = 0; i < numSamples; ++i){
        inPtr[writeIn] = input[i];
        writeIn = (writeIn + 1) % bufferSize;
    }

    // Send to processor when enough samples are accumulated
    const int availableIn = getAvailableSamples();
    const int required = stretcher->getSamplesRequired();
    if(availableIn >= required && required > 0)
        processRubberBand(required);
    
    // Fill output buffer with processed samples
    float* output = buffer.getWritePointer(0);
    for(int i = 0; i < numSamples; ++i){
        //if there is data in the buffer
        if(readOut != writeOut){
            output[i] = outPtr[readOut];
            readOut = (readOut + 1) % bufferSize;
        }else{
            output[i] = 0.0f;
        }
    }

    // Copy mono data to left and right channels
    for(int channel = 1; channel < buffer.getNumChannels(); channel++){
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
    }
}

/**
 * @brief Handle pitch and time details for stretcher
 * @param required Minimum amount of input data threshold
 * @details Determine pitch and time ratio information 
 * and pass that information to pitch and time ratio
 */
void PitchShifter::processRubberBand(int required){
    const float* inPtr = inputBuffer.getReadPointer(0);
    float* outPtr = outputBuffer.getWritePointer(0);

    std::vector<float> inputData(required);
    for(int i = 0; i < required; ++i){
        inputData[i] = inPtr[(readIn + i) % bufferSize];
    }

    // Pass samples to stretcher
    const float* inPtrs[] = { inputData.data() }; // copy with direct access
    const float ratio = pitchRatio.load();  // load atomic
    //load pitch and time as inverse of each other
    stretcher->setPitchScale(ratio);
    stretcher->setTimeRatio(1.0 / ratio);

    stretcher->process(inPtrs, required, false);
    readIn = (readIn + required) % bufferSize;

    // Output processed samples
    const int available = stretcher->available();
    if(available <= 0) return;  // check if ready

    std::vector<float> outputData(available);
    float* outPtrs[] = { outputData.data() };
    const int retrieved = stretcher->retrieve(outPtrs, available); 
    const int clampedRetrieved = std::min(retrieved, bufferSize);

    for(int i = 0; i < clampedRetrieved; ++i){
        outPtr[writeOut] = outputData[i];
        writeOut = (writeOut + 1) % bufferSize;
    }
}

/**
 * @brief Getter for current amount of samples in buffer
 */
int PitchShifter::getAvailableSamples() const{
    return (writeIn - readIn + bufferSize) % bufferSize;
}