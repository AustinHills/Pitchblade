//Austin Hills

#include "Pitchblade/panels/SettingsPanel.h"

//Make sure that it has this if it's standalone
#if JUCE_STANDALONE_APPLICATION
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

SettingsPanel::SettingsPanel(AudioPluginAudioProcessor& p) : processor(p) {
    //Framerate label
    framerateLabel.setText("Graph FPS:", juce::dontSendNotification);
    framerateLabel.setJustificationType(juce::Justification::centredLeft);
    framerateLabel.setColour(juce::Label::textColourId,Colors::buttonText);
    addAndMakeVisible(framerateLabel);

    //Framerate Menu
    framerateDropDown.addItemList(juce::StringArray{"5 FPS", "15 FPS", "30 FPS", "60 FPS"},1);
    addAndMakeVisible(framerateDropDown);

    //Attach menu to parameter
    framerateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "GLOBAL_FRAMERATE", framerateDropDown);

    //Theme Label
    themeLabel.setText("Theme:", juce::dontSendNotification);
    themeLabel.setJustificationType(juce::Justification::centredLeft);
    themeLabel.setColour(juce::Label::textColourId,Colors::buttonText);
    addAndMakeVisible(themeLabel);

    //Theme Menu
    themeDropDown.addItemList(juce::StringArray{"Dark", "Light", "Sunset", "Bubblegum", "Mint"}, 1);
    addAndMakeVisible(themeDropDown);
    
    themeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "GLOBAL_THEME", themeDropDown);

    //Configure the Audio Settings button
    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.setColour(juce::TextButton::buttonColourId, Colors::button);
    audioSettingsButton.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
    
    // Only show/enable this button if we are actually in Standalone mode
    audioSettingsButton.setVisible(juce::JUCEApplication::isStandaloneApp());

    audioSettingsButton.onClick = [] {
        #if JUCE_STANDALONE_APPLICATION
            // This opens the standard Audio/MIDI settings dialog
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                holder->showAudioSettingsDialog();
        #endif
    };

    // Monitor Device Configuration (Standalone Only)
    if (juce::JUCEApplication::isStandaloneApp()) {
        
        // 1. Device Selector
        addAndMakeVisible(monitorDeviceLabel);
        monitorDeviceLabel.setText("Monitor Output:", juce::dontSendNotification);
        monitorDeviceLabel.setJustificationType(juce::Justification::centredLeft);
        monitorDeviceLabel.setColour(juce::Label::textColourId, Colors::buttonText);

        addAndMakeVisible(monitorDeviceSelector);
        
        // Populate Device List
        // Note: In real setup, we might need to refresh this. For now, init once.
        const auto& deviceTypes = processor.monitorDeviceManager.getAvailableDeviceTypes();
        int itemId = 1;
        
        // Add "None" option
        monitorDeviceSelector.addItem("None", itemId++);

        for (auto* type : deviceTypes) {
            auto devices = type->getDeviceNames(); 
            for (const auto& deviceName : devices) {
                // Skip if it looks like an input-only device? 
                // device names are just strings. 
                monitorDeviceSelector.addItem(deviceName + " (" + type->getTypeName() + ")", itemId++);
            }
        }
        
        // Restore selection if possible (would need saved state, skipping for now)
        monitorDeviceSelector.onChange = [this] {
            // Parse name from text or maintain a map. Simplest is text for now.
            // Items format: "DeviceName (TypeName)"
            // Use ID to map back? Or just pass full text?
            // Since we stored name+type, we need to extract name or just pass name if it's unique enough.
            // Let's assume for this MVP we just pass the raw "Device Name" part.
            // Actually, setMonitorDevice expects exact name for `outputDeviceName`.
            
            auto text = monitorDeviceSelector.getText();
            if (text == "None") {
                 processor.setMonitorDevice("");
                 return;
            }

            // quick hack to strip (Type)
            int bracket = text.lastIndexOf("(");
            if (bracket > 0) {
                auto name = text.substring(0, bracket).trim();
                processor.setMonitorDevice(name);
            }
        };

        // 2. Volume Slider
        addAndMakeVisible(monitorGainLabel);
        monitorGainLabel.setText("Monitor Vol:", juce::dontSendNotification);
        monitorGainLabel.setJustificationType(juce::Justification::centredLeft);
        monitorGainLabel.setColour(juce::Label::textColourId, Colors::buttonText);

        addAndMakeVisible(monitorGainSlider);
        monitorGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        monitorGainSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
        monitorGainSlider.setRange(0.0, 1.0, 0.01);
        monitorGainSlider.setValue(processor.monitorVolume.load());
        
        monitorGainSlider.onValueChange = [this] {
            processor.monitorVolume.store((float)monitorGainSlider.getValue());
        };
    }

    //License stuff
    addAndMakeVisible(licenseText);
    licenseText.setMultiLine(true);
    licenseText.setReadOnly(true);
    licenseText.setScrollbarsShown(true);
    licenseText.setCaretVisible(false);
    
    // Style the text to look like a label but scrollable
    licenseText.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    licenseText.setColour(juce::TextEditor::textColourId, Colors::buttonText.withAlpha(0.8f));

    refreshColors();

    juce::String msg;


    msg << "PITCHBLADE\n";
    msg << "Copyright (c) 2025 WSU Pitchblade Team\n";
    msg << "Licensed under GNU GPL v3\n\n";
    msg << "CREDITS:\n";
    msg << "- Pitch Shifting: Rubber Band Library (GPL)\n";
    msg << "  (c) Particular Programs Ltd\n";
    msg << "- Framework: JUCE (GPL)\n";
    msg << "  (c) Raw Material Software\n";
    msg << "- VST is a registered trademark of\n";
    msg << "  Steinberg Media Technologies GmbH";

    licenseText.setText(msg);
}

SettingsPanel::~SettingsPanel(){}

void SettingsPanel::paint(juce::Graphics& g){
    auto r = getLocalBounds().toFloat();

    juce::ColourGradient gradient(
        Colors::background.brighter(0.1f),
        r.getX(), r.getY(),
        Colors::background,
        r.getX(), r.getBottom(),
        false
    );

    g.setGradientFill(gradient);

    g.fillRect(getLocalBounds());
    g.drawRect(getLocalBounds(), 2);

    g.setColour(Colors::buttonText);
    g.setFont(24.0f);
    g.drawText("Settings Panel", getLocalBounds().removeFromTop(50), juce::Justification::centred, true);

    //horizontal line
    g.setColour(Colors::background);
    float y = (float)getLocalBounds().removeFromTop(42).getBottom();  
    g.drawLine(10.0f, y, (float)getWidth() - 10.0f, y, 3.0f);
}

void SettingsPanel::resized(){
    //Layout of UI elements
    auto area = getLocalBounds();
    area.removeFromTop(50);

    if (juce::JUCEApplication::isStandaloneApp()){
        //Position the Audio Settings button above the framerate dropdown
        audioSettingsButton.setBounds(area.removeFromTop(40).reduced(20, 0));

        area.removeFromTop(10); 
        
        // Monitor Device UI
        auto monitorArea = area.removeFromTop(40).reduced(20, 0);
        monitorDeviceLabel.setBounds(monitorArea.removeFromLeft(monitorArea.getWidth() / 3));
        monitorArea.removeFromLeft(10);
        monitorDeviceSelector.setBounds(monitorArea);
        
        area.removeFromTop(5);

        auto monitorVolArea = area.removeFromTop(40).reduced(20, 0);
        monitorGainLabel.setBounds(monitorVolArea.removeFromLeft(monitorVolArea.getWidth() / 3));
        monitorVolArea.removeFromLeft(10);
        monitorGainSlider.setBounds(monitorVolArea);

        area.removeFromTop(10);
    }

    auto framerateArea = area.removeFromTop(40).reduced(20,0);

    framerateLabel.setBounds(framerateArea.removeFromLeft(framerateArea.getWidth()/3));
    framerateArea.removeFromLeft(10);
    framerateDropDown.setBounds(framerateArea);

    auto themeArea = area.removeFromTop(40).reduced(20,0);
    themeLabel.setBounds(themeArea.removeFromLeft(themeArea.getWidth()/3));
    themeArea.removeFromLeft(10);
    themeDropDown.setBounds(themeArea);

    // License text takes up some bottom room
    licenseText.setBounds(area.removeFromBottom(100));
}

void SettingsPanel::refreshColors() {
    framerateLabel.setColour(juce::Label::textColourId, Colors::buttonText);
    themeLabel.setColour(juce::Label::textColourId, Colors::buttonText);
    
    // Explicitly update button colors as they might be cached
    audioSettingsButton.setColour(juce::TextButton::buttonColourId, Colors::button);
    audioSettingsButton.setColour(juce::TextButton::textColourOffId, Colors::buttonText);
    
    // Text editor needs updates
    licenseText.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    licenseText.setColour(juce::TextEditor::textColourId, Colors::buttonText.withAlpha(0.8f));

    // Monitor labels (Standalone only)
    if (juce::JUCEApplication::isStandaloneApp()) {
        monitorDeviceLabel.setColour(juce::Label::textColourId, Colors::buttonText);
        monitorGainLabel.setColour(juce::Label::textColourId, Colors::buttonText);
    }

    repaint();
}