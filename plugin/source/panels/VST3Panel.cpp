//Austin

#include "Pitchblade/panels/VST3Panel.h"

//Static global plugin list
juce::KnownPluginList VST3Node::globalPluginList;
std::recursive_mutex VST3Node::pluginListMutex;

//Local Window Class
//This handles the VST3 editor window and ensures it deletes itself properly
class VST3PluginWindow : public juce::DocumentWindow
{
public:
    VST3PluginWindow(const juce::String& name, std::unique_ptr<juce::AudioProcessorEditor> editor)
        : DocumentWindow(name, juce::Desktop::getInstance().getDefaultLookAndFeel()
            .findColour(juce::ResizableWindow::backgroundColourId),
            juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setContentOwned(editor.release(), true);
        setVisible(true);
    }

    //Standard close button behavior
    void closeButtonPressed() override
    {
        delete this;
    }
};

//Scanner Thread Implementation
void VST3Node::ScannerThread::run()
{
    //Check if manager exists
    juce::AudioPluginFormat* vst3Format = nullptr;
    if (owner.formatManager) {
        for (int i = 0; i < owner.formatManager->getNumFormats(); ++i) {
            if (owner.formatManager->getFormat(i)->getName() == "VST3") {
                vst3Format = owner.formatManager->getFormat(i);
                break;
            }
        }
    }

    if (!vst3Format) return;

    //Search paths
    juce::FileSearchPath searchPath;
    #if JUCE_WINDOWS
        searchPath.add(juce::File("C:\\Program Files\\Common Files\\VST3").getFullPathName());
        searchPath.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3").getFullPathName());
    #elif JUCE_MAC
        searchPath.add("/Library/Audio/Plug-Ins/VST3");
        searchPath.add("~/Library/Audio/Plug-Ins/VST3");
    #else
        searchPath.add("/usr/lib/vst3");
    #endif

    // [FIX] Lock during the entire background scan to prevent races with foreground actions
    std::lock_guard<std::recursive_mutex> lock(VST3Node::pluginListMutex);

    //Scanner
    juce::PluginDirectoryScanner scanner(owner.globalPluginList, 
                                         *vst3Format, 
                                         searchPath, 
                                         true, 
                                         juce::File());

    juce::String name;
    
    //Scan loop
    while (scanner.scanNextFile(true, name)) {
        if (threadShouldExit()) break;
        progress.store(scanner.getProgress());
    }

    if (!threadShouldExit()) {
        progress.store(1.0f);
    }
    
    // [FIX] Notify owner that scan is complete
    owner.handleScanFinished();
}

//VST3Panel Implementation
VST3Panel::VST3Panel(AudioPluginAudioProcessor& proc, VST3Node& node) 
    : processor(proc), vstNode(node)
{
    //Lazy init hosting
    vstNode.initializeHosting();

    //Title Label - Austin
    addAndMakeVisible(titleLabel);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    //Scan Button
    addAndMakeVisible(scanButton);
    scanButton.onClick = [this] { vstNode.scanStandardPlugins(); };

    //Plugin List ComboBox
    addAndMakeVisible(pluginList);
    pluginList.onChange = [this] {
        int id = pluginList.getSelectedId();
        if (id > 0) {
            auto list = vstNode.getPluginList().getTypes();
            if (id - 1 < list.size()) {
                vstNode.loadPluginById(list[id - 1].createIdentifierString());
            }
        }
    };
    pluginList.setTextWhenNoChoicesAvailable("No plugins found (Try Scanning)");
    pluginList.setTextWhenNothingSelected("Select a plugin...");

    //Open GUI Button
    addAndMakeVisible(openGuiButton);
    openGuiButton.onClick = [this] { vstNode.openPluginEditor(); };

    //Status Label
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

    //Initial list update only if it's not currently scanning
    if (!vstNode.isScanning()) {
        updatePluginListUI();
    }
    startTimer(100); 
}

VST3Panel::~VST3Panel() { stopTimer(); }

VST3Node::VST3Node(AudioPluginAudioProcessor& proc, const juce::ValueTree& state)
    : EffectNode(proc, state)
{
    scannerThread = std::make_unique<ScannerThread>(*this);
    fifo.resize(fftSize);
    fftData.resize(fftSize * 2);
    
    // Standard init
    if (!formatManager) {
        formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        formatManager->addDefaultFormats();
    }

    // [FIX] Schedule restore to run after the constructor finishes.
    // We must cast the shared pointer to VST3Node because shared_from_this() 
    // returns a pointer to the base EffectNode class.
    juce::MessageManager::callAsync([this]() {
        try {
            if (auto self = std::dynamic_pointer_cast<VST3Node>(shared_from_this())) {
                self->restoreFromState(); // Default param true
            }
        } catch (...) {
            // Abort if node is already deleted
        }
    });
}

void VST3Panel::updatePluginListUI()
{
    // Lock when reading the list for UI update
    std::lock_guard<std::recursive_mutex> lock(VST3Node::pluginListMutex);

    pluginList.clear();
    const auto list = vstNode.getPluginList().getTypes();
    for (int i = 0; i < list.size(); ++i) {
        pluginList.addItem(list[i].name, i + 1);
    }
}

void VST3Panel::resized()
{
    //Reduced bounds for padding - Austin
    auto area = getLocalBounds().reduced(10);
    
    //Layout components
    titleLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    
    scanButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(5);
    
    pluginList.setBounds(area.removeFromTop(30));
    area.removeFromTop(5);
    
    openGuiButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(5);
    
    statusLabel.setBounds(area.removeFromTop(20));
}

void VST3Panel::paint(juce::Graphics& g) {
    //Background
    g.setColour(juce::Colours::black.withAlpha(0.2f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
}

void VST3Panel::timerCallback() {
    //Handle scanning state
    if (vstNode.isScanning()) {
        scanButton.setEnabled(false);
        float p = vstNode.getScanProgress();
        statusLabel.setText("Scanning... " + juce::String((int)(p * 100.0f)) + "%", juce::dontSendNotification);
    } else {
        scanButton.setEnabled(true);
        
        // [FIX] Lock when checking list size
        {
            std::lock_guard<std::recursive_mutex> lock(VST3Node::pluginListMutex);
            if (pluginList.getNumItems() != vstNode.getPluginList().getNumTypes()) {
                // updatePluginListUI handles its own lock, so we unlock here or rely on recursive mutex
                // updatePluginListUI(); // Call below outside this scope or inside recursive is fine
            }
        }
        
        // Re-check with recursive lock safety
        std::lock_guard<std::recursive_mutex> lock(VST3Node::pluginListMutex);
        if (pluginList.getNumItems() != vstNode.getPluginList().getNumTypes()) {
            updatePluginListUI(); // This locks again, fine for recursive
             if (pluginList.getNumItems() > 0)
                statusLabel.setText("Plugins Loaded from Cache", juce::dontSendNotification);
        } else if (vstNode.getLoadedPluginName().isNotEmpty()) {
             statusLabel.setText("Loaded: " + vstNode.getLoadedPluginName(), juce::dontSendNotification);
        } else {
             if (pluginList.getNumItems() == 0) statusLabel.setText("No Plugins Found", juce::dontSendNotification);
             else statusLabel.setText("Ready", juce::dontSendNotification);
        }
    }
}

//VST3 Visualizer Implementation
VST3Visualizer::VST3Visualizer(AudioPluginAudioProcessor& proc, VST3Node& node)
    : vstNode(node)
{
    //Time Graph - Austin
    timeGraph = std::make_unique<RealTimeGraphVisualizer>(proc.apvts, "dB", juce::Range<float>(-100.0f, 0.0f), false, 4);
    addChildComponent(timeGraph.get());
    timeGraph->setVisible(true);

    //Frequency Graph
    freqGraph = std::make_unique<FrequencyGraphVisualizer>(proc.apvts, 4, 0);
    addChildComponent(freqGraph.get());
    freqGraph->setVisible(false);

    //Toggle
    addAndMakeVisible(modeSwitch);
    modeSwitch.onClick = [this] {
        bool showFreq = modeSwitch.getToggleState();
        timeGraph->setVisible(!showFreq);
        freqGraph->setVisible(showFreq);
    };
    startTimerHz(30);
}

void VST3Visualizer::resized() {
    auto area = getLocalBounds();
    modeSwitch.setBounds(area.removeFromTop(25));
    if (timeGraph) timeGraph->setBounds(area);
    if (freqGraph) freqGraph->setBounds(area);
}

void VST3Visualizer::timerCallback() {
    if (timeGraph->isVisible()) {
        timeGraph->pushData(vstNode.getCurrentLevelDb());
        timeGraph->timerCallback(); 
    }
    else if (freqGraph->isVisible()) {
        updateFrequencyData();
        freqGraph->timerCallback();
    }
}

void VST3Visualizer::updateFrequencyData() {
    std::vector<float> rawFFT;
    if (vstNode.getNextFFTBlock(rawFFT)) {
        std::vector<juce::Point<float>> points;
        for (size_t i = 1; i < rawFFT.size() / 2; ++i) {
            float freq = (float)i * 44100.0f / (float)rawFFT.size(); 
            float magnitude = rawFFT[i];
            float db = juce::Decibels::gainToDecibels(magnitude) - 10.0f;
            points.emplace_back(freq, db);
        }
        freqGraph->updateSpectrumData(points);
    }
}

//VST3 Node Implementation
VST3Node::VST3Node(AudioPluginAudioProcessor& proc) 
    : EffectNode(proc, "VST3Node", "External VST3")
{
    scannerThread = std::make_unique<ScannerThread>(*this);
    fifo.resize(fftSize);
    fftData.resize(fftSize * 2);
}

VST3Node::~VST3Node() {
    if (scannerThread->isThreadRunning()) scannerThread->stopThread(2000);
    
    if (activeWindow) {
        activeWindow->setVisible(false);
        delete activeWindow.getComponent();
    }
}

void VST3Node::initializeHosting() {
    if (!formatManager) {
        formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        formatManager->addDefaultFormats();
    }
}

void VST3Node::process(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) {
    //Fix a race condition crash
    std::unique_lock<std::recursive_mutex> lock(proc.getMutex(), std::try_to_lock);
    
    if (lock.owns_lock()) {
        if (hostedPlugin && !bypassed) {
            juce::MidiBuffer midi;
            
            // Determine the maximum channels the plugin can handle
            int maxCh = hostedPlugin->getTotalNumInputChannels();
            
            // Use the smaller of: Host Channels (buffer) OR Plugin Channels
            int numCh = std::min(buffer.getNumChannels(), maxCh);

            if (numCh > 0) {
                // Create a proxy buffer that only exposes the safe channels
                // This does NOT copy audio, it just points to the existing data
                juce::AudioBuffer<float> proxy(buffer.getArrayOfWritePointers(), numCh, buffer.getNumSamples());
                
                hostedPlugin->processBlock(proxy, midi);
            }
        }
    }
    
    //Calculate level for visualizer
    float magnitude = buffer.getMagnitude(0, 0, buffer.getNumSamples());
    currentLevelDb.store(juce::Decibels::gainToDecibels(magnitude, -100.0f));

    //FFT Data
    if (buffer.getNumChannels() > 0) {
        const float* c = buffer.getReadPointer(0);
        for (int i=0; i<buffer.getNumSamples(); ++i) {
            if (fifoIndex < fftSize) fifo[fifoIndex++] = c[i];
            else {
                if (!nextFFTBlockReady.load()) {
                    std::copy(fifo.begin(), fifo.end(), fftData.begin());
                    nextFFTBlockReady.store(true);
                }
                fifoIndex = 0; fifo[0] = c[i];
            }
        }
    }
}

void VST3Node::prepareToPlay(double sampleRate, int samplesPerBlock) {
    if (hostedPlugin) hostedPlugin->prepareToPlay(sampleRate, samplesPerBlock);
}

void VST3Node::scanStandardPlugins() {
    initializeHosting();
    if (scannerThread->isThreadRunning()) return;
    //Below code can be reactivated if there is any issue with rescanning. I had a small issue so I wanted to test it
    // knownPluginList.clear();
    // scannerThread->progress.store(0.0f);
    scannerThread->startThread();
}

bool VST3Node::isScanning() const {
    return scannerThread->isThreadRunning();
}

float VST3Node::getScanProgress() const {
    return scannerThread->progress.load();
}

const juce::KnownPluginList& VST3Node::getPluginList() const {
    return globalPluginList;
}

void VST3Node::loadPluginById(const juce::String& pluginId) {
    initializeHosting();
    
    std::unique_ptr<juce::PluginDescription> type;
    {
        std::lock_guard<std::recursive_mutex> lock(pluginListMutex);
        type = globalPluginList.getTypeForIdentifierString(pluginId);
    }

    if (!type) return;

    //Async load to prevent freezing
    std::weak_ptr<VST3Node> weakSelf = std::dynamic_pointer_cast<VST3Node>(shared_from_this());

    // Dereference type (*type) to pass the PluginDescription reference
    formatManager->createPluginInstanceAsync(*type, 44100.0, 512, 
        [weakSelf](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) {
            if (auto self = weakSelf.lock()) {
                self->finishLoad(std::move(instance), error);
            }
        }
    );
}

void VST3Node::finishLoad(std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& errorMsg, const juce::String& preferredName) {
    if (instance) {
        if (activeWindow) {
            activeWindow->setVisible(false);
            delete activeWindow.getComponent();
        }

        const juce::String oldName = effectName;
        juce::String baseRawName = preferredName.isNotEmpty() ? preferredName : instance->getName();
        
        {
            std::lock_guard<std::recursive_mutex> lock(processor.getMutex());
            
            // 1. Clean Name
            juce::String cleanBase = baseRawName.trim();
            int lastSpace = cleanBase.lastIndexOfChar(' ');
            if (lastSpace > 0) {
                juce::String suffix = cleanBase.substring(lastSpace + 1);
                bool isNumber = true;
                for (auto c : suffix) 
                    if (!juce::CharacterFunctions::isDigit(c)) isNumber = false;
                
                if (isNumber) 
                    cleanBase = cleanBase.substring(0, lastSpace);
            }

            // 2. Unique Name
            juce::String uniqueName = cleanBase;
            int counter = 2;
            auto nameExists = [&](const juce::String& name) {
                const auto& nodes = processor.getEffectNodes();
                for (auto& n : nodes) {
                    if (n && n->effectName == name && n.get() != this)
                        return true;
                }
                return false;
            };

            // If we are restoring from a save, we might want to keep the saved name 
            // even if it duplicates (technically), but unique logic handles conflicts.
            // If preferredName was passed (from restore), we might want to respect it strictly
            // but the uniqueness check is safer.

            if (nameExists(uniqueName)) {
                 while (nameExists(cleanBase + " " + juce::String(counter)))
                     counter++;
                 uniqueName = cleanBase + " " + juce::String(counter);
            }

            // 3. Update ValueTree (This triggers listeners to update UI/Layout)
            getMutableNodeState().setProperty("name", uniqueName, &processor.undoManager);

            // 4. Setup Plugin
            auto layout = processor.getBusesLayout();
            if (instance->checkBusesLayoutSupported(layout)) {
                instance->setBusesLayout(layout);
            }

            double sr = processor.getSampleRate();
            int bs = processor.getBlockSize();
            if (sr <= 0) sr = 44100.0;
            if (bs <= 0) bs = 512;

            hostedPlugin = std::move(instance);
            
            setDisplayName(uniqueName); 
            loadedPluginName = uniqueName;

            hostedPlugin->prepareToPlay(sr, bs);

            // [FIX] Ensure the ValueTree has the plugin info for future saves
            flushPluginStateToValueTree();

            processor.triggerUIRebuild();
        }
    } else {
        // [Optional] If loading failed, maybe don't show a box if it was an automated restore
        // but user might want to know.
        if (preferredName.isEmpty()) // Only show if this was an explicit load attempt?
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed", errorMsg);
    }
}

void VST3Node::openPluginEditor() {
    if (!hostedPlugin) return;
    
    if (auto* editor = hostedPlugin->createEditorIfNeeded()) {
        if (activeWindow) {
            activeWindow->toFront(true);
        } else {
            //Create new window if needed
            auto* w = new VST3PluginWindow(hostedPlugin->getName(), std::unique_ptr<juce::AudioProcessorEditor>(editor));
            activeWindow = w;
        }
    }
}

juce::String VST3Node::getLoadedPluginName() const {
    return loadedPluginName;
}

bool VST3Node::getNextFFTBlock(std::vector<float>& dest) {
    if (nextFFTBlockReady.exchange(false)) {
        dest.resize(fftSize * 2);
        std::copy(fftData.begin(), fftData.end(), dest.begin());
        window.multiplyWithWindowingTable(dest.data(), fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(dest.data());
        return true;
    }
    return false;
}

std::unique_ptr<juce::Component> VST3Node::createPanel(AudioPluginAudioProcessor& proc) {
    return std::make_unique<VST3Panel>(proc, *this);
}

std::unique_ptr<juce::Component> VST3Node::createVisualizer(AudioPluginAudioProcessor& proc) {
    return std::make_unique<VST3Visualizer>(proc, *this);
}

std::shared_ptr<EffectNode> VST3Node::clone() const {
    auto clonePtr = std::make_shared<VST3Node>(processor);
    clonePtr->setDisplayName(effectName); 
    return clonePtr;
}

//Updated the toXml to make it so presets involving VST3s are handled properly!
std::unique_ptr<juce::XmlElement> VST3Node::toXml() const {
    auto xml = std::make_unique<juce::XmlElement>("VST3Node");
    xml->setAttribute("name", effectName);
    
    if (hostedPlugin) {
        // Save the unique ID (file path or plugin ID)
        xml->setAttribute("pluginId", hostedPlugin->getPluginDescription().createIdentifierString());
        
        // Save the plugin's internal state (knobs, settings, etc.)
        juce::MemoryBlock state;
        hostedPlugin->getStateInformation(state);
        xml->setAttribute("state", state.toBase64Encoding());
    }
    return xml;
}

void VST3Node::loadFromXml(const juce::XmlElement& xml) {
    // Initialize hosting to populate formatManager
    initializeHosting();

    // Get the display name and ID
    juce::String name = xml.getStringAttribute("name");
    juce::String pluginId = xml.getStringAttribute("pluginId");
    
    // Decode the state
    juce::MemoryBlock state;
    if (xml.hasAttribute("state")) {
        state.fromBase64Encoding(xml.getStringAttribute("state"));
    }

    juce::PluginDescription desc;

    // Helper lambda to find the plugin description in the global list
    // Returns true if found and updates 'desc'
    auto findPluginInList = [&]() -> bool {
        // [FIX] Lock
        std::lock_guard<std::recursive_mutex> lock(pluginListMutex);
        
        // 1. Try to find by unique Plugin ID (Most Reliable)
        if (pluginId.isNotEmpty()) {
            if (auto type = globalPluginList.getTypeForIdentifierString(pluginId)) {
                desc = *type;
                return true;
            }
        }

        // 2. Fallback: Find by name
        if (name.isNotEmpty()) {
            const auto& types = globalPluginList.getTypes();
            
            // A. Try Exact Match
            for (const auto& type : types) { 
                if (type.name == name) {
                    desc = type;
                    return true;
                }
            }

            // B. Try Stripped Name (e.g. find "Serum" if saved as "Serum 2")
            juce::String cleanName = name.trim();
            int lastSpace = cleanName.lastIndexOfChar(' ');
            if (lastSpace > 0) {
                juce::String suffix = cleanName.substring(lastSpace + 1);
                bool isNumber = true;
                for (auto c : suffix) 
                    if (!juce::CharacterFunctions::isDigit(c)) isNumber = false;

                if (isNumber) {
                    juce::String baseName = cleanName.substring(0, lastSpace);
                    for (const auto& type : types) {
                        if (type.name == baseName) {
                            desc = type;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    };

    // First Lookup Attempt
    bool found = findPluginInList();

    // If not found, perform a synchronous scan and try again
    if (!found) {
        juce::AudioPluginFormat* vst3Format = nullptr;
        if (formatManager) {
            for (int i = 0; i < formatManager->getNumFormats(); ++i) {
                if (formatManager->getFormat(i)->getName() == "VST3") {
                    vst3Format = formatManager->getFormat(i);
                    break;
                }
            }
        }

        if (vst3Format) {
            juce::FileSearchPath searchPath;
            #if JUCE_WINDOWS
                searchPath.add(juce::File("C:\\Program Files\\Common Files\\VST3").getFullPathName());
                searchPath.add(juce::File("C:\\Program Files (x86)\\Common Files\\VST3").getFullPathName());
            #elif JUCE_MAC
                searchPath.add("/Library/Audio/Plug-Ins/VST3");
                searchPath.add("~/Library/Audio/Plug-Ins/VST3");
            #else
                searchPath.add("/usr/lib/vst3");
            #endif

            // Sync scan into global list
            // [FIX] Lock
            {
                std::lock_guard<std::recursive_mutex> lock(pluginListMutex);
                juce::PluginDirectoryScanner scanner(globalPluginList, *vst3Format, searchPath, true, juce::File());
                juce::String scanName;
                while (scanner.scanNextFile(true, scanName)) {
                    // block until finished
                }
            }
        }

        // Second Lookup Attempt after scan
        found = findPluginInList();
    }

    if (found) {
        // Async Load
        std::weak_ptr<VST3Node> weakSelf = std::dynamic_pointer_cast<VST3Node>(shared_from_this());

        formatManager->createPluginInstanceAsync(desc, 44100.0, 512, 
            [weakSelf, state, name](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable {
                if (auto self = weakSelf.lock()) {
                    // Finish load (updates hostedPlugin pointer)
                    self->finishLoad(std::move(instance), error, name);
                    
                    // Apply state safely under lock
                    if (self->hostedPlugin && state.getSize() > 0) {
                        std::lock_guard<std::recursive_mutex> lock(self->processor.getMutex());
                        self->hostedPlugin->setStateInformation(state.getData(), (int)state.getSize());
                    }
                }
            }
        );
    }
}

void VST3Node::restoreFromState(bool allowScan) {
    // 1. Get metadata from the ValueTree
    auto& state = getMutableNodeState();
    juce::String pluginId = state.getProperty("pluginId").toString();
    juce::String savedName = state.getProperty("name").toString();
    juce::String savedStateData = state.getProperty("pluginState").toString();

    // If we have no ID and the name is just "VST3", likely a fresh node, do nothing
    if (pluginId.isEmpty() && savedName == "VST3") return;

    initializeHosting();

    juce::PluginDescription desc;

    // 2. Helper to find plugin in global list
    auto findPluginInList = [&]() -> bool {
        // [FIX] Lock list access
        std::lock_guard<std::recursive_mutex> lock(pluginListMutex);

        // A. Try exact ID match
        if (pluginId.isNotEmpty()) {
            if (auto type = globalPluginList.getTypeForIdentifierString(pluginId)) {
                desc = *type;
                return true;
            }
        }

        // B. Try Fuzzy Name match
        if (savedName.isNotEmpty()) {
            const auto& types = globalPluginList.getTypes();
            // Try exact name
            for (const auto& type : types) {
                if (type.name == savedName) {
                    desc = type;
                    return true;
                }
            }
            // Try stripped name (e.g. "Serum 2" -> "Serum")
            juce::String cleanName = savedName.trim();
            int lastSpace = cleanName.lastIndexOfChar(' ');
            if (lastSpace > 0) {
                juce::String baseName = cleanName.substring(0, lastSpace);
                for (const auto& type : types) {
                    if (type.name == baseName) {
                        desc = type;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // 3. First Attempt
    bool found = findPluginInList();

    // 4. If not found and scan allowed, start BACKGROUND scan
    if (!found) {
        if (allowScan) {
            // Setup callback to try again (with allowScan=false to prevent loops)
            std::weak_ptr<VST3Node> weakSelf = std::dynamic_pointer_cast<VST3Node>(shared_from_this());
            
            pendingScanAction = [weakSelf]() {
                if (auto self = weakSelf.lock()) {
                    self->restoreFromState(false);
                }
            };

            // Trigger scan if not already running
            if (!isScanning()) {
                scanStandardPlugins();
            }
            // If already scanning, the handleScanFinished() will eventually fire 
            // and execute our pendingScanAction.
        }
        return; // Return immediately to unblock UI
    }

    // 6. Load if found
    if (found) {
        std::weak_ptr<VST3Node> weakSelf = std::dynamic_pointer_cast<VST3Node>(shared_from_this());

        formatManager->createPluginInstanceAsync(desc, 44100.0, 512,
            [weakSelf, savedStateData, savedName](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error) mutable {
                if (auto self = weakSelf.lock()) {
                    
                    // Finish load (updates hostedPlugin pointer)
                    self->finishLoad(std::move(instance), error, savedName);

                    // Restore internal plugin state (knobs etc)
                    if (self->hostedPlugin && savedStateData.isNotEmpty()) {
                        juce::MemoryBlock mb;
                        if (mb.fromBase64Encoding(savedStateData)) {
                             std::lock_guard<std::recursive_mutex> lock(self->processor.getMutex());
                             self->hostedPlugin->setStateInformation(mb.getData(), (int)mb.getSize());
                        }
                    }
                }
            }
        );
    }
}

void VST3Node::flushPluginStateToValueTree() {
    if (!hostedPlugin) return;

    // Update ID
    getMutableNodeState().setProperty("pluginId", 
        hostedPlugin->getPluginDescription().createIdentifierString(), 
        &processor.undoManager);

    // Update State Blob
    juce::MemoryBlock state;
    hostedPlugin->getStateInformation(state);
    getMutableNodeState().setProperty("pluginState", 
        state.toBase64Encoding(), 
        &processor.undoManager);
}

void VST3Node::handleScanFinished() {
    // We try to capture a weak reference to ourselves.
    // If we are in the process of destruction, this might throw or fail gracefully.
    try {
        std::weak_ptr<VST3Node> weakSelf = std::dynamic_pointer_cast<VST3Node>(shared_from_this());
        
        juce::MessageManager::callAsync([weakSelf]() {
            if (auto self = weakSelf.lock()) {
                if (self->pendingScanAction) {
                    auto action = self->pendingScanAction;
                    self->pendingScanAction = nullptr;
                    action();
                }
            }
        });
    } catch (...) {}
}