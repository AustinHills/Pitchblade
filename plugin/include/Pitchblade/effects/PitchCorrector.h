/**
 * Author: Hayley Spellicy-Ryan
 * 
 * Pitch Correction
 */
#pragma once

#include "PitchDetector.h"
#include "PitchShifter.h"

enum scaleType{
    Major,
    Minor
};

class PitchCorrector{
public:
    PitchCorrector(IPitchDetector& detector, IPitchShifter& shifter)
        : pitchDetector(detector), pitchShifter(shifter) {}

    void prepare(double, int);
    void processBlock(juce::AudioBuffer<float>&);

    void setScaleType(int);
    void setScaleOffset(int);
    void setCorrectionRatio(float);
    void setRetuneSpeed(float);
    void setNoteTransition(float);
    void setWaver(float);

    int getScaleType() { return scaleType; }
    int getScaleOffset() { return scaleOffset; }
    float getCorrectionRatio() { return smoothing; }
    float getRetuneSpeed() { return retuneSpeed; } 
    float getNoteTransition() { return noteTransition; }
    float getWaver() { return waver; }

    float getCurrentPitch() { return pitchDetector.getCurrentPitch(); }
    float getTargetPitch() { return targetPitch; }
    float getSemitoneError();
    std::string getCurrentNoteName();
    std::string getTargetNoteName();
    
    std::atomic<float> currentOutputPitch{69.f};
    bool getWasBypassing(){ return wasBypassing; };

    IPitchDetector& getDetector();

private:
    int quantizeToScale(int);
    static float noteToFrequency(float midi);
    static float frequencyToNote(float freq);
    float applyParameters(float &midi);

    IPitchDetector& pitchDetector;
    IPitchShifter& pitchShifter;

    juce::AudioBuffer<float> monoBuffer;

    std::vector<std::vector<int>> scale = {
        { 12, 14, 16, 17, 19, 21, 23}, // Major
        { 12, 14, 15, 17, 19, 20, 22}  // Minor
    };
    int scaleType = 0;
    int scaleOffset = 0;
    
    // Parameters
    float currentRatio = 1.0f;
    float noteTransition;
    float retuneSpeed;
    float waver;
    //Parameters helpers
    float smoothing = 1.0f;
    float waverPhase = 0.f;
    float prevMidi;
    float lastStableMidi;
    bool wasBypassing;
    int stableCount;
    const int stableThreshold = 3;

    // Hz
    float targetPitch;
    float correctedPitch;

    // Midi
    float currentMidi;
    float targetMidi;
    float semitoneErrorMidi;
    float correctedMidi;
    
    // Note name params
    std::string targetNoteName = "C";
    std::string aNoteNames[12] = {
        "C", "C#", "D", "D#", "E", "F", 
        "F#", "G", "G#", "A", "A#", "B"
    };

    double sampleRate;
};