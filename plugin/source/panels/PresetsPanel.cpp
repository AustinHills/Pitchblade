// reyna
#include "Pitchblade/panels/PresetsPanel.h"

// preset panel shows "save preset", "load preset", "default preset" buttons
// allows user to save/load presets to/from xml files
// default preset resets all parameters to default values 

PresetsPanel::PresetsPanel(AudioPluginAudioProcessor& proc) : processor(proc) {
	// buttons
    addAndMakeVisible(saveButton);
    addAndMakeVisible(loadButton);
    addAndMakeVisible(defaultButton);

	// button styles
    saveButton.onClick = [this]() { handleSavePreset(); };
    loadButton.onClick = [this]() { handleLoadPreset(); };
    defaultButton.onClick = [this]() { showDefaultMenu(); };

    // preset change confirmation message
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, Colors::buttonText);
    statusLabel.setText("", juce::dontSendNotification);

	// preset loaded message timeout
    statusLabel.setText("Preset loaded successfully!", juce::dontSendNotification);
    juce::Timer::callAfterDelay(2000, [this]() {
        statusLabel.setText("", juce::dontSendNotification); 
        });

}

// paint and layout
void PresetsPanel::paint(juce::Graphics& g) {
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
    g.drawText("Preset Manager", getLocalBounds().removeFromTop(50), juce::Justification::centred, true);

    //horizontal line
    g.setColour(Colors::background);
    float y = (float)getLocalBounds().removeFromTop(42).getBottom();
    g.drawLine(10.0f, y, (float)getWidth() - 10.0f, y, 3.0f);
}

// layout
void PresetsPanel::resized() {
    auto area = getLocalBounds().reduced(45);
    const int buttonH = 60;

    saveButton.setBounds(area.removeFromTop(buttonH).reduced(0, 5));
    loadButton.setBounds(area.removeFromTop(buttonH).reduced(0, 5));
    defaultButton.setBounds(area.removeFromTop(buttonH).reduced(0, 5));

    // Status label
    area.removeFromTop(5);
    statusLabel.setBounds(area.removeFromTop(30));
}

/////////////////////////////////// preset handlers

// save preset to xml file
void PresetsPanel::handleSavePreset() {
	// makes sure preset directory exists
    auto initialDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Pitchblade/Presets");
    initialDir.createDirectory();

	// open file chooser to save preset
    chooser = std::make_unique<juce::FileChooser>("Save Preset", initialDir, "*.xml");

	// launch save dialog
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& c) {
            auto result = c.getResult();
            if (result != juce::File{}) {
                processor.savePresetToFile(result);
                // update label
                statusLabel.setText("Preset saved!", juce::dontSendNotification);
            } else {
                statusLabel.setText("Save canceled", juce::dontSendNotification);
            }
            chooser.reset();
        });
}

// load preset from xml file
void PresetsPanel::handleLoadPreset() {
    auto initialDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Pitchblade/Presets");
    initialDir.createDirectory();

    chooser = std::make_unique<juce::FileChooser>("Load Preset", initialDir, "*.xml");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& c) {
            auto result = c.getResult();
            if (result.existsAsFile()) {
                processor.loadPresetFromFile(result);
                // label 
                statusLabel.setText("Preset loaded!", juce::dontSendNotification);
                juce::MessageManager::callAsync([this]() {
                    if (onPresetActionFinished) onPresetActionFinished();
                    });
            } else {
                statusLabel.setText("Load canceled", juce::dontSendNotification);
            }
            chooser.reset();
        });
}

// show default preset menu
void PresetsPanel::showDefaultMenu() {
    juce::PopupMenu menu;
    //  all effects - all default effects
    menu.addItem("All Effects", [this]() {
        processor.loadDefaultPreset("default");
        statusLabel.setText("Default preset loaded", juce::dontSendNotification);

        juce::MessageManager::callAsync([this]() {
            if (onPresetActionFinished) 
                onPresetActionFinished();
            });
        });

    // empty chain : clear everything
    menu.addItem("Empty Chain", [this]() {
        processor.clearAllNodes();
        statusLabel.setText("Chain cleared", juce::dontSendNotification);

        juce::MessageManager::callAsync([this]() {
            if (onPresetActionFinished) onPresetActionFinished();
            });
        });
    menu.setLookAndFeel(&getLookAndFeel());

    menu.showMenuAsync( 
        juce::PopupMenu::Options()
        .withTargetComponent(&defaultButton)   
        .withMinimumWidth(defaultButton.getWidth())
    );
}
