// huda
#include "Pitchblade/ui/FormantVisualizer.h"

//=========================== Overlay ===========================
void FormantVisualizer::FormantOverlay::paint(juce::Graphics& g)
{
    // Draw overlay markers, labels, and curve on top of the underlying frequency grid
    // Transparent background; draw only markers over the grid
    auto bounds = getLocalBounds();
    auto graph = bounds.reduced(0);
    // align with FrequencyGraphVisualizer layout
    auto xLabels = graph.removeFromBottom(labelHeight);
    auto yLabels = graph.removeFromLeft(labelWidth);

    // Hide y-axis labeling and x-axis label strip from the underlying FrequencyGraphVisualizer
    // without modifying it: paint over the left strip and bottom strip.
    g.setColour(Colors::panel);
    g.fillRect(juce::Rectangle<int>(bounds.getX(), bounds.getY(), labelWidth, bounds.getHeight() - labelHeight));
    // cover the FrequencyGraphVisualizer x-axis area (accounting for its internal padding)
    const int freqGraphPad = 15; // padding used inside FrequencyGraphVisualizer
    auto fgBounds = bounds.reduced(freqGraphPad);
    auto fgXLabels = fgBounds.removeFromBottom(labelHeight);
    const int coverY = std::max(bounds.getY(), fgXLabels.getY() - 6); // extend a few px upward to hide its border
    const int coverHeight = bounds.getBottom() - coverY;
    g.fillRect(juce::Rectangle<int>(bounds.getX(), coverY, bounds.getWidth(), coverHeight));

    // Compute an inner drawing area to avoid edge clipping and visually center content
    const int padX = 8;           // horizontal padding to keep lines off the border
    const int padTop = 2;         // small pad from top border
    const int padBottom = 2;      // small pad from bottom border
    auto inner = graph;
    inner.removeFromLeft(padX);
    inner.removeFromRight(padX);
    inner.removeFromTop(padTop);
    inner.removeFromBottom(padBottom);

    // Title across the top of the plotting area
    {
        const int titleH = 18;
        juce::Rectangle<int> titleStrip(bounds.getX() + labelWidth, bounds.getY(), bounds.getWidth() - labelWidth, titleH);
        // Cover the top border line so it doesn't collide with the title
        g.setColour(Colors::panel);
        g.fillRect(titleStrip);
        g.setColour(Colors::buttonText);
        g.setFont(12.0f);
        g.drawText("Formants", titleStrip, juce::Justification::centred, true);
    }

    // Draw custom x-axis labels for 300–5000 Hz over the hidden bottom strip (always present)
    g.setColour(Colors::buttonText);
    g.setFont(10.0f);
    const float tickY = (float) (bounds.getBottom() - labelHeight + 2);
    const std::vector<float> ticks { 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f };
    for (float t : ticks)
    {
        const float x = mapVisibleFreqToX(t, inner);

        // small tick mark
        g.drawVerticalLine((int)std::round(x), (float)inner.getBottom(), (float)inner.getBottom() + 4.0f);

        // label text
        juce::String label;
        if (t >= 1000.0f) label = juce::String(t / 1000.0f, 0) + "k";
        else              label = juce::String(t, 0);

        g.drawText(label,
                   (int)std::round(x) - 20,
                   bounds.getBottom() - labelHeight,
                   40,
                   labelHeight,
                   juce::Justification::centred);
    }

    // Draw a smooth curve representing the current formant positions
    const auto& freqs = proc.getLatestFormants();
    const float sigma = 0.03f;                // slightly sharper peaks to emphasize motion
    const float twoSigma2 = 2.0f * sigma * sigma;

    auto drawCurveFor = [&](const std::vector<float>& fset, juce::Colour c, float thickness)
    {
        if (fset.empty()) return;
        juce::Path p;
        bool started = false;
        for (int px = inner.getX(); px <= inner.getRight(); ++px)
        {
            const float fx = (float) px;
            const float fHz = mapXToVisibleFreq(fx, inner);
            const float lf = std::log10(juce::jlimit(visibleXAxisHz.getStart(), visibleXAxisHz.getEnd(), fHz));

            float amp = 0.0f;
            for (float cHz : fset)
            {
                if (cHz < visibleXAxisHz.getStart() || cHz > visibleXAxisHz.getEnd()) continue;
                const float lc = std::log10(cHz);
                const float d = lf - lc;
                amp += std::exp(-(d * d) / twoSigma2);
            }
            amp = juce::jlimit(0.0f, 1.0f, amp);
            const float y = juce::jmap(amp, 0.0f, 1.0f, (float)inner.getBottom(), (float)inner.getY());
            if (!started) { p.startNewSubPath(fx, y); started = true; }
            else { p.lineTo(fx, y); }
        }
        g.setColour(c);
        g.strokePath(p, juce::PathStrokeType(thickness));
    };

    // Draw ghost of previous curve to emphasize movement
    if (hasLast)
        drawCurveFor(lastFormants, Colors::accent.withAlpha(0.3f), 2.0f);
    // Draw current curve bold
    drawCurveFor(freqs, Colors::accent, 3.0f);

    // Draw pulsing markers at current formant centers to make motion obvious
    if (!freqs.empty())
    {
        pulse += 0.15f; if (pulse > juce::MathConstants<float>::twoPi) pulse -= juce::MathConstants<float>::twoPi;
        for (float fHz : freqs)
        {
            if (fHz < visibleXAxisHz.getStart() || fHz > visibleXAxisHz.getEnd()) continue;
            const float x = mapVisibleFreqToX(fHz, inner);

            // Find curve amplitude at this x for y position (recompute quickly)
            const float lf = std::log10(fHz);
            float amp = 0.0f;
            for (float cHz : freqs)
            {
                const float lc = std::log10(juce::jlimit(visibleXAxisHz.getStart(), visibleXAxisHz.getEnd(), cHz));
                const float d = lf - lc;
                amp += std::exp(-(d * d) / twoSigma2);
            }
            amp = juce::jlimit(0.0f, 1.0f, amp);
            const float y = juce::jmap(amp, 0.0f, 1.0f, (float)inner.getBottom(), (float)inner.getY());

            const float r = 4.0f + 2.0f * (0.5f + 0.5f * std::sin(pulse));
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillEllipse(x - r, y - r, 2*r, 2*r);
            g.setColour(Colors::accent);
            g.drawEllipse(x - r, y - r, 2*r, 2*r, 2.0f);
        }
    }

    // Update last set for next frame
    lastFormants = freqs;
    hasLast = true;
}

float FormantVisualizer::FormantOverlay::mapVisibleFreqToX(float freq, juce::Rectangle<int> graph) const
{
    // Map Hz within the focused 300-5000 Hz range to pixel X
    auto clamped = juce::jlimit(visibleXAxisHz.getStart(), visibleXAxisHz.getEnd(), freq);
    float lf = std::log10(clamped);
    float proportion = (lf - logVisibleStart) / (logVisibleEnd - logVisibleStart);
    // Avoid drawing on the exact right edge to prevent clipping
    return juce::jmap(proportion, (float)graph.getX(), (float)graph.getRight() - 1.0f);
}

float FormantVisualizer::FormantOverlay::mapXToVisibleFreq(float x, juce::Rectangle<int> graph) const
{
    // Convert pixel X back to Hz for sampling the curve
    const float proportion = juce::jlimit(0.0f, 1.0f, (x - (float)graph.getX()) / (float)graph.getWidth());
    const float lf = juce::jmap(proportion, logVisibleStart, logVisibleEnd);
    return std::pow(10.0f, lf);
}

//=========================== Container ===========================
FormantVisualizer::FormantVisualizer(AudioPluginAudioProcessor& processorRef,
                                     juce::AudioProcessorValueTreeState& vts)
    : processor(processorRef), apvts(vts)
{
    // Compose background graph plus overlay and drive refresh from GLOBAL_FRAMERATE
    // Background grid/axes from FrequencyGraphVisualizer
    // Pass 0 y-axis labels to hide y-axis labeling for formants
    freqGraph = std::make_unique<FrequencyGraphVisualizer>(apvts, 0, 0);
    addAndMakeVisible(freqGraph.get());

    // Overlay markers
    overlay = std::make_unique<FormantOverlay>(processor, apvts);
    addAndMakeVisible(overlay.get());

    // Listen to global framerate and start timer accordingly
    apvts.addParameterListener("GLOBAL_FRAMERATE", this);

    int initialIndex = apvts.getRawParameterValue("GLOBAL_FRAMERATE")
                         ? (int) *apvts.getRawParameterValue("GLOBAL_FRAMERATE")
                         : 2; // default ~30 Hz

    switch (initialIndex)
    {
        case 0: startTimerHz(5);  break;
        case 1: startTimerHz(15); break;
        case 2: startTimerHz(30); break;
        case 3: startTimerHz(60); break;
        default: startTimerHz(30); break;
    }
}

FormantVisualizer::~FormantVisualizer()
{
    stopTimer();
    apvts.removeParameterListener("GLOBAL_FRAMERATE", this);
}

void FormantVisualizer::resized()
{
    // Keep overlay and grid stacked in the same bounds
    auto b = getLocalBounds();
    if (freqGraph) freqGraph->setBounds(b);
    if (overlay)   overlay->setBounds(b);
}

void FormantVisualizer::paint(juce::Graphics& g)
{
    // Container paints nothing; children handle drawing
}

void FormantVisualizer::timerCallback()
{
    // Repaint at the chosen frame rate when visible
    if (isShowing() && overlay)
        overlay->repaint();
}

void FormantVisualizer::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Adjust repaint cadence when the global framerate setting changes
    if (parameterID != "GLOBAL_FRAMERATE")
        return;

    int index = (int) newValue;
    switch (index)
    {
        case 0: startTimerHz(5);  break;
        case 1: startTimerHz(15); break;
        case 2: startTimerHz(30); break;
        case 3: startTimerHz(60); break;
        default: startTimerHz(30); break;
    }
}
