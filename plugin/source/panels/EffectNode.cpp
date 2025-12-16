#include "Pitchblade/panels/EffectNode.h"
#include "Pitchblade/PluginProcessor.h"
#include "Pitchblade/ui/EffectPanel.h"

EffectNode::EffectNode(AudioPluginAudioProcessor& proc, const juce::String& type, const juce::String& displayName) : processor(proc),
                                                                        nodeType(type), effectName(displayName), nodeState(juce::ValueTree(juce::Identifier(type))) {
    nodeState.setProperty("name", effectName, nullptr);                 // set display name
    nodeState.setProperty("uuid", juce::Uuid().toString(), nullptr);    // unique id
    nodeState.addListener(this);                                        // listen to state changes
}

EffectNode::EffectNode(AudioPluginAudioProcessor& proc, const juce::ValueTree& existingState) : processor(proc),
                                                                    nodeType(existingState.getType().toString()),
                                                                    effectName(existingState.getProperty("name", "Effect").toString()), nodeState(existingState) {
    jassert(nodeState.isValid());   // ensure valid state
    nodeState.addListener(this);  
}

void EffectNode::processAndForward(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) {
    // make a copy of the input
    juce::AudioBuffer<float> temp(buffer);
    temp.makeCopyOf(buffer, true);
    if (!bypassed) {
        process(proc, temp);
    }

    //  chain mode rout behavior
    switch (chainMode) {
    case ChainMode::Down:   
    case ChainMode::LeftDouble: // New: Behaves same as Down (Single Output)
    { // Single output
        if (!children.empty() && children.front()) {    
            children.front()->processAndForward(proc, temp);    // process first child
            buffer.makeCopyOf(temp, true);          // output processed buffer
        } else {
            buffer.makeCopyOf(temp, true);          // No children, output the processed buffer
        }
        break;
    }

    case ChainMode::Split: 
    { //  Multiple outputs
        if (children.empty()) {             // if no children
            buffer.makeCopyOf(temp, true);  // output processed buffer
            break;
        }
        // prepare mix buffer
        const int numCh = buffer.getNumChannels();  
        const int nsamp = buffer.getNumSamples();
        juce::AudioBuffer<float> mix(numCh, nsamp);     // mix buffer
        mix.clear();
        int branches = 0;               // count active branches

        for (auto& c : children) {      // process each child
            if (!c) continue;           // skip null children
            juce::AudioBuffer<float> branch(temp);  // copy input
            branch.makeCopyOf(temp, true);          
            c->processAndForward(proc, branch);     // process child
            
            for (int ch = 0; ch < numCh; ++ch) // sum into mix
                mix.addFrom(ch, 0, branch, ch, 0, nsamp, 1.0f);     
            ++branches;     
        }
        
        if (branches > 0)
            mix.applyGain(1.0f / (float)branches);  // average

        buffer.makeCopyOf(mix, true);
        break;
    }

    case ChainMode::DoubleDown:
    { // Two outputs summed (Original DoubleDown / Right Double behavior)
        if (children.size() >= 2 && children[0] && children[1]) {   // need two valid children
            juce::AudioBuffer<float> a(temp), b(temp);  // buffers for each branch
            a.makeCopyOf(temp, true);           // copy inputs 
            b.makeCopyOf(temp, true);           

            children[0]->processAndForward(proc, a);    // process each child
            children[1]->processAndForward(proc, b);

            buffer.makeCopyOf(a, true);                 // start with first branch
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)    // sum second branch
                buffer.addFrom(ch, 0, b, ch, 0, b.getNumSamples(), 1.0f);   

            buffer.applyGain(0.5f); // average 
        }
        else if (!children.empty() && children.front()) {
            children.front()->processAndForward(proc, temp); // fallback to Down if only one child
            buffer.makeCopyOf(temp, true);
        } else {
            buffer.makeCopyOf(temp, true);  // no children, output processed buffer
        }
        break;
    }

    case ChainMode::Unite: 
    { // single output merged from all children
        // nothing to unite, behave as down
        if (parents.empty()) {             
            if (!children.empty() && children.front()) {
                children.front()->processAndForward(proc, temp);    // process first child
                buffer.makeCopyOf(temp, true);                      // output processed buffer
            }
            else {
                buffer.makeCopyOf(temp, true);  // No children, output the processed buffer
            }
            break;
        }

        // process each parent and accumulate into mix buffer
        const int numCh = temp.getNumChannels();
        const int nSamp = temp.getNumSamples();

        if (uniteMixBuffer.getNumChannels() != numCh || uniteMixBuffer.getNumSamples() != nSamp) {
            uniteMixBuffer.setSize(numCh, nSamp, false, false, true);       // resize mix buffer if needed
        }
        if (uniteAccumulated == 0) {
            uniteMixBuffer.clear();     // clear mix buffer on first accumulation
        }

        // process each parent
        for (int ch = 0; ch < numCh; ++ch) {
            uniteMixBuffer.addFrom(ch, 0, temp, ch, 0, nSamp, 1.0f);
        }
        ++uniteAccumulated; // number of parents processed

        // check if all parents have contributed, if not, wait for more
        const int expected = static_cast<int>(parents.size());
        if (uniteAccumulated < std::max(1, expected)) {
            return;
        }

        // average the mix
        if (expected > 0) {
            uniteMixBuffer.applyGain(1.0f / static_cast<float>(expected));
        }

        // process node and forward once
        buffer.makeCopyOf(uniteMixBuffer, true);
        uniteAccumulated = 0; // reset for next block

        if (!bypassed) {    
            process(proc, buffer);      // process the united buffer
        }

        if (!children.empty() && children.front()) {    // forward to first child
            children.front()->processAndForward(proc, buffer);  // process it
        }
        break;
    }

    default:
        break;
    }
}

void EffectNode::mergeParentBuffers(AudioPluginAudioProcessor& proc, juce::AudioBuffer<float>& buffer) {
    // clear output buffer
    if (parents.empty())
        return;

    //  prepare for merging
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    juce::AudioBuffer<float> mergeBuffer(numChannels, numSamples);  // temporary buffer for merging
    mergeBuffer.clear();

    int activeInputs = 0;

    for (auto& weakParent : parents) {  // iterate over parents
        if (auto parent = weakParent.lock()) {  // check if parent is still valid
            juce::AudioBuffer<float> temp(numChannels, numSamples); 
            temp.clear();
            parent->process(proc, temp);    // process parent into temp buffer

            for (int ch = 0; ch < numChannels; ++ch) {
                mergeBuffer.addFrom(ch, 0, temp, ch, 0, numSamples);    // sum into merge buffer
            }
            ++activeInputs;
        }
    }

    if (activeInputs > 0) {
        mergeBuffer.applyGain(1.0f / (float)activeInputs);      // average
    }
    for (int ch = 0; ch < numChannels; ++ch) {
        buffer.copyFrom(ch, 0, mergeBuffer, ch, 0, numSamples); // copy merged data to output buffer
    }
}
    // allows multiple outputs
    void EffectNode::connectTo(std::shared_ptr<EffectNode> next) {
        // avoid self-connection and null
        if (!next || next.get() == this) { return; }

        // avoid duplicate child links
        if (std::find(children.begin(), children.end(), next) == children.end()) { 
            children.push_back(next);
        }
        std::shared_ptr<EffectNode> self;   // shared ptr to this

        try {   // only safe if shared_from_this() is valid
            self = shared_from_this();
        } catch (const std::bad_weak_ptr&) {
            juce::Logger::outputDebugString(" connectTo(): error for " + effectName);
            return;
        } 

        // avoid duplicate parent links
        auto alreadyParent = std::any_of(next->parents.begin(), next->parents.end(),
            [&](const std::weak_ptr<EffectNode>& w) {   // check if this is already a parent
                auto p = w.lock();                      // try to get shared_ptr
                return p && p.get() == this;            // compare raw pointers
            });
        if (!alreadyParent)
            next->parents.push_back(self);  // in case shared_from_this() is called on an object not owned by a shared_ptr
    }
