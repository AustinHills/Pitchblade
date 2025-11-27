/**
 * Author: Hayley Spellicy-Ryan
 * 
 * Implements the pYIN algorithm to detect pitch.
 * Pitch is determined by the frequency of the waveform, which is the inverse of the period.
 * Detecting pitch relies on detecting the period.
 * 
 * The YIN algorithm overlaps a waveform with itself over a lag parameter 
 * and calculates a difference function to determine the most likely period.
 * 
 * The pYIN algorithm takes an array of likely periods and applies smoothing over time.
 * 
 */

 #include "Pitchblade/effects/PitchDetector.h"

 /**
  * @brief Detailed constructor with custom window size and reference pitch
  */
 PitchDetector::PitchDetector(int windowSize, float referencePitch):
    windowSize(windowSize),
    yinBufferSize(windowSize / 2),
    referencePitch(referencePitch),
    voiceThreshold(0.1f),
    ampThreshold(0.001f)
 {

 }

  /**
  * @brief Constructor with custom window size
  * @details Defaults reference pitch to 440 Hz, standard A4 in concert pitch.
  */
 PitchDetector::PitchDetector(int windowSize) : PitchDetector(windowSize, 440) {

 }

  /**
  * @brief Default constructor
  * @details
  * Defaults reference pitch to 440 Hz, standard A4 in concert pitch. 
  * Defaults window size to 1024. Higher values increase resolution, lower values increase speed.
  */
 PitchDetector::PitchDetector() : PitchDetector(1024, 440) {

 }

 /**
  * @brief Default destructor
  */
 PitchDetector::~PitchDetector()
 {

 }
 
 /**
  * @brief Prepare block to initialize detector
  */
 void PitchDetector::prepare(double sampleRate, int samplesPerBlock, double hopSizeDenominator = 4)
 {
    this->sampleRate = sampleRate;
    circularFrame.assign(windowSize, 0.0f);
    r.assign(yinBufferSize + 1, 0.0f);

    // Initialize circular buffer of size windowSize with empty floats
    for(int i = 0; i < windowSize; ++i)
        circularBuffer.push_back(0.0f);
    circularIdx = 0;
    
    // Initialize YIN buffer with all zeroes
    for(int i = 0; i < yinBufferSize; ++i)
        yinBuffer.push_back(0.0f);

    // Set hop size to fraction of window size. Set higher for more resolution, lower for better CPU
    hopSize = windowSize / hopSizeDenominator;
    samplesUntilHop = hopSize;                    // Initialize hops to start at highest and count down

    // Initialize Hann window
    for(int i = 0; i < windowSize; ++i)
        windowFunction.push_back(0.0f);
        
    // Define Hann window
    for (int i = 0; i < windowSize; ++i) {
        windowFunction[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (windowSize - 1)));
    }

    // Clear last group of pitch candidates, room for new ones
    pitchCandidates.clear();
 }

/**
 * @brief Handle pitch detection on incoming buffer
 * @param buffer The juce AudioBuffer that holds raw time domain audio data
 * @details Accumulates samples in internal circular buffer.
 * Passes these samples to frame processor to run YIN calculations
 * when the buffer is full with new data
 */
void PitchDetector::processBlock(const juce::AudioBuffer<float> &buffer)
 {
    // Set info pointers
    auto* bufferData = buffer.getReadPointer(0);
    auto bufferNumSamples = buffer.getNumSamples();  

    // Accumulate incoming samples in circular buffer
    for(int i = 0; i < bufferNumSamples; ++i){
        circularBuffer[circularIdx] = bufferData[i];
        circularIdx = (circularIdx + 1) % windowSize;

        samplesUntilHop--; // Decrement hop counter

        if(samplesUntilHop <= 0){
            // Handle wrap-around and apply windowing function
            int startIndex = (circularIdx - windowSize + windowSize) % windowSize;
            for (int i = 0; i < windowSize; ++i) {
                int index = (startIndex + i) % windowSize;
                circularFrame[i] = circularBuffer[index] * windowFunction[i];
            }

            // Pass to processor to calculate pYIN
            processFrame(circularFrame);

            // Reset counter
            samplesUntilHop += hopSize; 
        }
    }

 }

 /**
  * @brief Helper to handle pitch detection on individual frame
  * @param frame Individual frame of raw audio data with windowing function applied
  * @details Applies difference and cumulative, finds pitch candidates and applies
  * Viterbi algorithm to determine most likely pitch.
  */
 void PitchDetector::processFrame(const std::vector<float>& frame)
 {
    currentAmp = calculateRMS(frame);  // Check if amp is below threshold
    if(currentAmp < ampThreshold){
        currentPitch = 0.0f;           // Set pitch to 0
        previousCandidates.clear();    // Reset Viterbi
        return;
    }

    // Processing steps
    difference(frame);                          // Populate yinBuffer with difference function
    cumulative();                               // Apply cumulative mean to yinBuffer
    auto candidates = findPitchCandidates();    // Populate array with possible candidates for real pitch
    currentPitch = processViterbi(candidates);  // Determine most likely pitch
 }

 /**
  * @brief Apply difference function from YIN algorithm
  * @param frame Individual frame of raw audio data with windowing function applied
  * @details O(N^2) loop to determine difference in time domain. 
  * Overlaps frequency with itself to find lowest possible difference between the same points
  * This should return the ideal waveform change
  */
 void PitchDetector::difference(const std::vector<float>& frame)
 {
    // ACF at lag 0
    float sumSquares = 0.0f;
    for(int i = 0; i < windowSize; ++i){
        sumSquares += frame[i] * frame[i];  //from ACF = sum_{j=t+1}^{t+W}(x_j*x_{j+\tau})
    }

    // Running sum
    r[0] = sumSquares;

    // from DF(tau) = r_{\tau}(0) + r_{t + \tau}(0) - 2r_t(\tau)
    for(int tau = 1; tau < yinBufferSize; ++tau){
        // Calculate ACF sum for this lag
        float acf = 0.0f;
        for(int j = 0; j < windowSize - tau; ++j){
            acf += frame[j] * frame[j + tau]; //from ACF = sum_{j=t+1}^{t+W}(x_j*x_{j+\tau})
        }

        // Running sum: lag for prev, but delete oldest sample and add newest
        r[tau] = r[tau - 1] 
                - (frame[tau - 1] * frame[tau - 1]);

        // DF
        yinBuffer[tau] = r[0] + r[tau] - 2 * acf;
    }
    yinBuffer[0] = 1.0f; // Avoid div by 0
 }

 /**
  * @brief Apply normalization function from YIN algorithm
  * @details Cumulative mean normalization applied to YIN buffer.
  * Divide value by YIN sum, or clamp to reasonable value.
  */
 void PitchDetector::cumulative()
 {   
    // Running sum
    float r = 0;
    yinBuffer[0] = 1.0f; // Special case
    
    // Cumulative mean normalization
    for (int tau = 1; tau < yinBufferSize; ++tau) {
        r += yinBuffer[tau];
        if (r <= 0.0f)
        {
            yinBuffer[tau] = 1;
        } else {
            yinBuffer[tau] *= tau / r;
        }
    }  
 }

 /**
  * @brief Estimate optimization curve using parabolic interpretation for sample accuracy
  * @param tau X value of the time x amplitude vector, index of YIN buffer.
  * @details Helper function for Viterbi algorithm. Optimizes and approximates actual minimum.
  */
 float PitchDetector::parabolicMinimum(int tau)
 {
    if(tau <= 0 || tau >= yinBufferSize+1) return (float)tau;

    float x = (float) tau;  // for x = tau find nearest y to left and right
    float y1 = yinBuffer[tau - 1];
    float y2 = yinBuffer[tau];
    float y3 = yinBuffer[tau + 1];

    float denominator = 2 * (2* y2-y1-y3);
    if(std::abs(denominator) < 0.0001) return x;    // Avoid div by 0

    float delta = (y3 - y1) / denominator;
    return x + delta;
 }

 /**
  * @brief Fill pitch candidates vector with ideal candidates
  * @details Populate vector with any local minima below threshold
  * Or use global minimum if there is no local minimum
  */
 std::vector<std::pair<int, float>> PitchDetector::findPitchCandidates()
 {
    std::vector<std::pair<int, float>> pitchCandidates;
    
    // Find all local minima below threshold
    for(int tau = 2; tau < yinBufferSize-1; ++tau){
        // If it is a local minimum, it is a candidate
        if(yinBuffer[tau] < yinBuffer[tau-1] && yinBuffer[tau] < yinBuffer[tau+1]){
            pitchCandidates.emplace_back(tau, yinBuffer[tau]);
        }
    }

    // Fallback for silence: find global min
    if(pitchCandidates.empty()){
        float minVal = std::numeric_limits<float>::max();
        int minTau = -1;
        for (int tau = 2; tau < yinBufferSize - 1; ++tau) {
            if (yinBuffer[tau] < minVal) {
                minVal = yinBuffer[tau];
                minTau = tau;
            }
        }
        if (minTau > 0) pitchCandidates.emplace_back(minTau, minVal);
    }

    return pitchCandidates;
 }

 /**
  * @brief Find most likely pitch candidate given pitch context
  * @param rawCandidates Array of pitch candidates populated by the pitch candidate finder
  * @details DP algorithm for most probable hidden state sequence.
  * Useing parabolically interpolated pitch candidate objects and cost function,
  * find cheapest path from previous pitch to current,
  * and return ideal pitch with the cheapest path.
  */
 float PitchDetector::processViterbi(std::vector<std::pair<int, float>>& rawCandidates)
 {
    std::vector<PitchCandidate> currentCandidates;  

    //1. use parabolic interpolation to get pitch-probability-cost objects for each candidate
    for(auto&p : rawCandidates){
        PitchCandidate c;

        //parabolic interpolation
        float parabolicLag = parabolicMinimum(p.first);
        c.pitch = convertLagToPitch(parabolicLag);

        if(p.second < 0.001f) p.second = 0.001f;
        c.probability = 1.0f - p.second;
        if(c.probability < 0) c.probability = 0;

        c.cost = 0.0f;
        currentCandidates.push_back(c);
    }

    //safeguard against white noise
    if(currentCandidates.empty()){
        previousCandidates.clear();
        return 0.f; 
    }

    //2. Initialize if in initial state
    if(previousCandidates.empty()){
        previousCandidates = currentCandidates;
        auto best = std::max_element(currentCandidates.begin(), currentCandidates.end(),
            [](const PitchCandidate& a, const PitchCandidate& b){return a.probability < b.probability; });
        return best->pitch;
    }

    //3. Induction: cheapest path from prev to current
    // Cost function: (1 - prob) + transitionCost * log2(pitchdiff)
    int bestCandidate_x = 0;    //index of best candidate
    float minGlobalCost = std::numeric_limits<float>::max();    //spawn minimum cost at max

    for(int i = 0; i < currentCandidates.size(); ++i){
        float minPathCost = std::numeric_limits<float>::max(); // minimum for this particular path

        // Go through all previous notes to find the shortest path from current note to previous
        for(const auto&prev : previousCandidates){
            // Calculate distance between pitches
            float pitchRatio = currentCandidates[i].pitch / (prev.pitch + 0.001f);
            float dist = std::abs(std::log2(pitchRatio));

            // Penalize large distance, to cut transients
            float penalty = transitionCost * dist;

            // Cost to get to prev + jump penalty + unlikelihood of note (from step 1)
            float currentCost = prev.cost + penalty + (1.f - currentCandidates[i].probability);

            // Update minimum
            if(currentCost < minPathCost) minPathCost = currentCost;
        }

        // Handle last index
        currentCandidates[i].cost = minPathCost;

        // Update new minimum
        if(minPathCost < minGlobalCost){
            minGlobalCost = minPathCost;
            bestCandidate_x = i;
        }
    }

    //4. Update state
    previousCandidates = currentCandidates;

    //5. Return ideal pitch + gate to cut the transients
    if(currentCandidates[bestCandidate_x].probability < voiceThreshold) return 0.f;
    return currentCandidates[bestCandidate_x].pitch;
 }

 /**
  * @brief Determine whether amplitude is being applied
  * @param frame Raw audio data with windowing applied
  * @details Repetitive function from Juce API
  */
 float PitchDetector::calculateRMS(const std::vector<float>& frame)
 {
    float sumSquares = 0.0f;
    for (float sample : frame) {
        sumSquares += sample * sample;
    }
    return std::sqrt(sumSquares / frame.size());
 }

 float PitchDetector::convertLagToPitch(float lag)
 {
    if (lag <= 0) return 0.0f;
    return static_cast<float>(sampleRate) / static_cast<float>(lag);
 }

 float PitchDetector::getCurrentPitch()
 {
    return currentPitch;
 }

 float PitchDetector::getCurrentMidiNote()
{
    if (currentPitch <= 0.f) return 0.f;   
    return (int)(round(69.0f + 12.0f * log2(currentPitch / 440.0f)));
}

 /**
  * @brief Returns number of semitones above or below reference pitch
  * @details 
  * f = f₀ * 2^(n/12)
  * n = 12log_2(f / f₀)
  */
 float PitchDetector::getCurrentNote()
 {
    if (currentPitch <= 0.f) return 0.f;
    return 12 * std::log2(currentPitch / referencePitch);
 }

 float PitchDetector::getSemitoneError()
 {
    return getCurrentNote() - getCurrentPitch();
 }

 std::string PitchDetector::getCurrentNoteName()
 {
    int index = (int)(getCurrentNote()) % 12;
    if (index < 0) index += 12;
    return noteNames[index];
 }