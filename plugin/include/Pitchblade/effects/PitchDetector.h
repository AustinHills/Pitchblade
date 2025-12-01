/**
 * Author: Hayley Spellicy-Ryan
 * PitchDetector class
 * -------------------
 * Fundamental frequency detector for monophonic audio. 
 * Uses the pYIN algorithm.
 */

 #pragma once
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_devices/juce_audio_devices.h>
 #include <juce_dsp/juce_dsp.h> 
 #include <cmath>

 /**
  * Public interface class for testing
  */
 class IPitchDetector{
 public:
    virtual ~IPitchDetector() = default;
    virtual void prepare(double, int, double) = 0;
    virtual void processBlock(const juce::AudioBuffer<float>&) = 0;
    virtual float getCurrentPitch() = 0;
    virtual float getCurrentMidiNote() = 0;
 };

 /**
  * @brief Candidate object for determining best pitch
  * @details 
  * Has pitch, probability, and cost properties.
  * Probability is likelihood that it is the actual note.
  * Cost is how much it takes to get from the previous  note to this.
  */
 struct PitchCandidate{
    float pitch;
    float probability;
    float cost;
 };
 
 class PitchDetector : public IPitchDetector{
    public:
        // Constructor
        PitchDetector(int, float);
        PitchDetector(int);
        PitchDetector();

        // Prepare detector
        void prepare(double, int, double) override;

        // Process a block of audio
        void processBlock(const juce::AudioBuffer<float>&) override;
        void processFrame(const std::vector<float>&);

        float getCurrentPitch() override;
        float getSemitoneError();
        float getCurrentNote();
        float getCurrentMidiNote() override;
        std::string getCurrentNoteName();

        // Destructor
        ~PitchDetector();

    private:
        void difference(const std::vector<float>&);
        void cumulative();
        int absoluteThreshold();
        float calculateRMS(const std::vector<float>&);
        float convertLagToPitch(float);     // Helper function for YIN
        float parabolicMinimum(int);        // Helper function for Viterbi
        std::vector<std::pair<int, float>> findPitchCandidates();
        float processViterbi(std::vector<std::pair<int, float>>&);
        void prepareFFT(int);               // Helper function for FFT

        float currentPitch;                 // Pitch of most recent sample batch in Hz
        double sampleRate;                  // Sample rate
        int windowSize;                     // YIN algorithm: interval i to 2W
        int yinBufferSize;                  // YIN algorithm: W, on sum j = t + 1 to t + W
        std::vector<float> yinBuffer;       // YIN buffer
        std::vector<float> circularBuffer;  // Accumulative buffer of samples in Process Block
        int circularIdx;                    // Start position in circular buffer
        int hopSize;                        // Amount to jump fwd by. Creates overlapping frames.
        int samplesUntilHop;                // Decrease lag: do not hop until this threshold is met
        std::vector<float> windowFunction;  // Hann window
        std::vector<float> circularFrame;   // Circular Buffer * Windowing Function in Process Block
        std::vector<float> r;               // Running sum for difference function
        float currentAmp;                   // Amplitude tracker for RMS cutoff
        float ampThreshold;                 // Threshold for RMS cutoff
        float referencePitch;               // Pitch that notes are tuned to
        std::vector<std::pair<int, float>> rawCandidates;// Likely pitch per frame for pYIN
        float voiceThreshold;                            // Min threshold for a freq to be considered voiced
        std::vector<PitchCandidate> previousCandidates;
        std::vector<PitchCandidate> currentCandidates;
        float transitionCost = 15.f;                     // Penalty for changing pitch

        std::unique_ptr<juce::dsp::FFT> forwardFFT;      // Contain FFT info
        std::vector<float> fftTemp;                      // Temporary buffer for intermediate FFT calculations
        std::vector<float> cumulativeSquare;             // Buffer to store x^2 operations

        std::string noteNames[12] = {
                "A", "A#", "B", "C", "C#", "D", 
                "D#", "E", "F", "F#", "G", "G#"
            };
 };