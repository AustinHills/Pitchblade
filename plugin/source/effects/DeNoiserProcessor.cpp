//Austin Hills

#include "Pitchblade/effects/DeNoiserProcessor.h"

//Constructor
DeNoiserProcessor::DeNoiserProcessor() :
    forwardFFT(fftOrder),
    //Adding the below will fix the errors relating to added gain in the denoiser. Going to do fully when unit testing
    window(fftSize,juce::dsp::WindowingFunction<float>::hann, false)
{
    //Initialize buffers
    inputBuffer.resize(fftSize,0.0f);
    outputBuffer.resize(fftSize,0.0f);
    fftData.resize(fftSize * 2,0.0f);
    noiseProfile.resize(fftSize / 2 + 1,0.0f);

    //Visualizer stuff
    currentSpectrumData.resize(fftSize / 2 + 1);
    noiseProfileData.resize(fftSize / 2 + 1);
}

void DeNoiserProcessor::prepare(const double sRate){
    sampleRate = sRate;

    //reset buffers and state
    std::fill(inputBuffer.begin(),inputBuffer.end(),0.0f);
    std::fill(outputBuffer.begin(),outputBuffer.end(),0.0f);
    std::fill(noiseProfile.begin(),noiseProfile.end(),0.0f);
    std::fill(fftData.begin(),fftData.end(),0.0f);

    inputBufferPos = 0;
    outputBufferPos = 0;
    samplesProcessed = 0;
    noiseProfileSamples = 0;

    //Visualizer stuff
    currentSpectrumData.resize(fftSize / 2 + 1);
    noiseProfileData.resize(fftSize / 2 + 1);
}

//Setters for user controlled parameters
void DeNoiserProcessor::setReduction(float reduction){
    reductionAmount = reduction;
}

void DeNoiserProcessor::setLearning(bool learning){
    if(learning){
        //If starting to learn, reset the noise profile
        if(!isLearning){
            std::fill(noiseProfile.begin(),noiseProfile.end(),0.0f);
            noiseProfileSamples = 0;
        }
        isLearning = true;
    }else{
        //If stopping learning, average the collected profile
        if(isLearning && (noiseProfileSamples > 0)){
            for(int i = 0; i < noiseProfile.size(); i++){
                float& bin = noiseProfile[i];
                bin /= (float)noiseProfileSamples;
            }
        }

        //Attempt to make the resulting sound less staticky
        //Before this, the noise profile has a lot of peaks, but it's a singular number for each bin
        //Noise, however, goes up and down. I'm thinking that might be the issue? Let's see
        //Trying to smooth it all out
        // std::vector<float> profileCopy = noiseProfile;

        // for(int i = 1;i < noiseProfile.size()-1;i++){
        //     float prevBin = profileCopy[i-1];
        //     float thisBin = profileCopy[i];
        //     float nextBin = profileCopy[i+1];

        //     noiseProfile[i] = (prevBin+thisBin+nextBin) / 3;
        // }

        isLearning = false;
    }
}

//Main processing loop
void DeNoiserProcessor::process(juce::AudioBuffer<float>& buffer){
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for(int i = 0; i < numSamples; i++){
        //Get mono mix sample
        float inputSample = 0.0f;
        for(int j = 0; j < numChannels; j++){
            inputSample += buffer.getSample(j,i);
        }
        if(numChannels > 0){
            inputSample /= (float)numChannels;
        }

        //Overlap add input
        inputBuffer[inputBufferPos++] = inputSample;

        //Get output sample
        float outputSample = outputBuffer[outputBufferPos++];

        //Write output sample to all channels
        for(int j = 0; j < numChannels; j++){
            buffer.setSample(j,i,outputSample);
        }

        //Process frame
        //If the input buffer's position is equal to the size of the fft, then send it off for processing
        if(inputBufferPos == fftSize){
            //Moving output buffer logic up to see if it fixes the choppy audio. It's like gambling
            std::memmove(outputBuffer.data(),outputBuffer.data() + hopSize,overlap * sizeof(float));
            std::fill(outputBuffer.data() + overlap,outputBuffer.data() + fftSize,0.0f);
            outputBufferPos = 0;
            
            processFrame();

            //Shift input buffer to prepare for the next samples
            std::memmove(inputBuffer.data(),inputBuffer.data() + hopSize,overlap * sizeof(float));
            //Clear the end of the buffer
            std::fill(inputBuffer.data() + overlap,inputBuffer.data() + fftSize, 0.0f);
            //Reset the write pointer
            inputBufferPos = overlap;
        }

        //Moved this stuff up because there was choppy audio
        // //If the output buffer's position is equal to the size of the fft, then do the same as above but for the output, preparing the cleaned audio to be sent out
        // if(outputBufferPos == fftSize){
        //     //Shift output buffer
        //     std::memmove(outputBuffer.data(),outputBuffer.data() + hopSize,overlap * sizeof(float));
        //     std::fill(outputBuffer.data() + overlap,outputBuffer.data() + fftSize,0.0f);
        //     outputBufferPos = 0;
        // }
    }
}

//Processing of the frame, either for learning or cleaning data
void DeNoiserProcessor::processFrame(){
    juce::ScopedNoDenormals noDenormals; //I really hope this fixes the static
    //Window the input buffer
    std::copy(inputBuffer.begin(),inputBuffer.end(),fftData.begin());
    window.multiplyWithWindowingTable(fftData.data(),fftSize);

    //Clear imaginary part
    std::fill(fftData.data() + fftSize,fftData.data() + fftSize * 2, 0.0f);

    //Perform forward FFT
    forwardFFT.performRealOnlyForwardTransform(fftData.data());

    //Process magnitudes and phases so fftData contains complex numbers in packed format
    const int numBins = fftSize / 2 + 1;

    //Visualizer temp vectors to avoid locking mutex during processing
    std::vector<juce::Point<float>> spectrumSnapshot(numBins);
    std::vector<juce::Point<float>> noiseSnapshot(numBins);
    
    //Storing the value of isLearning within this context so that the next part happens faster
    bool learning = isLearning;

    //Was having crackling issues, and I thought that separating the special cases (first and last bins) would help
    //Using curly brackets to separate it so I can use variable names without worrying if they are repeated later on
    {
        float magnitude0 = fftData[0];
        float magnitude1024 = fftData[1];

        float noiseMag0 = noiseProfile[0];
        float noiseMag1024 = noiseProfile[numBins - 1];

        if(learning){
            noiseProfile[0] += magnitude0;
            noiseProfile[numBins - 1] += magnitude1024;
        }else{
            float reduction0 = noiseMag0 * (reductionAmount * POWER_MULTIPLIER);
            float reduction1024 = noiseMag1024 * (reductionAmount * POWER_MULTIPLIER);

            float reducedMag0 = std::max(0.0f,magnitude0-reduction0);
            float reducedMag1024 = std::max(0.0f,magnitude1024-reduction1024);

            float floor0 = magnitude0 * 0.001f;
            float floor1024 = magnitude1024 * 0.001f;
            
            fftData[0] = std::max(reducedMag0,floor0);
            fftData[1] = std::max(reducedMag1024,floor1024);
        }

        //Gathering stuff for visualizer
        spectrumSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(magnitude0,-100.0f));
        spectrumSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude1024,-100.0f));
        noiseSnapshot[0].setXY(20.0f,juce::Decibels::gainToDecibels(noiseMag0,-100.0f));
        noiseSnapshot[numBins-1].setXY((float)(numBins - 1) * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(noiseMag1024,-100.0f));
    }

    //Cycling through
    for(int i = 1; i < (numBins-1); i++){
        float real;
        float imag;
        //Unpacking parts for the ith frequency bins
        real = fftData[i * 2];
        imag = fftData[i * 2 + 1];

        //Magnitude is the strength of the frequency
        float magnitude = std::sqrt(real * real + imag * imag);
        //Phase is the timing if the frequency's wave (from what I understand). Messing with this will do bad things to the audio, so leave it unchanged
        float phase = std::atan2(imag, real);

        if(learning){
            //Accumulate info for noise profile
            noiseProfile[i] += magnitude;
        }else{
            //Spectral subtraction
            //This part is the actual denoising part that uses the noiseProfile and reductionAmount as keys to determine how much and what to reduce
            float noiseMag = noiseProfile[i];
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
        }

        //Reconstruct the frequencies
        //Calculate new real part
        fftData[i * 2] = magnitude * std::cos(phase);
        //Calculate new imag part
        fftData[i * 2 + 1] = magnitude * std::sin(phase);

        //Visualizer stuff
        spectrumSnapshot[i].setXY((float)i * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(magnitude, -100.0f));
        noiseSnapshot[i].setXY((float)i * sampleRate / (float)fftSize,juce::Decibels::gainToDecibels(noiseProfile[i],-100.0f));

    }

    //increment noiseProfileSamples if it is learning for use in averaging later on
    if(learning){
        noiseProfileSamples++;
    }

    //Performing the inverse FFT function! This is putting those pieces back together into an actual bit of audio!
    forwardFFT.performRealOnlyInverseTransform(fftData.data());

    //Apply the window to the processed audio sitting in fftData
    window.multiplyWithWindowingTable(fftData.data(),fftSize);

    //Normalize the audio so its level is correct
    juce::FloatVectorOperations::multiply(fftData.data(),1.0f/1.5f,fftSize);

    //Add the processed sample back to the output buffer
    for(int i = 0; i < fftSize; i++){
        outputBuffer[i] += fftData[i];
    }

    //The graph was too spikey, so I figured I'd apply some smoothing to make it more readable
    const int smoothingAmount = 3;

    //Stores smoothed stuff
    std::vector<juce::Point<float>> smoothedSpectrum(numBins);
    std::vector<juce::Point<float>> smoothedNoise(numBins);

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
                spectrumSum += spectrumSnapshot[index].getY();
                noiseSum += noiseSnapshot[index].getY();
                numPoints++;
            }
        }
        
        // Get the average and store it
        // We keep the original frequency (X) but use the new averaged amplitude (Y)
        smoothedSpectrum[i].setXY(spectrumSnapshot[i].getX(), spectrumSum / (float)numPoints);
        smoothedNoise[i].setXY(noiseSnapshot[i].getX(), noiseSum / (float)numPoints);
    }

    //Lock the mutex and swap the data
    {
        juce::ScopedLock lock(dataMutex);
        currentSpectrumData = std::move(smoothedSpectrum);
        noiseProfileData = std::move(smoothedNoise);
    }
}

//Visualizer getters
std::vector<juce::Point<float>> DeNoiserProcessor::getSpectrumData()
{
    juce::ScopedLock lock(dataMutex);
    return currentSpectrumData; // Returns a copy
}

std::vector<juce::Point<float>> DeNoiserProcessor::getNoiseProfileData()
{
    juce::ScopedLock lock(dataMutex);
    return noiseProfileData; // Returns a copy
}