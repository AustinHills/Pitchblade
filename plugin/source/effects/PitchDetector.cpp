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
    circularBuffer.assign(windowSize, 0.0f);
    r.assign(yinBufferSize + 1, 0.0f);
    yinBuffer.assign(yinBufferSize + 1, 0.0f);
    windowFunction.assign(windowSize, 0.0f);

    circularIdx = 0;

    // Allocate memory for Viterbi processing
    int maxCandidates = yinBufferSize / 2;  // Local minima cannot be greater than half buffer size
    previousCandidates.reserve(maxCandidates);
    currentCandidates.reserve(maxCandidates);

    // Set hop size to fraction of window size. Set higher for more resolution, lower for better CPU
    hopSize = windowSize / hopSizeDenominator;
    samplesUntilHop = hopSize;                    // Initialize hops to start at highest and count down

    // Define Hann window
    for (int i = 0; i < windowSize; ++i) {
        windowFunction[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (windowSize - 1)));
    }

    // Prepare FFT
    prepareFFT(windowSize);

    // Clear last group of pitch candidates, room for new ones
    previousCandidates.clear();
    currentCandidates.clear();
 }


 /**
  * @brief FFT preparation handler for difference function
  * @param windowSize
  * @details Initialize FFT buffer to handle
  * autocorrelation and energy terms. 
  * d(tau) = sum(x_(j)^2) + sum(x_(j+tau)^2) - 2 * sum(x_j * x_(j + tau))
  * where    ^^ energy terms                    ^^ autocorrelation
  */
 void PitchDetector::prepareFFT(int windowSize){
    // Determine magnitude to multiple 2^n size
    int magnitude = 0;
    int fftsize = windowSize * 2;
    while((1 << magnitude) < fftsize) magnitude++;

    // Construct FFT with magnitude
    forwardFFT = std::make_unique<juce::dsp::FFT>(magnitude);

    // Create fft workspace buffer that's twice as big as that
    fftTemp.resize(forwardFFT->getSize() * 2);

    // Cumulative square holds energy term calculations
    cumulativeSquare.resize(windowSize + 1);
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
  * @details O(NlogN) loop to determine difference in frequency domain. 
  * Overlaps frequency with itself to find lowest possible difference between the same points
  * This should return the ideal waveform change
  */
 void PitchDetector::difference(const std::vector<float>& frame)
 {
    // 1. Calculate energy terms
    // sum(x^2) for every value in frame
    cumulativeSquare[0] = 0.0f;
    for(unsigned int i = 0; i < windowSize; ++i)
        cumulativeSquare[i+1] = cumulativeSquare[i] + (frame[i] * frame[i]);
    
    // 2. Calculate autocorrelation with FFT
    // 2 * sum(x_j * x_(j + tau))
    // Fill buffer and pad with zeroes
    int fftSize = forwardFFT->getSize();
    std::fill(fftTemp.begin(), fftTemp.end(), 0.0f);
    std::copy(frame.begin(), frame.end(), fftTemp.begin());

    // Forward transform Time->Frequency
    forwardFFT->performRealOnlyForwardTransform(fftTemp.data());

    // Compute power spectral density
    // Multiply complex number by its conjugate
    for(unsigned int i = 0; i < fftSize; ++i){
        // Extract info from Juce automatic transform
        float real = fftTemp[i * 2];
        float imaginary = fftTemp[i * 2 + 1];

        // Complex multiplication foil (a+bi)(a-bi) = a^2+b^2
        float power = real * real + imaginary * imaginary;
        
        fftTemp[i * 2] = power;
        fftTemp[i * 2 + 1] = 0.0f;
    }

    // Inverse transform Frequency->Time
    forwardFFT->performRealOnlyInverseTransform(fftTemp.data());

    // Combine terms according to YIN formula
    // d(tau) = energy(a) + energy(b) - 2*acf(tau)
    for(int tau = 0; tau < yinBufferSize; ++tau){
        // energy(a) = sum(x_j^2) from j=0 to W - 1 - tau
        // using cumulative sum: last - first
        float energyA = cumulativeSquare[windowSize - tau] - cumulativeSquare[0];

        // energy(b) = sum(x_(j + tau) ^2)
        // so, sum from tau to W
        float energyB = cumulativeSquare[windowSize] - cumulativeSquare[tau];

        // acf(tau) is inverse fft result
        float acf = fftTemp[tau];

        yinBuffer[tau] = energyA + energyB - 2 * acf;
    }

    // Quick check to handle null value at lag 0
    yinBuffer[0] = 1.0f; // this gets normalized out
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
    rawCandidates.clear();

    // Find all local minima below threshold
    for(int tau = 2; tau < yinBufferSize-1; ++tau){
        // If it is a local minimum, it is a candidate
        if(yinBuffer[tau] < yinBuffer[tau-1] && yinBuffer[tau] < yinBuffer[tau+1]){
            rawCandidates.emplace_back(tau, yinBuffer[tau]);
        }
    }

    // Fallback for silence: find global min
    if(rawCandidates.empty()){
        float minVal = std::numeric_limits<float>::max();
        int minTau = -1;
        for (int tau = 2; tau < yinBufferSize - 1; ++tau) {
            if (yinBuffer[tau] < minVal) {
                minVal = yinBuffer[tau];
                minTau = tau;
            }
        }
        if (minTau > 0) rawCandidates.emplace_back(minTau, minVal);
    }

    return rawCandidates;
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
    currentCandidates.clear(); 

    //1. use parabolic interpolation to get pitch-probability-cost objects for each candidate
    for(auto&p : rawCandidates){
        // quickly construct pitch candidate    
        currentCandidates.emplace_back();
        auto& c = currentCandidates.back();

        //parabolic interpolation
        float parabolicLag = parabolicMinimum(p.first);
        c.pitch = convertLagToPitch(parabolicLag);

        if(p.second < 0.001f) p.second = 0.001f;
        c.probability = 1.0f - p.second;
        if(c.probability < 0) c.probability = 0;

        c.cost = 0.0f;
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

    //4. Update state by changing name of buffer and treating prev as current
    std::swap(previousCandidates, currentCandidates);

    //5. Return ideal pitch + gate to cut the transients
    if(previousCandidates[bestCandidate_x].probability < voiceThreshold) return 0.f;
    return previousCandidates[bestCandidate_x].pitch;
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

 /**
  * @brief Helper function to extract pitch value given ideal lag phase
  */
 float PitchDetector::convertLagToPitch(float lag)
 {
    if (lag <= 0) return 0.0f;
    return static_cast<float>(sampleRate) / static_cast<float>(lag);
 }

 /**
  * @brief Getter for pitch
  */
 float PitchDetector::getCurrentPitch()
 {
    return currentPitch;
 }

 /**
  * @brief Getter for midi note value, integer
  */
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

 /**
  * @brief Getter for difference between reference pitch and detected pitch.
  * Use Pitch Corrector's getSemitoneError for difference between detected and target pitch.
  */
 float PitchDetector::getSemitoneError()
 {
    return getCurrentNote() - getCurrentPitch();
 }

 /**
  * @brief Getter for name of current note. Assumes reference pitch of 440Hz
  */
 std::string PitchDetector::getCurrentNoteName()
 {
    int index = (int)(getCurrentNote()) % 12;
    if (index < 0) index += 12;
    return noteNames[index];
 }