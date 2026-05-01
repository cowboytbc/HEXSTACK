#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

JUCE_BEGIN_IGNORE_WARNINGS_MSVC(4127 4244 4267 4702)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wzero-as-null-pointer-constant", "-Wconversion", "-Wshadow")
#include <signalsmith-stretch/signalsmith-stretch.h>
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
JUCE_END_IGNORE_WARNINGS_MSVC

class HexstackAudioProcessor : public juce::AudioProcessor
{
public:
    enum class TunerRangeMode
    {
        wide = 0,
        bass,
        guitar
    };

    struct TunerData
    {
        float frequencyHz { 0.0f };
        float centsOffset { 0.0f };
        float levelDb { -100.0f };
        int midiNote { -1 };
        bool hasSignal { false };
    };

    HexstackAudioProcessor();
    ~HexstackAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParametersState() { return parameters; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    bool loadUserIRFromFile(const juce::File& file);
    void loadInfernoCabIR();
    juce::String getCurrentIRName() const;
    bool isUsingExternalIR() const { return currentIRFile.existsAsFile(); }
    double getDebugSampleRate() const { return currentSampleRate; }
    TunerData getTunerData() const;
    void setTunerAnalysisEnabled(bool enabled);
    bool isTunerAnalysisEnabled() const;
    void setTunerOutputMuted(bool shouldMute);
    bool isTunerOutputMuted() const;
    void setTunerReferenceHz(float hz);
    float getTunerReferenceHz() const;
    void setTunerRangeMode(TunerRangeMode mode);
    TunerRangeMode getTunerRangeMode() const;
    bool saveHexPresetToFile(const juce::File& file, const juce::String& presetName);
    bool loadHexPresetFromFile(const juce::File& file);
    juce::String getActiveHexFilePath() const { return activeHexFilePath; }
    float getLimiterGainReductionDb() const { return limiterGainReductionDb.load(std::memory_order_relaxed); }

    // ── MIDI Learn ────────────────────────────────────────────────────────────
    void armMidiLearn(const juce::String& paramId);
    void disarmMidiLearn();
    void clearMidiCC(const juce::String& paramId);
    void clearAllMidiCC();
    bool isMidiLearnArmed() const noexcept { return midiLearnArmedParamIndex.load(std::memory_order_acquire) >= 0; }
    juce::String getMidiLearnArmedParamId() const;
    int getMidiCCForParam(const juce::String& paramId) const;
    void applyPendingMidiCCsOnMessageThread();
    // ─────────────────────────────────────────────────────────────────────────

private:
    bool applyLoadedStateTree(const juce::ValueTree& tree);
    void prepareCabinetConvolution();
    void reloadStockCabConvolutions();
    void requestCabTransitionReset();
    void resetRuntimeVoicingState();
    void applyAutomatableControlState(bool forceReload);
    void processPitchEffect(juce::AudioBuffer<float>& buffer, float pitchShift, float pitchMix, float pitchWidth);
    float getHostSyncedDelayTimeMs(float manualDelayTimeMs);
    bool isFxPedalEnabled(size_t index) const;
    float getParameterValue(const char* paramId, float fallback) const;
    static std::vector<float> phaseAlignIRToReference(const std::vector<float>& reference, std::vector<float> candidate);
    static juce::MemoryBlock createWavFromSamples(const std::vector<float>& samples, int sampleRate, int numChannels = 1);
    void analyzeTunerInput(const juce::AudioBuffer<float>& buffer);
    float estimateFrequencyYIN(const float* samples, int numSamples);

    juce::AudioProcessorValueTreeState parameters;

    std::array<juce::dsp::IIR::Filter<float>, 2> inputHighPassFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> preampTightFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> bassShelfFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> midPeakFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> trebleShelfFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> presenceShelfFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> outputLowPassFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> cabLowCutFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> cabHighCutFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> thumpResonanceFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> depthShelfFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> wahBandPassFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, 10> postEqFilters;
    juce::dsp::Convolution primaryCabConvolution;
    juce::Reverb reverb;
    juce::AudioBuffer<float> pitchBuffer;
    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> reverbBuffer;
    juce::AudioBuffer<float> reverbPreDelayBuffer;
    int delayWritePosition { 0 };
    int pitchWritePosition { 0 };
    int reverbPreDelayWritePosition { 0 };
    std::array<float, 2> overdriveToneState { 0.0f, 0.0f };
    std::array<float, 2> overdriveTightState { 0.0f, 0.0f };
    juce::SmoothedValue<float> overdriveBypassSmooth;
    // Signalsmith phase-vocoder pitch shifter (MIT, header-only)
    signalsmith::stretch::SignalsmithStretch<float> pitchStretcher;
    std::vector<float> pitchTempIn, pitchTempOut;  // per-block scratch buffers
    // Chorus state
    std::array<float, 2> chorusLpfState { 0.0f, 0.0f };  // 1-pole LPF for BBD warmth
    // Chorus LFO state (width effect, independent of pitch shifter)
    float pitchChorusPhase  { 0.0f };
    float pitchChorusPhase2 { juce::MathConstants<float>::pi };
    std::array<float, 2> delayToneState { 0.0f, 0.0f };
    std::array<float, 2> delayHighPassState { 0.0f, 0.0f };
    std::array<float, 2> delayDuckEnvelope { 0.0f, 0.0f };
    std::array<float, 2> reverbHighPassState { 0.0f, 0.0f };
    std::array<float, 2> reverbLowPassState { 0.0f, 0.0f };
    std::array<float, 2> reverbDuckEnvelope { 0.0f, 0.0f };
    std::array<float, 2> lofiHighPassState { 0.0f, 0.0f };
    std::array<float, 2> lofiLowPassState { 0.0f, 0.0f };
    std::array<float, 2> lofiMidState { 0.0f, 0.0f };
    std::array<int, 2> lofiSampleHoldCounter { 0, 0 };
    std::array<float, 2> lofiHeldSample { 0.0f, 0.0f };
    std::array<float, 2> stfuEnvelope { 0.0f, 0.0f };
    std::array<float, 2> stfuGain { 1.0f, 1.0f };
    std::array<float, 2> limiterEnvelope { 0.0f, 0.0f };
    std::array<int, 2>   stfuHoldCounter { 0, 0 };  // per-channel hold-open sample counter
    std::array<float, 2> stfuHpfX { 0.0f, 0.0f };   // sidechain HPF: previous input sample
    std::array<float, 2> stfuHpfY { 0.0f, 0.0f };   // sidechain HPF: previous output sample
    std::array<float, 2> pickTransientState { 0.0f, 0.0f };

    juce::String currentIRName { "HEXSTACK OS V30 4X12 (SM57)" };
    juce::File currentIRFile;
    int currentProgramIndex { 0 };
    std::atomic<bool> presetChangeResetPending { false };
    int presetTransitionRampRemainingSamples { 0 };
    juce::String activeHexFilePath; // path of last loaded .hex file; used by editor to restore combo selection

    // ── MIDI Learn (lock-free audio-thread-safe design) ───────────────────────
    // Atomic array: index = MIDI CC number (0..127), value = index into
    // kMidiLearnableParamIDs[], or -1 for no mapping.
    // paramToCCIndex[paramIdx] = CC number assigned to that param (-1 = none).
    // Multiple params CAN share the same CC (one physical knob → many params).
    std::array<std::atomic<int>, 64> paramToCCIndex;
    // -1 = not armed; 0..N = index of param waiting for its first CC.
    std::atomic<int> midiLearnArmedParamIndex { -1 };
    // Lock-free FIFO: audio thread pushes {cc, normValue}; message thread drains.
    static constexpr int kMidiCCFifoSize = 64;
    struct PendingCCEvent { int cc; float normValue; };
    juce::AbstractFifo midiCCFifo { kMidiCCFifoSize };
    PendingCCEvent midiCCFifoData[kMidiCCFifoSize];
    // ─────────────────────────────────────────────────────────────────────────

    double currentSampleRate { 44100.0 };
    int currentMaxBlockSize { 512 };
    int currentNumChannels { 2 };

    std::vector<float> tunerCaptureBuffer;
    std::vector<float> tunerWindowBuffer;
    std::vector<float> yinDiffBuf;       // pre-allocated: avoids audio-thread heap alloc
    std::vector<float> yinNormBuf;       // same
    int tunerCaptureWritePos { 0 };
    int tunerCaptureValidSamples { 0 };
    int tunerSamplesSinceLastAnalysis { 0 };
    float tunerSmoothedFrequencyHz { 0.0f };
    int tunerLastMidiNote { -1 };
    int tunerStableFrames { 0 };
    int tunerSilenceFrames { 0 };
    int   tunerCommittedMidiNote   { -1 };
    float tunerRawFreqHistory[3]   {};      // 3-slot circular median buffer
    int   tunerRawFreqHistoryPos   { 0 };
    int   tunerRawFreqHistoryCount { 0 };

    std::atomic<float> tunerFrequencyHz { 0.0f };
    std::atomic<float> tunerCentsOffset { 0.0f };
    std::atomic<float> tunerLevelDb { -100.0f };
    std::atomic<int> tunerMidiNote { -1 };
    std::atomic<bool> tunerHasSignal { false };
    std::atomic<bool> tunerAnalysisEnabled { false };
    std::atomic<bool> tunerOutputMuted { false };
    std::atomic<float> tunerReferenceHz { 440.0f };
    std::atomic<int> tunerRangeMode { static_cast<int>(TunerRangeMode::wide) };
    std::atomic<float> limiterGainReductionDb { 0.0f };

    int lastPrimaryMicParamIndex { -1 };
    int lastTunerReferenceIndex { -1 };
    int lastTunerRangeParamIndex { -1 };

    // Per-block gain smoothing — stores the value used at the end of the previous block
    // so each new block can ramp from that value to the current target, eliminating zipper noise.
    float prevAmpInputGainLinear  { 0.01f };
    float prevPreampGain          { 4.4f };
    float prevStage2Gain          { 3.1f };
    float prevPowerAmpDrive       { 1.08f };
    float prevOutputGainLinear    { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HexstackAudioProcessor)
};
