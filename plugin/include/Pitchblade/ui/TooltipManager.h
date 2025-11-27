//reyna
// used to add tooltip text to components
#pragma once
#include <JuceHeader.h>
#include <unordered_map>

class TooltipManager {
public:
	TooltipManager() = default;     // default constructor

    // tooltip displays above everything
    void initialize( juce::LookAndFeel* lf) {
        if (!tooltipWindow) {
            tooltipWindow = std::make_unique<juce::TooltipWindow>(nullptr, 500); // delay for tooltip to show up
            tooltipWindow->setLookAndFeel(lf);
            tooltipWindow->setAlwaysOnTop(true);  
            tooltipWindow->setOpaque(false);
        }
    }

	// singleton instance
    static TooltipManager& instance() {
        static TooltipManager tm;
        return tm;
    }

	// load tooltips from file 
    void loadTooltipsFromFile(const juce::File& file) {
        tooltips.clear();

        juce::StringArray lines;  
		file.readLines(lines);      // read line by line
        
        for (auto& line : lines) {
			if (line.trim().isEmpty() || line.trim().startsWithChar('//')) {     // skip empty lines and comments
                continue;
            }
			auto parts = juce::StringArray::fromTokens(line, "=", "");          // split where "=" is in file
            if (parts.size() == 2) {
				tooltips[parts[0].trim()] = parts[1].trim();    // store in map
            }
        }
    }

	// get tooltip for a given part
    juce::String getTooltipFor(const juce::String& part) const {
        if (auto it = tooltips.find(part); it != tooltips.end())
            return it->second;
        return {};
    }

private:
	// map of part names to tooltips
    std::unordered_map<juce::String, juce::String> tooltips;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
};
