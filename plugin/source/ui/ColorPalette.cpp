#include "Pitchblade/ui/ColorPalette.h"

namespace Colors
{
    // Definitions
    // Definitions
    juce::Colour background   = juce::Colour::fromString("ff363e52");
    juce::Colour panel        = juce::Colour::fromString("ff19182b");
    juce::Colour accent       = juce::Colour::fromString("fff551c1");
    juce::Colour accentLight  = juce::Colour::fromString("ff686495");
    juce::Colour accentPink   = juce::Colour::fromString("ffe966ed");
    juce::Colour accentPurple = juce::Colour::fromString("ffae66ed");
    juce::Colour accentBlue   = juce::Colour::fromString("ff668ced");
    juce::Colour accentTeal   = juce::Colour::fromString("ff46bad4");
    juce::Colour button       = juce::Colour::fromString("ff19182b");
    juce::Colour buttonText   = juce::Colours::white;
    juce::Colour buttonActive = juce::Colour::fromString("fff551c1");

    static Theme currentTheme = Theme::Dark;

    Theme getCurrentTheme() { return currentTheme; }

    void setTheme(Theme t)
    {
        currentTheme = t;

        switch (t)
        {
        case Theme::Dark:
            // Original Dark Theme
            background    = juce::Colour::fromString("ff363e52");
            panel         = juce::Colour::fromString("ff19182b");
            accent        = juce::Colour::fromString("fff551c1");
            accentLight   = juce::Colour::fromString("ff686495");
            accentPink    = juce::Colour::fromString("ffe966ed");
            accentPurple  = juce::Colour::fromString("ffae66ed");
            accentBlue    = juce::Colour::fromString("ff668ced");
            accentTeal    = juce::Colour::fromString("ff46bad4");
            button        = juce::Colour::fromString("ff19182b");
            buttonText    = juce::Colours::white;
            buttonActive  = juce::Colour::fromString("fff551c1");
            break;

        case Theme::Light:
            // Modern Clean Light (Refined: Gray/Blue + Coral)
            background    = juce::Colour::fromString("ffF0F2F5"); // Cool Light Gray
            panel         = juce::Colour::fromString("ffE1E4E8"); // Mid Cool Gray
            accent        = juce::Colour::fromString("ff3498DB"); // Sky Blue (Primary)
            accentLight   = juce::Colour::fromString("ff95A5A6"); // Darker Gray (Concrete)
            accentPink    = juce::Colour::fromString("ff3498DB"); // Mapped to Blue for Sliders (Start)
            accentPurple  = juce::Colour::fromString("ff9B59B6"); // Amethyst (Complementary)
            accentBlue    = juce::Colour::fromString("ff2980B9"); // Darker Blue
            accentTeal    = juce::Colour::fromString("ffFF7675"); // Soft Coral (Secondary/Slider End)
            button        = juce::Colour::fromString("ffE1E4E8"); // Matches panel
            buttonText    = juce::Colour::fromString("ff2C3E50"); // Dark Blue-Gray text
            buttonActive  = juce::Colour::fromString("ff3498DB"); // Active state Blue
            break;

        case Theme::Sunset:
            // Retro / Synthwave Sunset (Refined 2)
            background    = juce::Colour::fromString("ff3D1E45"); // Deep Purple (Less Muted/Bluer)
            panel         = juce::Colour::fromString("ff6D2753"); // Magenta/Purple (Lighter/Bluer)
            accent        = juce::Colour::fromString("ffFF4D6D"); // Hot pink/red
            accentLight   = juce::Colour::fromString("ffC9184A");
            accentPink    = juce::Colour::fromString("ffFF4D6D"); 
            accentPurple  = juce::Colour::fromString("ffA4133C"); 
            accentBlue    = juce::Colour::fromString("ffFF758F"); // Lighter pinkish
            accentTeal    = juce::Colour::fromString("ffFF9E00"); // Orange for Pink->Orange gradients
            button        = juce::Colour::fromString("ff590D22");
            buttonText    = juce::Colour::fromString("ffFFF0F3"); // Pale
            buttonActive  = juce::Colour::fromString("ffFF4D6D");
            break;

        case Theme::Pink:
            // Pastel Pink Dream
            background    = juce::Colour::fromString("ffFFE5EC"); // Very pale pink bg
            panel         = juce::Colour::fromString("ffFFB3C6"); // Darker pink panel
            accent        = juce::Colour::fromString("ffFB6F92"); // Strong pink accent
            accentLight   = juce::Colour::fromString("ffFA709A"); // Slightly darker pink
            accentPink    = juce::Colour::fromString("ffFB6F92");
            accentPurple  = juce::Colour::fromString("ffD81159"); // Deep magenta
            accentBlue    = juce::Colour::fromString("ff8F2D56"); // Tyrian purple
            accentTeal    = juce::Colour::fromString("ffFFC2D1"); 
            button        = juce::Colour::fromString("ffFFB3C6");
            buttonText    = juce::Colour::fromString("ff590D22"); // Dark red text for contrast
            buttonActive  = juce::Colour::fromString("ffD81159");
            break;

        case Theme::Green:
            // Mint / Pastel Green
            background    = juce::Colour::fromString("ffD8F3DC"); // Mint cream
            panel         = juce::Colour::fromString("ff95D5B2"); // Soft green
            accent        = juce::Colour::fromString("ff1B4332"); // Dark forest green (contrast)
            accentLight   = juce::Colour::fromString("ff40916C");
            accentPink    = juce::Colour::fromString("ff2D6A4F"); // Using green tones for "pink" slots to maintain harmony
            accentPurple  = juce::Colour::fromString("ff1B4332");
            accentBlue    = juce::Colour::fromString("ff52B788");
            accentTeal    = juce::Colour::fromString("ff74C69D");
            button        = juce::Colour::fromString("ff95D5B2");
            buttonText    = juce::Colour::fromString("ff081C15"); // Very dark green text
            buttonActive  = juce::Colour::fromString("ff1B4332");
            break;
        }
    }
}
