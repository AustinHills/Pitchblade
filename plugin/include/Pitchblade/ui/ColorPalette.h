//reyna macabebe
/* shared color palette for the entire UI using JUCE framework customLookandfeel */

#pragma once
#include <JuceHeader.h>

// Color palette
namespace Colors
{
    inline const auto background    = juce::Colour::fromString("ff363e52");     // background
    inline const auto panel         = juce::Colour::fromString("ff19182b");     // panel bg

    inline const auto accent        = juce::Colour::fromString("fff551c1");     // outlines & active text
    inline const auto accentLight   = juce::Colour::fromString("ff686495");     // accent color

    inline const auto accentPink    = juce::Colour::fromString("ffe966ed");     // light pink
    inline const auto accentPurple  = juce::Colour::fromString("ffae66ed");     // purple
    inline const auto accentBlue    = juce::Colour::fromString("ff668ced");     // blue
    inline const auto accentTeal    = juce::Colour::fromString("ff46bad4");     // teal

    inline const auto button        = juce::Colour::fromString("ff19182b");     // button bg
    inline const auto buttonText    = juce::Colours::white;                     // button text normal
    inline const auto buttonActive  = juce::Colour::fromString("fff551c1");     // button text active
}