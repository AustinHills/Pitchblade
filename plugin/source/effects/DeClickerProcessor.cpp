//Austin Hills

#include "Pitchblade/effects/DeClickerProcessor.h"

DeClickerProcessor::DeClickerProcessor() :
    forwardFFT(fftOrder),
    window(fftSize, juce::dsp::WindowingFunction<float>::hann, false)
{
}

void DeClickerProcessor::prepare(double sRate, int numChannels) {
    sampleRate = sRate;
    
    // Resize channel states
    channels.resize(numChannels);

    for (auto& ch : channels) {
        ch.inputBuffer.assign(fftSize, 0.0f);
        ch.outputBuffer.assign(fftSize, 0.0f);
        ch.fftData.assign(fftSize * 2, 0.0f);
        ch.inputBufferPos = 0;
        ch.outputBufferPos = 0;
        ch.spectralHistory.clear();
        
        // Pre-fill history to avoid empty checks or divide by zero
        // We'll just push empty vectors, they'll be populated as we go
        // Actually, let's keep it empty and handle it in processFrame
    }

    // Initialize visualizer vectors
    visualizerInputData.resize(fftSize / 2 + 1);
    visualizerOutputData.resize(fftSize / 2 + 1);
}

void DeClickerProcessor::setHistoryLength(int length) {
    historyLength = juce::jlimit(1, 50, length);
    // We don't necessarily need to clear history here, just let the deque shrink/grow naturally in the loop
}

void DeClickerProcessor::setSensitivity(float sens) {
    sensitivity = sens;
}

void DeClickerProcessor::process(juce::AudioBuffer<float>& buffer) {
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Ensure we have enough channel states (in case channel count changes dynamically)
    if (channels.size() != numChannels) {
        prepare(sampleRate, numChannels);
    }

    // Process each channel independently
    for (int ch = 0; ch < numChannels; ++ch) {
        auto& channelState = channels[ch];
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            float inputSample = channelData[i];

            // Push to input buffer
            channelState.inputBuffer[channelState.inputBufferPos++] = inputSample;

            // Read from output buffer
            float outputSample = channelState.outputBuffer[channelState.outputBufferPos++];
            channelData[i] = outputSample; // Write back to buffer

            // If input buffer is full, process frame
            if (channelState.inputBufferPos == fftSize) {
                // Shift output buffer (Overlap-Add logic)
                std::memmove(channelState.outputBuffer.data(), channelState.outputBuffer.data() + hopSize, overlap * sizeof(float));
                std::fill(channelState.outputBuffer.data() + overlap, channelState.outputBuffer.data() + fftSize, 0.0f);
                channelState.outputBufferPos = 0;

                processFrame(channelState);

                // Shift input buffer
                std::memmove(channelState.inputBuffer.data(), channelState.inputBuffer.data() + hopSize, overlap * sizeof(float));
                std::fill(channelState.inputBuffer.data() + overlap, channelState.inputBuffer.data() + fftSize, 0.0f);
                channelState.inputBufferPos = overlap;
            }
        }
    }

    // After processing all samples for this block, update visualizer data
    // We'll average the latest spectrum from all channels
    if (numChannels > 0) {
        const int numBins = fftSize / 2 + 1;
        std::vector<juce::Point<float>> avgInput(numBins);
        std::vector<juce::Point<float>> avgOutput(numBins);

        // Sum up
        for (const auto& ch : channels) {
            if (ch.currentSpectrum.size() != numBins || ch.processedSpectrum.size() != numBins) continue;
            
            for (int i = 0; i < numBins; ++i) {
                // simple X-axis generation
                float freq = (float)i * sampleRate / (float)fftSize;
                
                // Add DB values? Or add linear then convert? 
                // Visualizer expects DB usually. Let's average the DB values for visual smoothness.
                // Or better: Average Magnitude then convert to DB.
                // ch.currentSpectrum stores Magnitude. 
                
                // We'll store the accumulated MAGNITUDE in the Y slot for now
                avgInput[i].setX(freq);
                avgInput[i].setY(avgInput[i].getY() + ch.currentSpectrum[i]);

                avgOutput[i].setX(freq);
                avgOutput[i].setY(avgOutput[i].getY() + ch.processedSpectrum[i]);
            }
        }

        // Divide and Convert
        for (int i = 0; i < numBins; ++i) {
            float magIn = avgInput[i].getY() / (float)numChannels;
            float magOut = avgOutput[i].getY() / (float)numChannels;

            avgInput[i].setY(juce::Decibels::gainToDecibels(magIn, -100.0f));
            avgOutput[i].setY(juce::Decibels::gainToDecibels(magOut, -100.0f));
        }

        // Swap to shared storage
        {
            juce::ScopedLock lock(dataMutex);
            visualizerInputData = avgInput;
            visualizerOutputData = avgOutput;
        }
    }
}

void DeClickerProcessor::processFrame(ChannelState& channel) {
    // 1. Prepare FFT Data
    std::copy(channel.inputBuffer.begin(), channel.inputBuffer.end(), channel.fftData.begin());
    window.multiplyWithWindowingTable(channel.fftData.data(), fftSize);
    
    // Clear imaginary part for real-only transform
    // Note: juce::dsp::FFT uses packed format for real-only, but the size required is 2*fftSize for the perform function?
    // Wait, performRealOnlyForwardTransform takes a buffer of size 2*fftSize where the first fftSize are input time-domain samples.
    // The output is (fftSize/2 + 1) complex numbers.
    // The existing DeNoiser implementation clears the second half:
    // std::fill(fftData.data() + fftSize,fftData.data() + fftSize * 2, 0.0f);
    std::fill(channel.fftData.data() + fftSize, channel.fftData.data() + fftSize * 2, 0.0f);

    forwardFFT.performRealOnlyForwardTransform(channel.fftData.data());

    // 2. Compute Magnitudes and Phases
    const int numBins = fftSize / 2 + 1;
    
    // Resize visualizer buffers if needed (first run)
    if (channel.currentSpectrum.size() != numBins) channel.currentSpectrum.resize(numBins);
    if (channel.processedSpectrum.size() != numBins) channel.processedSpectrum.resize(numBins);

    // Save Magnitudes
    std::vector<float> magnitudes(numBins);
    std::vector<float> phases(numBins);

    // 3. Algorithm: Look-Ahead Queue & Processing
    
    // Store RAW FFT Data (Real+Imag interleaved) in the buffer to preserve Phase
    std::vector<float> currentFrameRaw(fftSize * 2);
    std::copy(channel.fftData.begin(), channel.fftData.end(), currentFrameRaw.begin());
    channel.lookAheadBuffer.push_back(currentFrameRaw);
    
    // Manage Latency
    if (channel.lookAheadBuffer.size() <= lookAheadDepth) {
        // Latency period: Output silence (or passed zeroes) to maintain sync
        std::fill(channel.fftData.begin(), channel.fftData.end(), 0.0f);
    } 
    else {
        // Pop the "oldest" frame -> This is the frame we will PROCESS now
        std::vector<float> processingRaw = channel.lookAheadBuffer.front();
        channel.lookAheadBuffer.pop_front();
        
        // Load into fftData scratch buffer
        std::copy(processingRaw.begin(), processingRaw.end(), channel.fftData.begin());
        
        // Re-Compute Magnitudes & Phases for the Processing Frame
        // (Yes, redundant calc, but safer than storing huge structs)
        // Note: processingRaw has DC at [0], Nyquist at [1]
        
        float mag0 = std::abs(channel.fftData[0]);
        float magNyq = std::abs(channel.fftData[1]);
        
        magnitudes[0] = mag0;
        magnitudes[numBins - 1] = magNyq;
        phases[0] = 0.0f;
        phases[numBins - 1] = 0.0f;
        
        // Complex bins
        for (int i = 1; i < numBins - 1; ++i) {
            float real = channel.fftData[i * 2];
            float imag = channel.fftData[i * 2 + 1];
            magnitudes[i] = std::sqrt(real * real + imag * imag);
            phases[i] = std::atan2(imag, real);
        }
        
        // Update Visualizer Input (This shows what is being processed *now*, slightly delayed)
        for(int i=0; i<numBins; ++i) channel.currentSpectrum[i] = magnitudes[i];


        // --- CORE DETECTION LOGIC ---
        
        // 1. Calculate History Average (Past)
        std::vector<float> avgPast(numBins, 0.0f);
        if (!channel.spectralHistory.empty()) {
            for (const auto& pastFrame : channel.spectralHistory) {
                juce::FloatVectorOperations::add(avgPast.data(), pastFrame.data(), numBins);
            }
            juce::FloatVectorOperations::multiply(avgPast.data(), 1.0f / (float)channel.spectralHistory.size(), numBins);
        }

        // 2. Calculate Future Average (Buffer peek)
        // The deque contains [NextFrame, NextNextFrame, ...]
        std::vector<float> avgFuture(numBins, 0.0f);
        int futureCount = 0;
        
        // Peek at the *frames remaining in the buffer* (The Future)
        // We need to compute magnitudes for them on the fly
        for (const auto& futureRaw : channel.lookAheadBuffer) {
             // Quick magnitude estimation (DC/Nyq check skipped for speed, just iterate all as complex? No unsafe.)
             // Just duplicate logic for safety
             float fMag0 = std::abs(futureRaw[0]);
             // float fMagNyq = std::abs(futureRaw[1]); // Not strictly 100% correct if packed differently but consistent
             avgFuture[0] += fMag0;
             // We can skip Nyquist for averaging, it's rarely voice fundamental
             
             for (int i = 1; i < numBins - 1; ++i) {
                 float r = futureRaw[i * 2];
                 float im = futureRaw[i * 2 + 1];
                 avgFuture[i] += std::sqrt(r * r + im * im);
             }
             futureCount++;
        }
        
        if (futureCount > 0) {
            juce::FloatVectorOperations::multiply(avgFuture.data(), 1.0f / (float)futureCount, numBins);
        }


        // 3. Thresholds & Correction
        // Detect Click: Spikes above PAST but NOT above FUTURE (if future is loud, it's an onset)
        // Actually: If (Current > Past * Thresh) AND (Current > Future * SustainThresh)?
        // Or: (Current > Past*Thresh) AND (absolute future is quiet?)
        
        // Refined Logic from Plan:
        // Click = Louder than Past AND Louder than Future.
        // Onset = Louder than Past BUT Future is similar/louder.
        
        float baseThreshMult = juce::jmap(sensitivity, 0.0f, 1.0f, 20.0f, 1.5f);
        
        for (int i = 0; i < numBins; ++i) {
            float currentMag = magnitudes[i];
            float pastVal = avgPast[i];
            float futureVal = avgFuture[i]; // Look-Ahead Sustain
            
            // Frequency Weighting (Low Freq Protection)
            // Frequencies below ~2000Hz get a multiplier
            // Bin index -> Freq: i * SampleRate / FFTSize
            // 2000Hz approx bin: 2000 * 2048 / 44100 ~= 92
            
            float freqWeight = (i < 92) ? 3.0f : 1.0f; 
            
            float clickThreshold = pastVal * baseThreshMult * freqWeight;
            
            // Silence Gate
            if (pastVal < 0.0001f) clickThreshold = 0.001f; // higher floor

            bool isSpike = currentMag > clickThreshold;
            
            // Sustain Verification (The Look-Ahead Check)
            // If future is also loud (sustained), then it's NOT a click.
            // Check if Future is at least 50% of Current? 
            bool isSustained = futureVal > (currentMag * 0.5f);
            
            if (isSpike && !isSustained) {
                // IT IS A CLICK (Transient)
                
                // Ceiling Clamp: Limit to the threshold (plus headroom) instead of replacing with average
                // This preserves energy and avoids "holes"
                magnitudes[i] = clickThreshold; 
            }
        }
        
        // Update History with the *Processed* magnitude (Decoupling)
        // Maintain history size
        while (channel.spectralHistory.size() >= historyLength) {
             channel.spectralHistory.pop_front();
        }
        channel.spectralHistory.push_back(magnitudes);
        
        // Update Visualizer Output
        for(int i=0; i<numBins; ++i) channel.processedSpectrum[i] = magnitudes[i];
        
        // Reconstruct from Processed Magnitudes + Original Phase
        channel.fftData[0] = magnitudes[0];
        channel.fftData[1] = magnitudes[numBins - 1]; // Nyquist
        
        for (int i = 1; i < numBins - 1; ++i) {
            float m = magnitudes[i];
            float p = phases[i];
            channel.fftData[i * 2] = m * std::cos(p);
            channel.fftData[i * 2 + 1] = m * std::sin(p);
        }
    }

    
    // Window again (overlap-add requirement)
    window.multiplyWithWindowingTable(channel.fftData.data(), fftSize);

    // Normalize (Overlap-add gain correction + Window gain)
    // Hanning window adds up to 1.5x gain with 50% overlap? Or 2/3 scaling needed?
    // DeNoiser used: multiply(..., 1.0f/1.5f, ...)
    juce::FloatVectorOperations::multiply(channel.fftData.data(), 1.0f / 1.5f, fftSize);

    // Accumulate to Output Buffer
    // Note: outputBuffer logic in 'process' shifts the buffer. 
    // Here we just add the current frame's contribution.
    for (int i = 0; i < fftSize; ++i) {
        channel.outputBuffer[i] += channel.fftData[i];
    }
}

std::vector<juce::Point<float>> DeClickerProcessor::getInputSpectrumData() {
    juce::ScopedLock lock(dataMutex);
    return visualizerInputData;
}

std::vector<juce::Point<float>> DeClickerProcessor::getProcessedSpectrumData() {
    juce::ScopedLock lock(dataMutex);
    return visualizerOutputData;
}
