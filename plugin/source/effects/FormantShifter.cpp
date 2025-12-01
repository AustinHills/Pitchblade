#include "Pitchblade/effects/FormantShifter.h"

// Map UI [-50..+50] -> formant ratio [0.8..1.25]
// ratio > 1.0 = brighter (formants shift up)
// ratio < 1.0 = darker (formants shift down)
float FormantShifter::amountToRatio (float amount)
{
    // Map UI range to RubberBand formant ratio clamp
    float t = juce::jlimit (-1.0f, 1.0f, amount / 50.0f); // -1..1
    return juce::jlimit (0.8f, 1.25f, 1.0f + 0.25f * t);
}


void FormantShifter::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    // Allocate RubberBand stretcher and buffers sized to host constraints
    sr  = sampleRate;
    nCh = juce::jlimit (1, numChannels, numChannels);

    using RB = RubberBand::RubberBandStretcher;

    // EngineFiner required for setFormantScale()
    int options =
        RB::OptionProcessRealTime |
        RB::OptionPitchHighQuality |
        RB::OptionFormantPreserved |
        RB::OptionEngineFiner;

    stretcher = std::make_unique<RB> (
        (size_t) sr,
        (size_t) nCh,
        options,
        1.0,  // timeRatio
        1.0   // pitchScale
    );

    // RubberBand internal latency, in samples
    latencySamples = stretcher->getLatency();

    // FIFO: big enough to hold latency + a few blocks
    fifoSize = maxBlockSize * 16;
    fifo.setSize (nCh, fifoSize);
    fifo.clear();
    fifoWrite = fifoRead = fifoFill = 0;

    // Temp buffer for retrieve chunks (no alloc in processBlock)
    tempCapacity = maxBlockSize * 8;
    temp.setSize (nCh, tempCapacity);
    temp.clear();

    // Neutral formant
    setShiftAmount (0.0f);
}

void FormantShifter::reset()
{
    // Flush RubberBand internal state and our FIFO
    if (stretcher)
        stretcher->reset();

    fifo.clear();
    fifoWrite = fifoRead = fifoFill = 0;
}

void FormantShifter::setShiftAmount (float amount)
{
    // Clamp UI amount and push updated formant scale into RubberBand
    shiftAmount = juce::jlimit (-50.0f, 50.0f, amount);
    formantRatio = amountToRatio (shiftAmount);

    if (stretcher)
    {
        // Keep pitch fixed, move only formants
        stretcher->setPitchScale (1.0);
        stretcher->setFormantScale (formantRatio);
    }
}

void FormantShifter::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    // Stream block through RubberBand and align variable-latency output via FIFO
    juce::ScopedNoDenormals noDenormals;

    if (! stretcher)
        return;

    const int numSamples = buffer.getNumSamples();
    const int chans = juce::jmin (nCh, buffer.getNumChannels());

    if (numSamples <= 0 || chans <= 0)
        return;

    //Feed input block into RubberBand
    {
        float* inPtrs[8] {}; // adjust to support >8 channels
        for (int c = 0; c < chans; ++c)
            inPtrs[c] = buffer.getWritePointer (c);

        stretcher->process (inPtrs, numSamples, false);
    }

    // Retrieve whatever RubberBand has produced
    int avail = stretcher->available();
    while (avail > 0)
    {
        const int chunk = std::min (avail, tempCapacity);

        float* outPtrs[8] {};
        for (int c = 0; c < chans; ++c)
            outPtrs[c] = temp.getWritePointer (c);

        stretcher->retrieve (outPtrs, chunk);

        // write chunk into FIFO
        juce::AudioBuffer<float> view (temp.getArrayOfWritePointers(), chans, chunk);
        writeToFifo (view, chunk);

        avail -= chunk;
    }

    //Read exactly numSamples from FIFO into output buffer (zeros if not enough yet)
    readFromFifo (buffer, numSamples);

    // Clear any extra channels
    for (int c = chans; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

//============================================================
// FIFO helpers
//============================================================

void FormantShifter::writeToFifo (const juce::AudioBuffer<float>& src, int numSamples)
{
    // Push chunked data into ring buffer, clamping overflows
    const int ch = juce::jmin (nCh, src.getNumChannels());
    int remaining = numSamples;
    int pos = 0;

    // If we somehow overflow FIFO, we’ll drop oldest by clamping fill
    if (numSamples > fifoSize)
        remaining = fifoSize;

    while (remaining > 0)
    {
        const int spaceToEnd = fifoSize - fifoWrite;
        const int chunk = std::min (remaining, spaceToEnd);

        for (int c = 0; c < ch; ++c)
        {
            const float* srcPtr = src.getReadPointer (c);
            float* dstPtr = fifo.getWritePointer (c);
            std::memcpy (dstPtr + fifoWrite,
                         srcPtr + pos,
                         (size_t) chunk * sizeof (float));
        }

        fifoWrite = (fifoWrite + chunk) % fifoSize;
        pos += chunk;
        remaining -= chunk;
        fifoFill = std::min (fifoFill + chunk, fifoSize);
    }
}

void FormantShifter::readFromFifo (juce::AudioBuffer<float>& dst, int numSamples)
{
    // Pop exactly numSamples from FIFO, zero-filling if underflowing
    const int ch = juce::jmin (nCh, dst.getNumChannels());
    int remaining = numSamples;
    int pos = 0;

    // If we don't have enough yet (startup), output zeros for the missing part
    int toRead = std::min (numSamples, fifoFill);

    while (toRead > 0)
    {
        const int spaceToEnd = fifoSize - fifoRead;
        const int chunk = std::min (toRead, spaceToEnd);

        for (int c = 0; c < ch; ++c)
        {
            float* dstPtr = dst.getWritePointer (c);
            const float* srcPtr = fifo.getReadPointer (c);
            std::memcpy (dstPtr + pos,
                         srcPtr + fifoRead,
                         (size_t) chunk * sizeof (float));
        }

        fifoRead = (fifoRead + chunk) % fifoSize;
        pos += chunk;
        remaining -= chunk;
        toRead -= chunk;
        fifoFill -= chunk;
    }

    // Zero any remainder if FIFO underfilled
    if (remaining > 0)
        for (int c = 0; c < ch; ++c)
            juce::FloatVectorOperations::clear (dst.getWritePointer (c) + pos, remaining);
}
