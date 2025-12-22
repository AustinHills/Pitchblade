//Austin Hills

#include "Pitchblade/effects/AdaptiveDeNoiserProcessor.h"

//Constructor
AdaptiveDeNoiserProcessor::AdaptiveDeNoiserProcessor() :
    forwardFFT(fftOrder),
    //Adding the below will fix the errors relating to added gain in the denoiser. Going to do fully when unit testing
    window(fftSize,juce::dsp::WindowingFunction<float>::hann, false)
{
    //Visualizer stuff
    currentSpectrumData.resize(fftSize / 2 + 1);
    noiseProfileData.resize(fftSize / 2 + 1);
}

void AdaptiveDeNoiserProcessor::prepare(const double sRate){
    sampleRate = sRate;

    //reset buffers and state by clearing the channels vector
    channels.clear();

    //Visualizer stuff
    currentSpectrumData.resize(fftSize / 2 + 1);
    noiseProfileData.resize(fftSize / 2 + 1);
}

//Setters for user controlled parameters
void AdaptiveDeNoiserProcessor::setReduction(float reduction){
    reductionAmount = reduction;
}

//Main processing loop
void AdaptiveDeNoiserProcessor::process(juce::AudioBuffer<float>& buffer){
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Resize channels vector if necessary
    if (channels.size() != numChannels) {
        channels.resize(numChannels);
    }

    for (int channelIdx = 0; channelIdx < numChannels; ++channelIdx) {
        auto* channelData = buffer.getWritePointer(channelIdx);
        auto& state = channels[channelIdx];

        for (int i = 0; i < numSamples; ++i) {
            float inputSample = channelData[i];

            //Overlap add input
            state.inputBuffer[state.inputBufferPos++] = inputSample;

            //Get output sample
            float outputSample = state.outputBuffer[state.outputBufferPos++];

            //Write output sample
            channelData[i] = outputSample;

            //Process frame
            //If the input buffer's position is equal to the size of the fft, then send it off for processing
            if(state.inputBufferPos == fftSize){
                //Moving output buffer logic up to see if it fixes the choppy audio. It's like gambling
                std::memmove(state.outputBuffer.data(), state.outputBuffer.data() + hopSize, overlap * sizeof(float));
                std::fill(state.outputBuffer.data() + overlap, state.outputBuffer.data() + fftSize, 0.0f);
                state.outputBufferPos = 0;
                
                processFrame(state);

                //Shift input buffer to prepare for the next samples
                std::memmove(state.inputBuffer.data(), state.inputBuffer.data() + hopSize, overlap * sizeof(float));
                //Clear the end of the buffer
                std::fill(state.inputBuffer.data() + overlap, state.inputBuffer.data() + fftSize, 0.0f);
                //Reset the write pointer
                state.inputBufferPos = overlap;
            }
        }
    }

    // Update visualizer with average data
    updateVisualizer();
}

//Processing of the frame, either for learning or cleaning data
void AdaptiveDeNoiserProcessor::processFrame(ChannelState& state){
    juce::ScopedNoDenormals noDenormals; //I really hope this fixes the static
    //Window the input buffer
    std::copy(state.inputBuffer.begin(), state.inputBuffer.end(), state.fftData.begin());
    window.multiplyWithWindowingTable(state.fftData.data(), fftSize);

    //Clear imaginary part
    std::fill(state.fftData.data() + fftSize, state.fftData.data() + fftSize * 2, 0.0f);

    //Perform forward FFT
    forwardFFT.performRealOnlyForwardTransform(state.fftData.data());

    //Process magnitudes and phases so fftData contains complex numbers in packed format
    const int numBins = fftSize / 2 + 1;
    
    float frameAmplitude = 0.0f;
    for(float sample : state.inputBuffer){
        frameAmplitude += sample * sample;
    }

    frameAmplitude = std::sqrt(frameAmplitude / (float)state.inputBuffer.size());
    float frameDB = juce::Decibels::gainToDecibels(frameAmplitude,-100.0f);

    //If it's below the threshold, learn off of it
    bool learning = frameDB < threshold;

    //Was having crackling issues, and I thought that separating the special cases (first and last bins) would help
    //Using curly brackets to separate it so I can use variable names without worrying if they are repeated later on
    {
        float magnitude0 = state.fftData[0];
        float magnitude1024 = state.fftData[1];

        float noiseMag0 = state.noiseProfile[0];
        float noiseMag1024 = state.noiseProfile[numBins - 1];

        if(learning){
            state.noiseProfile[0] = (state.noiseProfile[0] * (1.0f - learningRate)) + (magnitude0 * learningRate);
            state.noiseProfile[numBins - 1] = (state.noiseProfile[numBins - 1] * (1.0f - learningRate)) + (magnitude1024 * learningRate);
        }
        float reduction0 = noiseMag0 * (reductionAmount * POWER_MULTIPLIER);
        float reduction1024 = noiseMag1024 * (reductionAmount * POWER_MULTIPLIER);

        float reducedMag0 = std::max(0.0f,magnitude0-reduction0);
        float reducedMag1024 = std::max(0.0f,magnitude1024-reduction1024);

        float floor0 = magnitude0 * 0.001f;
        float floor1024 = magnitude1024 * 0.001f;
        
        state.fftData[0] = std::max(reducedMag0,floor0);
        state.fftData[1] = std::max(reducedMag1024,floor1024);


        //Gathering stuff for visualizer (per channel)
        state.latestSpectrumSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(magnitude0,-100.0f));
        state.latestSpectrumSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude1024,-100.0f));
        state.latestNoiseSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(noiseMag0,-100.0f));
        state.latestNoiseSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(noiseMag1024,-100.0f));
    }

    //Cycling through
    for(int i = 1; i < (numBins-1); i++){
        float real;
        float imag;
        //Unpacking parts for the ith frequency bins
        real = state.fftData[i * 2];
        imag = state.fftData[i * 2 + 1];

        //Magnitude is the strength of the frequency
        float magnitude = std::sqrt(real * real + imag * imag);
        //Phase is the timing if the frequency's wave (from what I understand). Messing with this will do bad things to the audio, so leave it unchanged
        float phase = std::atan2(imag, real);

        if(learning){
            //Accumulate info for noise profile
            state.noiseProfile[i] = (state.noiseProfile[i] * (1.0f - learningRate)) + (magnitude * learningRate);
        }
        //Spectral subtraction
        //This part is the actual denoising part that uses the noiseProfile and reductionAmount as keys to determine how much and what to reduce
        float noiseMag = state.noiseProfile[i];
        //Calculates the amount to subtract from a given frequency based on the average magnitude during learning.
        //Multiplying allows the sweet spot to be close to 50% of the slider with the ability to subtract more if desired 
        float reduction = noiseMag * (reductionAmount * POWER_MULTIPLIER);

        //Make reduce the magnitude of the frequency using the reduction. If that results in a value below 0, just use 0 instead
        float reducedMag = std::max(0.0f, magnitude - reduction);

        //If the sound is complete silence, then don't completely make it empty. Give a little bit of noise (can be removed with noise gate)
        //This is to prevent artifacting in the sound 
        float floor = magnitude * 0.001f;

        //This is the final decision, and it chooses the greater value between the reducedMag and the floor value
        magnitude = std::max(reducedMag,floor);

        //Reconstruct the frequencies
        //Calculate new real part
        state.fftData[i * 2] = magnitude * std::cos(phase);
        //Calculate new imag part
        state.fftData[i * 2 + 1] = magnitude * std::sin(phase);

        //Visualizer stuff
        state.latestSpectrumSnapshot[i].setXY((float)i * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude, -100.0f));
        state.latestNoiseSnapshot[i].setXY((float)i * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(state.noiseProfile[i],-100.0f));

    }

    //Performing the inverse FFT function! This is putting those pieces back together into an actual bit of audio!
    forwardFFT.performRealOnlyInverseTransform(state.fftData.data());

    //Apply the window to the processed audio sitting in fftData
    window.multiplyWithWindowingTable(state.fftData.data(), fftSize);

    //Normalize the audio so its level is correct
    juce::FloatVectorOperations::multiply(state.fftData.data(), 1.0f/1.5f, fftSize);

    //Add the processed sample back to the output buffer
    for(int i = 0; i < fftSize; i++){
        state.outputBuffer[i] += state.fftData[i];
    }
}

void AdaptiveDeNoiserProcessor::updateVisualizer() {
    if (channels.empty()) return;

    const int numBins = fftSize / 2 + 1;
    const int numChannels = channels.size();
    
    //The graph was too spikey, so I figured I'd apply some smoothing to make it more readable
    const int smoothingAmount = 3;

    //Stores smoothed stuff
    std::vector<juce::Point<float>> smoothedSpectrum(numBins);
    std::vector<juce::Point<float>> smoothedNoise(numBins);

    // Averages data first
    std::vector<juce::Point<float>> avgSpectrum(numBins);
    std::vector<juce::Point<float>> avgNoise(numBins);

    for (int i = 0; i < numBins; ++i) {
        float sumSpecY = 0.0f;
        float sumNoiseY = 0.0f;
        float xVal = 0.0f; 

        for (const auto& channel : channels) {
            if (i < channel.latestSpectrumSnapshot.size()) {
                 sumSpecY += channel.latestSpectrumSnapshot[i].getY();
                 xVal = channel.latestSpectrumSnapshot[i].getX(); // Assume X is same for all
            }
            if (i < channel.latestNoiseSnapshot.size()) {
                 sumNoiseY += channel.latestNoiseSnapshot[i].getY();
            }
        }
        
        avgSpectrum[i].setXY(xVal, sumSpecY / (float)numChannels);
        avgNoise[i].setXY(xVal, sumNoiseY / (float)numChannels);
    }

    //Smoothing
    for (int i = 0; i < numBins; ++i)
    {
        float spectrumSum = 0.0f;
        float noiseSum = 0.0f;
        int numPoints = 0;

        // Create a moving average
        for (int j = -smoothingAmount; j <= smoothingAmount; ++j)
        {
            int index = i + j;
            if (index >= 0 && index < numBins)
            {
                spectrumSum += avgSpectrum[index].getY();
                noiseSum += avgNoise[index].getY();
                numPoints++;
            }
        }
        
        // Get the average and store it
        // We keep the original frequency (X) but use the new averaged amplitude (Y)
        smoothedSpectrum[i].setXY(avgSpectrum[i].getX(), spectrumSum / (float)numPoints);
        smoothedNoise[i].setXY(avgNoise[i].getX(), noiseSum / (float)numPoints);
    }

    //Lock the mutex and swap the data
    {
        juce::ScopedLock lock(dataMutex);
        currentSpectrumData = std::move(smoothedSpectrum);
        noiseProfileData = std::move(smoothedNoise);
    }
}

//Visualizer getters
std::vector<juce::Point<float>> AdaptiveDeNoiserProcessor::getSpectrumData()
{
    juce::ScopedLock lock(dataMutex);
    return currentSpectrumData; // Returns a copy
}

std::vector<juce::Point<float>> AdaptiveDeNoiserProcessor::getNoiseProfileData()
{
    juce::ScopedLock lock(dataMutex);
    return noiseProfileData; // Returns a copy
}

void AdaptiveDeNoiserProcessor::setThreshold(float thresholdDB){
    //If the threshold changes, reset the noise profile
    if(threshold != thresholdDB){
        threshold = thresholdDB;
        for(auto& channel : channels){
            std::fill(channel.noiseProfile.begin(), channel.noiseProfile.end(), 0.0f);
        }
    }
}
