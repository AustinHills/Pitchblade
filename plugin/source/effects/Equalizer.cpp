#include "Pitchblade/effects/Equalizer.h"

//Author: Huda Noor
// More robust version with unity gain bypass

void Equalizer::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    // Initialize smoothing, per-band filters, and coefficients for the given format
    sr = sampleRate;
    channels = juce::jmax(1, numChannels);
    isPrepared = true;

    // initialize smoothing filters
    lowGainSmooth.reset(sr, smoothingTimeSeconds);
    midGainSmooth.reset(sr, smoothingTimeSeconds);
    highGainSmooth.reset(sr, smoothingTimeSeconds);
    lowGainSmooth.setCurrentAndTargetValue(lowGainDb.load());
    midGainSmooth.setCurrentAndTargetValue(midGainDb.load());
    highGainSmooth.setCurrentAndTargetValue(highGainDb.load());

    // allocate filter states per band/channel
    auto makeBand = [this](Band& b)
    {
        b.filters.clear();
        b.filters.resize((size_t)channels);
        for (auto& f : b.filters)
           { f.reset();}
    };

    makeBand(lowBand);
    makeBand(midBand);
    makeBand(highBand);

    // prepare each per channel duplicator for single channel processing
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sr;
    spec.maximumBlockSize = (juce::uint32)maxBlockSize;
    spec.numChannels = 1; // each duplicator instance handles one channel

    for (auto& f : lowBand.filters)  f.prepare(spec);
    for (auto& f : midBand.filters)  f.prepare(spec);
    for (auto& f : highBand.filters) f.prepare(spec);

    updateFilters();
}

void Equalizer::reset()
{
    // Reset filter state but leave configuration intact
    for (auto& f : lowBand .filters) f.reset();
    for (auto& f : midBand .filters) f.reset();
    for (auto& f : highBand.filters) f.reset();
}

void Equalizer::setLowFreq(float hz)
{
    // Clamp and store low-shelf cutoff in Hz
    lowFreqHz = juce::jlimit(20.0f, 1000.0f, hz);
}

void Equalizer::setLowGainDb(float dB)
{
    // Clamp and store low-shelf gain in dB
    lowGainDb = juce::jlimit(-24.0f, 24.0f, dB);
}

void Equalizer::setMidFreq(float hz)
{
    // Clamp and store mid-band center frequency in Hz
    midFreqHz = juce::jlimit(200.0f, 6000.0f, hz);
}

void Equalizer::setMidGainDb(float dB)
{
    // Clamp and store mid-band gain in dB
    midGainDb = juce::jlimit(-24.0f, 24.0f, dB);
}

void Equalizer::setHighFreq(float hz)
{
    // Clamp and store high-shelf cutoff in Hz
    highFreqHz = juce::jlimit(1000.0f, 18000.0f, hz);
}

void Equalizer::setHighGainDb(float dB)
{
    // Clamp and store high-shelf gain in dB
    highGainDb = juce::jlimit(-24.0f, 24.0f, dB);
}

void Equalizer::updateFilters()
{
    // Recompute filter coefficients from current parameter values
    if (!isPrepared)
        return;

    float lowGain = lowGainDb.load();
    float midGain = midGainDb.load();
    float highGain = highGainDb.load();

    // Use shelving and peaking with linear gain factors (convert from dB)
    auto lowC = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sr, lowFreqHz.load(), midQ, juce::Decibels::decibelsToGain(lowGain));
    auto midC = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sr, midFreqHz.load(), midQ, juce::Decibels::decibelsToGain(midGain));
    auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sr, highFreqHz.load(), midQ, juce::Decibels::decibelsToGain(highGain));
    // Copy into existing state to avoid RT pointer swaps
    for (auto& f : lowBand.filters)
    {
        if (f.state) *f.state = *lowC; else f.state = lowC;
    }
    for (auto& f : midBand.filters)
    {
        if (f.state) *f.state = *midC; else f.state = midC;
    }
    for (auto& f : highBand.filters)
    {
        if (f.state) *f.state = *highC; else f.state = highC;
    }
}

void Equalizer::processBlock(juce::AudioBuffer<float>& buffer) noexcept
{
    // Smooth gain changes, update coefficients, and apply per-band filters
    if (!isPrepared || buffer.getNumChannels() == 0)
        return;

    const int nCh   = juce::jmin(channels, buffer.getNumChannels());
    const int nSmps = buffer.getNumSamples();

    // Update smoothing targets from GUI atomics and compute current values
    lowGainSmooth.setTargetValue(lowGainDb.load());
    midGainSmooth.setTargetValue(midGainDb.load());
    highGainSmooth.setTargetValue(highGainDb.load());
    const float curLowDb  = lowGainSmooth.getCurrentValue();
    const float curMidDb  = midGainSmooth.getCurrentValue();
    const float curHighDb = highGainSmooth.getCurrentValue();

    // Read current Hz directly (no smoothing to avoid warble)
    const float curLowHz  = lowFreqHz.load();
    const float curMidHz  = midFreqHz.load();
    const float curHighHz = highFreqHz.load();

    // Update coefficients at block rate using smoothed values
    {
        auto lowC = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
            sr, curLowHz, midQ, juce::Decibels::decibelsToGain(curLowDb));
        auto midC = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sr, curMidHz, midQ, juce::Decibels::decibelsToGain(curMidDb));
        auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sr, curHighHz, midQ, juce::Decibels::decibelsToGain(curHighDb));

        for (auto& f : lowBand .filters) { if (f.state) *f.state = *lowC; else f.state = lowC; }
        for (auto& f : midBand .filters) { if (f.state) *f.state = *midC; else f.state = midC; }
        for (auto& f : highBand.filters) { if (f.state) *f.state = *highC; else f.state = highC; }
    }

    // Check if all gains are essentially zero - if so, just pass through
    float lowG = std::abs(curLowDb);
    float midG = std::abs(curMidDb);
    float highG = std::abs(curHighDb);
    
    if (lowG < 0.01f && midG < 0.01f && highG < 0.01f)
    {
        // All gains near zero, pass through unchanged
        lowGainSmooth.skip(nSmps);
        midGainSmooth.skip(nSmps);
        highGainSmooth.skip(nSmps);
        // advance smoothing (no processing)
        lowGainSmooth.skip(nSmps);
        midGainSmooth.skip(nSmps);
        highGainSmooth.skip(nSmps);
        return;
    }

    juce::dsp::AudioBlock<float> block(buffer);

    // Apply each band's filter to each channel in series
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto channelBlock = block.getSingleChannelBlock((size_t)ch);
        
        // Only process if gain is not near zero
        if (lowG > 0.01f)
        {
            lowBand.filters[(size_t)ch].process(
                juce::dsp::ProcessContextReplacing<float>(channelBlock));
        }
        
        if (midG > 0.01f)
        {
            midBand.filters[(size_t)ch].process(
                juce::dsp::ProcessContextReplacing<float>(channelBlock));
        }
        
        if (highG > 0.01f)
        {
            highBand.filters[(size_t)ch].process(
                juce::dsp::ProcessContextReplacing<float>(channelBlock));
        }
    }

    // advance smoothing for this processed block
    lowGainSmooth.skip(nSmps);
    midGainSmooth.skip(nSmps);
    highGainSmooth.skip(nSmps);

    // clear any channels we didn't touch
    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, nSmps);
}
