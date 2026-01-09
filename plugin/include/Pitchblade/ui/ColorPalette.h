//reyna macabebe
/* shared color palette for the entire UI using JUCE framework customLookandfeel */

#pragma once
#include <JuceHeader.h>

// Color palette
namespace Colors
{
    // Define Theme Enum
    enum class Theme {
        Dark,
        Light,
        Sunset,
        Pink,
        Green
    };

    // Control functions
    void setTheme(Theme t);
    Theme getCurrentTheme();

    // Variable colors (now extern, defined in .cpp)
    extern juce::Colour background;
    extern juce::Colour panel;
    
    extern juce::Colour accent;
    extern juce::Colour accentLight;

    extern juce::Colour accentPink;
    extern juce::Colour accentPurple;
    extern juce::Colour accentBlue;
    extern juce::Colour accentTeal;

    extern juce::Colour button;
    extern juce::Colour buttonText;
    extern juce::Colour buttonActive;
}