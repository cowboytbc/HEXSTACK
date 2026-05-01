#pragma once

#include <JuceHeader.h>
#include <unordered_map>
#include "PluginProcessor.h"

class HexstackAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit HexstackAudioProcessorEditor(HexstackAudioProcessor&);
    ~HexstackAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseUp(const juce::MouseEvent& event) override;

private:
    enum class ActiveTab
    {
        amp,
        tuner,
        fx,
        fxPedal
    };

    static constexpr int baseEditorWidth = 940;
    static constexpr int baseEditorHeight = 430;
    static constexpr int maxScaleFactor = 2;

    HexstackAudioProcessor& audioProcessor;
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::TooltipWindow tooltipWindow { this, 500 };
    juce::Image backgroundImage;
    juce::Image titleImage;
    juce::Image scaledTitleImage;
    juce::Rectangle<float> titleImageDrawBounds;
    juce::Image ampImage;
    juce::Image scaledAmpImage;

    juce::Label titleLabel;
    juce::Label irStatusLabel;
    juce::Slider cabLowCutSlider;
    juce::Slider cabHighCutSlider;
    juce::Label cabLowCutLabel;
    juce::Label cabHighCutLabel;

    juce::TextButton loadIRButton { "Load IR..." };
    juce::TextButton clearIRButton { "X" };
    juce::TextButton saveHexButton { "Save .hex" };
    juce::TextButton loadHexButton { "Load .hex" };
    juce::TextButton helpButton { "?" };
    juce::ToggleButton voicingRhythmButton { "RHYTHM" };
    juce::ToggleButton voicingLeadButton   { "LEAD" };
    juce::TextButton lofiButton { "LOFI" };
    juce::TextButton postEqEnableButton { "POST EQ" };
    juce::Slider lofiIntensityKnob;
    juce::Label lofiIntensityLabel;
    juce::Slider stfuKnob;
    juce::Label stfuLabel;
    juce::Slider tapeSatKnob;
    juce::Label tapeSatLabel;
    juce::Slider limiterKnob;
    juce::Label limiterLabel;
    juce::Label limiterGrLabel;
    juce::Slider makeupGainSlider;
    juce::Label makeupGainLabel;
    juce::TextButton ampTabButton { "AMP" };
    juce::TextButton tunerTabButton { "TUNER" };
    juce::TextButton fxTabButton { "FX" };
    juce::ComboBox presetCombo;
    juce::Label tunerPanelLabel;
    juce::Label fxPanelLabel;
    juce::TextButton fxBackButton { "Back to FX" };
    juce::Label tunerRefLabel;
    juce::ComboBox tunerRefCombo;
    juce::Label tunerRangeLabel;
    juce::ComboBox tunerRangeCombo;
    juce::Label tunerNoteLabel;
    juce::Label tunerFreqLabel;
    juce::Label tunerCentsLabel;
    juce::Slider tunerCentsMeter;
    juce::Rectangle<int> tunerMeterDrawBounds;
    juce::Rectangle<int> tunerInputLevelBarBounds;
    float tunerInputLevelSmoothedUi { -100.0f };
    juce::Image expPedalRedImage;
    juce::Image expPedalBlackImage;
    juce::Image pedalStompImage;
    std::array<juce::Rectangle<int>, 5> fxPedalDrawBounds;
    std::array<juce::Label, 5> fxPedalLabels;
    std::array<juce::Rectangle<int>, 5> fxPowerDrawBounds;
    std::array<bool, 5> fxPedalPowerOn { true, true, true, true, true };
    juce::Rectangle<int> fxPedalAreaBounds;
    juce::Rectangle<int> fxDetailPedalBounds;
    juce::Rectangle<int> fxDetailPowerBounds;
    int selectedFxPedalIndex { 0 };
    bool lastDelaySyncUiState { false };
    float tunerTargetCentsUi { 0.0f };
    float tunerDisplayCentsUi { 0.0f };
    bool tunerSignalPresentUi { false };
    bool tunerInTuneUi { false };
    float tunerGlowStrengthUi { 0.0f };
    std::unique_ptr<juce::FileChooser> irChooser;
    std::unique_ptr<juce::FileChooser> presetChooser;
    ActiveTab activeTab { ActiveTab::amp };

    // ── User hex preset history ───────────────────────────────────────────────
    struct UserHexEntry { juce::String name; juce::File file; };
    std::vector<UserHexEntry> userHexPresets;
    int activeUserHexIndex { -1 };   // -1 = built-in preset active
    int numBuiltInPresets  { 0 };
    juce::String lastKnownHexPath;   // processor's hex path as of last reconciliation tick

    void loadUserHexList();
    void saveUserHexList();
    void rebuildPresetCombo(int targetSelectedId = 0);
    void addOrRefreshUserHexEntry(const juce::String& name, const juce::File& file);
    // ─────────────────────────────────────────────────────────────────────────

    juce::Slider inputSlider;
    juce::Slider driveSlider;
    juce::Slider toneSlider;
    juce::Slider micDistanceSlider;
    juce::Slider micBlendSlider;
    juce::Slider outputSlider;
    juce::Slider depthSlider;
    juce::Slider mixSlider;
    std::array<juce::Slider, 10> postEqBandSliders;

    juce::Label inputLabel;
    juce::Label driveLabel;
    juce::Label toneLabel;
    juce::Label micDistanceLabel;
    juce::Label micBlendLabel;
    juce::Label outputLabel;
    juce::Label depthLabel;
    juce::Label mixLabel;
    std::array<juce::Label, 10> postEqBandLabels;

    juce::Slider fxPitchShiftSlider;
    juce::Slider fxPitchMixSlider;
    juce::Slider fxPitchWidthSlider;
    juce::Slider fxPitchExprHeelSlider;
    juce::Slider fxPitchExprToeSlider;
    juce::Slider fxWahFreqSlider;
    juce::Slider fxWahQSlider;
    juce::Slider fxWahMixSlider;
    juce::Slider fxWahExprHeelSlider;
    juce::Slider fxWahExprToeSlider;
    juce::Slider fxDriveAmountSlider;
    juce::Slider fxDriveToneSlider;
    juce::Slider fxDriveLevelSlider;
    juce::Slider fxDriveMixSlider;
    juce::Slider fxDriveTightSlider;
    juce::Slider fxDelayTimeSlider;
    juce::Slider fxDelayFeedbackSlider;
    juce::Slider fxDelayToneSlider;
    juce::Slider fxDelayMixSlider;
    juce::Slider fxDelayWidthSlider;
    juce::ToggleButton fxDelaySyncButton { "SYNC TO HOST" };
    juce::Slider fxReverbSizeSlider;
    juce::Slider fxReverbDampSlider;
    juce::Slider fxReverbMixSlider;
    juce::Slider fxReverbPredelaySlider;

    juce::Label fxPitchShiftLabel;
    juce::Label fxPitchMixLabel;
    juce::Label fxPitchWidthLabel;
    juce::Label fxPitchExprHeelLabel;
    juce::Label fxPitchExprToeLabel;
    juce::Label fxWahFreqLabel;
    juce::Label fxWahQLabel;
    juce::Label fxWahMixLabel;
    juce::Label fxWahExprHeelLabel;
    juce::Label fxWahExprToeLabel;
    juce::Label fxDriveAmountLabel;
    juce::Label fxDriveToneLabel;
    juce::Label fxDriveLevelLabel;
    juce::Label fxDriveMixLabel;
    juce::Label fxDriveTightLabel;
    juce::Label fxDelayTimeLabel;
    juce::Label fxDelayFeedbackLabel;
    juce::Label fxDelayToneLabel;
    juce::Label fxDelayMixLabel;
    juce::Label fxDelayWidthLabel;
    juce::Label fxReverbSizeLabel;
    juce::Label fxReverbDampLabel;
    juce::Label fxReverbMixLabel;
    juce::Label fxReverbPredelayLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> toneAttachment;
    std::unique_ptr<SliderAttachment> micDistanceAttachment;
    std::unique_ptr<SliderAttachment> micBlendAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> cabLowCutAttachment;
    std::unique_ptr<SliderAttachment> cabHighCutAttachment;
    std::unique_ptr<SliderAttachment> fxPitchShiftAttachment;
    std::unique_ptr<SliderAttachment> fxPitchMixAttachment;
    std::unique_ptr<SliderAttachment> fxPitchWidthAttachment;
    std::unique_ptr<SliderAttachment> fxPitchExprHeelAttachment;
    std::unique_ptr<SliderAttachment> fxPitchExprToeAttachment;
    std::unique_ptr<SliderAttachment> fxWahFreqAttachment;
    std::unique_ptr<SliderAttachment> fxWahQAttachment;
    std::unique_ptr<SliderAttachment> fxWahMixAttachment;
    std::unique_ptr<SliderAttachment> fxWahExprHeelAttachment;
    std::unique_ptr<SliderAttachment> fxWahExprToeAttachment;
    std::unique_ptr<SliderAttachment> fxDriveAmountAttachment;
    std::unique_ptr<SliderAttachment> fxDriveToneAttachment;
    std::unique_ptr<SliderAttachment> fxDriveLevelAttachment;
    std::unique_ptr<SliderAttachment> fxDriveMixAttachment;
    std::unique_ptr<SliderAttachment> fxDriveTightAttachment;
    std::unique_ptr<SliderAttachment> fxDelayTimeAttachment;
    std::unique_ptr<SliderAttachment> fxDelayFeedbackAttachment;
    std::unique_ptr<SliderAttachment> fxDelayToneAttachment;
    std::unique_ptr<SliderAttachment> fxDelayMixAttachment;
    std::unique_ptr<SliderAttachment> fxDelayWidthAttachment;
    std::unique_ptr<ButtonAttachment> fxDelaySyncAttachment;
    std::unique_ptr<SliderAttachment> fxReverbSizeAttachment;
    std::unique_ptr<SliderAttachment> fxReverbDampAttachment;
    std::unique_ptr<SliderAttachment> fxReverbMixAttachment;
    std::unique_ptr<SliderAttachment> fxReverbPredelayAttachment;
    std::unique_ptr<ComboBoxAttachment> tunerRefAttachment;
    std::unique_ptr<ComboBoxAttachment> tunerRangeAttachment;
    std::unique_ptr<ButtonAttachment> voicingLeadAttachment;
    std::unique_ptr<ButtonAttachment> lofiAttachment;
    std::unique_ptr<ButtonAttachment> postEqEnableAttachment;
    std::unique_ptr<SliderAttachment> lofiIntensityAttachment;
    std::unique_ptr<SliderAttachment> stfuAttachment;
    std::unique_ptr<SliderAttachment> tapeSatAttachment;
    std::unique_ptr<SliderAttachment> limiterAttachment;
    std::unique_ptr<SliderAttachment> makeupGainAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 10> postEqBandAttachments;

    void loadBackgroundImage();
    void loadTitleImage();
    void loadAmpImage();
    void loadFxPedalImages();
    void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupHorizontalSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setupEqBandSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);
    void refreshIRStatus();
    void refreshDelaySyncUi();
    void refreshPostEqControlState();
    void updateTabVisibility();
    void updateTunerDisplay();
    void syncFxPowerFromParameters();
    bool getFxPowerParameterValue(size_t index) const;
    void setFxPowerParameterValue(size_t index, bool enabled);
    void timerCallback() override;

    // ── MIDI Learn UI ─────────────────────────────────────────────────────────
    std::unordered_map<juce::Component*, juce::String> sliderParamMap;
    void registerMidiLearnSlider(juce::Slider& s, const char* paramId);
    void showMidiLearnContextMenu(juce::Component* target, const juce::String& paramId);
    void updateMidiLearnHighlights();
    void mouseDown(const juce::MouseEvent& e) override;
    // ─────────────────────────────────────────────────────────────────────────

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HexstackAudioProcessorEditor)
};
