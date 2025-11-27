// EQ response visualizer using FrequencyGraphVisualizer

#include "Pitchblade/ui/EqualizerVisualizer.h"
#include "Pitchblade/effects/Equalizer.h"

EqualizerVisualizer::EqualizerVisualizer(AudioPluginAudioProcessor& proc)
    : processor(proc)
{
    // Use displayMode 1 to draw a horizontal 0 dB line and a vertical band marker
    // FrequencyGraphVisualizer y-axis is fixed to [-100, 0] dB.
    graph = std::make_unique<FrequencyGraphVisualizer>(processor.apvts, 5, 1);
    addAndMakeVisible(*graph);

    // Add overlay that repaints y-axis labels to match knob range [-24, +24] dB
    yLabels = std::make_unique<YAxisLabelOverlay>();
    addAndMakeVisible(*yLabels);
    yLabels->setInterceptsMouseClicks(false, false);
    yLabels->setAlwaysOnTop(true);

    // Initial threshold aligned to 0 dB baseline after our display mapping
    // We map [-24..+24] dB to the full [-100..0] visual range, so 0 dB -> -50
    graph->setThreshold(processor.getEqualizer().getMidFreq(), -50.0f);

    // Update regularly; cost is low (rebuilding 300 points)
    // FrequencyGraphVisualizer handles its own repaint timer; we just refresh data
    startTimerHz(15);

    // Populate once immediately
    updateResponseCurve();
}

EqualizerVisualizer::~EqualizerVisualizer()
{
    stopTimer();
}

void EqualizerVisualizer::resized()
{
    if (graph)
        graph->setBounds(getLocalBounds());
    if (yLabels)
        yLabels->setBounds(getLocalBounds());
}

void EqualizerVisualizer::forceUpdateForTest()
{
    // Keep threshold in sync and rebuild response immediately.
    graph->setThreshold(processor.getEqualizer().getMidFreq(), -50.0f);
    updateResponseCurve();
}

std::vector<juce::Point<float>> EqualizerVisualizer::getLastResponsePoints() const
{
    const juce::ScopedLock sl(responseLock);
    return lastResponse;
}

void EqualizerVisualizer::timerCallback()
{
    // Keep the vertical threshold in sync with the mid frequency, baseline at -50 dB (mapped 0 dB)
    graph->setThreshold(processor.getEqualizer().getMidFreq(), -50.0f);
    updateResponseCurve();
}

void EqualizerVisualizer::buildLogFrequencies(std::vector<float>& freqs, int numPoints)
{
    freqs.clear();
    freqs.reserve((size_t)numPoints);
    const float fStart = 20.0f;
    const float fEnd= 20000.0f;
    const float logStart = std::log10(fStart);
    const float logEnd = std::log10(fEnd);
    for (int i = 0; i < numPoints; ++i)
    {
        float t = (numPoints <= 1 ? 0.0f : (float)i / (float)(numPoints - 1));
        float f = std::pow(10.0f, juce::jmap(t, logStart, logEnd));
        freqs.push_back(f);
    }
}

void EqualizerVisualizer::updateResponseCurve()
{
    auto& eq = processor.getEqualizer();

    // Get current parameters (thread-safe atomics)
    const float lowHz = eq.getLowFreq();
    const float midHz = eq.getMidFreq();
    const float highHz = eq.getHighFreq();
    const float lowDb = eq.getLowGainDb();
    const float midDb = eq.getMidGainDb();
    const float highDb = eq.getHighGainDb();

    // Sample rate may be 0 before prepareToPlay; default to 44100
    double sr = processor.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;

    // Build coefficients mirroring the DSP path (Q=1.0 to match Equalizer)
    const float Q = 1.0f;
    auto lowC = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, lowHz, Q, juce::Decibels::decibelsToGain(lowDb));
    auto midC = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, midHz, Q, juce::Decibels::decibelsToGain(midDb));
    auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, highHz, Q, juce::Decibels::decibelsToGain(highDb));

    // Generate response over log-spaced frequencies
    std::vector<float> freqs;
    buildLogFrequencies(freqs, 300);

    std::vector<juce::Point<float>> points;
    points.reserve(freqs.size());

    for (float f : freqs)
    {
        // Combine magnitudes in linear domain
        float mag = 1.0f;
        if (lowC) mag *= lowC ->getMagnitudeForFrequency(f, (float)sr);
        if (midC)mag *= midC ->getMagnitudeForFrequency(f, (float)sr);
        if (highC) mag *= highC->getMagnitudeForFrequency(f, (float)sr);

        // Convert to dB and remap so [-24..+24] spans the full visualizer range [-100..0].
        float db = juce::Decibels::gainToDecibels(mag, -100.0f);
        db = juce::jlimit(-24.0f, 24.0f, db);
        float displayDb = juce::jmap(db, -24.0f, 24.0f, -100.0f, 0.0f);

        points.emplace_back(f, displayDb);
    }

    {
        const juce::ScopedLock sl(responseLock);
        lastResponse = points;
    }

    // Push to the graph (thread-safe inside the visualizer)
    graph->updateSpectrumData(points);
}

// Parent paint remains minimal; overlay handles labels
void EqualizerVisualizer::paint(juce::Graphics& g)
{
    // no-op; child components handle all painting
}

// ----------------- Y-axis overlay -----------------
void EqualizerVisualizer::YAxisLabelOverlay::paint(juce::Graphics& g)
{
    using namespace juce;

    const int labelWidth = 40;   // must match FrequencyGraphVisualizer
    const int labelHeight = 20;  // bottom x-label area height

    auto bounds = getLocalBounds();
    auto yLabelBounds = bounds.withHeight(bounds.getHeight() - labelHeight).removeFromLeft(labelWidth);

    // Graph vertical bounds inside the child visualizer are reduced by 5px top/bottom
    const int graphTop = 5;
    const int graphBottom = (bounds.getHeight() - labelHeight) - 5;
    const int graphHeight = juce::jmax(0, graphBottom - graphTop);

    // Mask the original labels area
    g.setColour(Colors::panel);
    g.fillRect(yLabelBounds);

    // Draw axis unit label
    g.setColour(Colors::buttonText);
    g.drawText("dB", yLabelBounds, Justification::centred);

    // Desired labels: -24, -12, 0, +12, +24 dB mapped using the same display transform to [-100..0]
    static const int numLabels = 5;
    static const float values[numLabels] = { -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };

    for (int i = 0; i < numLabels; ++i)
    {
        const float v = values[i];
        const float mapped = juce::jmap(v, -24.0f, 24.0f, -100.0f, 0.0f);
        float y = juce::jmap(mapped, -100.0f, 0.0f, (float)graphBottom, (float)graphTop);

        auto labelText = juce::String(v, 0);
        g.drawText(labelText, yLabelBounds.getX(), juce::roundToInt(y) - 6, labelWidth, 12, Justification::centredRight);
    }
}
