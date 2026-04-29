#include "PluginEditor.h"
#include "BinaryData.h"

#include <cmath>
#include <unordered_map>

namespace ParamIDs
{
    static constexpr auto depth = "depth";
    static constexpr auto cabLowCut = "cabLowCut";
    static constexpr auto cabHighCut = "cabHighCut";
    static constexpr auto tunerReference = "tunerReference";
    static constexpr auto tunerRange = "tunerRange";
    static constexpr auto postEqEnabled = "postEqEnabled";
    static constexpr auto postEq31 = "postEq31";
    static constexpr auto postEq62 = "postEq62";
    static constexpr auto postEq125 = "postEq125";
    static constexpr auto postEq250 = "postEq250";
    static constexpr auto postEq500 = "postEq500";
    static constexpr auto postEq1k = "postEq1k";
    static constexpr auto postEq2k = "postEq2k";
    static constexpr auto postEq4k = "postEq4k";
    static constexpr auto postEq8k = "postEq8k";
    static constexpr auto postEq16k = "postEq16k";
    static constexpr auto fxPower1 = "fxPower1";
    static constexpr auto fxPower2 = "fxPower2";
    static constexpr auto fxPower3 = "fxPower3";
    static constexpr auto fxPower4 = "fxPower4";
    static constexpr auto fxPower5 = "fxPower5";
    static constexpr auto fxPitchShift = "fxPitchShift";
    static constexpr auto fxPitchMix = "fxPitchMix";
    static constexpr auto fxPitchWidth = "fxPitchWidth";
    static constexpr auto fxWahFreq = "fxWahFreq";
    static constexpr auto fxWahQ = "fxWahQ";
    static constexpr auto fxWahMix = "fxWahMix";
    static constexpr auto fxDriveAmount = "fxDriveAmount";
    static constexpr auto fxDriveTone = "fxDriveTone";
    static constexpr auto fxDriveLevel = "fxDriveLevel";
    static constexpr auto fxDriveMix = "fxDriveMix";
    static constexpr auto fxDriveTight = "fxDriveTight";
    static constexpr auto fxDelayTimeMs = "fxDelayTimeMs";
    static constexpr auto fxDelaySync = "fxDelaySync";
    static constexpr auto fxDelayFeedback = "fxDelayFeedback";
    static constexpr auto fxDelayTone = "fxDelayTone";
    static constexpr auto fxDelayMix = "fxDelayMix";
    static constexpr auto fxDelayWidth = "fxDelayWidth";
    static constexpr auto fxReverbSize = "fxReverbSize";
    static constexpr auto fxReverbDamp = "fxReverbDamp";
    static constexpr auto fxReverbMix = "fxReverbMix";
    static constexpr auto fxReverbPreDelayMs = "fxReverbPreDelayMs";
    static constexpr auto voicing = "voicing";
    static constexpr auto lofi = "lofi";
    static constexpr auto lofiIntensity = "lofiIntensity";
    static constexpr auto stfu = "stfu";
    static constexpr auto tapeSaturation = "tapeSaturation";
    static constexpr auto limiter = "limiter";
    static constexpr auto makeupGain = "makeupGain";
}

namespace
{
    constexpr std::array<const char*, 10> postEqParamIds {
        ParamIDs::postEq31,
        ParamIDs::postEq62,
        ParamIDs::postEq125,
        ParamIDs::postEq250,
        ParamIDs::postEq500,
        ParamIDs::postEq1k,
        ParamIDs::postEq2k,
        ParamIDs::postEq4k,
        ParamIDs::postEq8k,
        ParamIDs::postEq16k
    };

    constexpr std::array<const char*, 10> postEqDisplayNames {
        "31", "62", "125", "250", "500", "1K", "2K", "4K", "8K", "16K"
    };

    constexpr std::array<const char*, 8> delaySyncDisplayNames {
        "1/16",
        "1/8T",
        "1/8",
        "1/8D",
        "1/4",
        "1/4D",
        "1/2",
        "1 BAR"
    };

    int getDelaySyncDivisionIndex(float manualDelayTimeMs)
    {
        const float normalizedTime = juce::jlimit(0.0f,
                                                  1.0f,
                                                  (manualDelayTimeMs - 60.0f) / (900.0f - 60.0f));

        return juce::jlimit(0,
                            static_cast<int>(delaySyncDisplayNames.size()) - 1,
                            juce::roundToInt(normalizedTime * static_cast<float>(delaySyncDisplayNames.size() - 1)));
    }
}

namespace
{
    juce::Image loadImageFromMemory(const void* data, int size)
    {
        if (data == nullptr || size <= 0)
            return {};

        return juce::ImageFileFormat::loadFrom(data, size);
    }

    juce::Image trimTransparentImage(juce::Image image, uint8 alphaThreshold = 8, int padding = 0)
    {
        if (! image.isValid() || ! image.hasAlphaChannel())
            return image;

        int minX = image.getWidth();
        int minY = image.getHeight();
        int maxX = -1;
        int maxY = -1;

        for (int y = 0; y < image.getHeight(); ++y)
        {
            for (int x = 0; x < image.getWidth(); ++x)
            {
                if (image.getPixelAt(x, y).getAlpha() > alphaThreshold)
                {
                    minX = juce::jmin(minX, x);
                    minY = juce::jmin(minY, y);
                    maxX = juce::jmax(maxX, x);
                    maxY = juce::jmax(maxY, y);
                }
            }
        }

        if (maxX < minX || maxY < minY)
            return image;

        minX = juce::jmax(0, minX - padding);
        minY = juce::jmax(0, minY - padding);
        maxX = juce::jmin(image.getWidth() - 1, maxX + padding);
        maxY = juce::jmin(image.getHeight() - 1, maxY + padding);

        return image.getClippedImage({ minX, minY, maxX - minX + 1, maxY - minY + 1 });
    }

    std::vector<juce::File> getAssetSearchDirectories()
    {
        std::vector<juce::File> dirs;
        dirs.reserve(10);

        const auto addUniqueDir = [&dirs](const juce::File& dir)
        {
            if (! dir.isDirectory())
                return;

            const auto fullPath = dir.getFullPathName();
            const auto duplicate = std::any_of(dirs.begin(), dirs.end(), [&fullPath](const juce::File& existing)
            {
                return existing.getFullPathName().equalsIgnoreCase(fullPath);
            });

            if (! duplicate)
                dirs.push_back(dir);
        };

        const juce::File cwd = juce::File::getCurrentWorkingDirectory();
        const juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        const juce::File exeDir = exeFile.getParentDirectory();
        const juce::File bundleContentsDir = exeDir.getParentDirectory();

        addUniqueDir(cwd);
        addUniqueDir(exeDir);
        addUniqueDir(exeDir.getChildFile("Resources"));
        addUniqueDir(bundleContentsDir);
        addUniqueDir(bundleContentsDir.getChildFile("Resources"));
        addUniqueDir(bundleContentsDir.getParentDirectory());

        return dirs;
    }

    juce::Image loadImageFromSearchPaths(const juce::StringArray& fileNames,
                                         bool trimTransparency,
                                         int trimPadding = 0)
    {
        const auto searchDirs = getAssetSearchDirectories();

        for (const auto& dir : searchDirs)
        {
            for (const auto& fileName : fileNames)
            {
                const auto candidate = dir.getChildFile(fileName);
                if (! candidate.existsAsFile())
                    continue;

                if (auto stream = std::unique_ptr<juce::InputStream>(candidate.createInputStream()))
                {
                    auto image = juce::ImageFileFormat::loadFrom(*stream);
                    if (image.isValid())
                        return trimTransparency ? trimTransparentImage(std::move(image), 8, trimPadding)
                                                : image;
                }
            }
        }

        return {};
    }
}

HexstackAudioProcessorEditor::HexstackAudioProcessorEditor(HexstackAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    const auto disengagedButtonColour = juce::Colour::fromRGB(26, 26, 30);
    const auto engagedButtonColour = juce::Colour::fromRGB(196, 34, 48);

    constexpr double editorAspectRatio = static_cast<double>(baseEditorWidth)
                                       / static_cast<double>(baseEditorHeight);

    resizeConstrainer.setFixedAspectRatio(editorAspectRatio);
    resizeConstrainer.setSizeLimits(baseEditorWidth,
                                    baseEditorHeight,
                                    baseEditorWidth * maxScaleFactor,
                                    baseEditorHeight * maxScaleFactor);

    setConstrainer(&resizeConstrainer);

        constexpr double defaultScaleFactor = 4.0 / 3.0;
        setSize(juce::roundToInt(baseEditorWidth * defaultScaleFactor),
            juce::roundToInt(baseEditorHeight * defaultScaleFactor));
    setResizable(true, true);
    loadBackgroundImage();
    loadTitleImage();
    loadAmpImage();
    loadFxPedalImages();

    titleLabel.setText("HEXSTACK", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::FontOptions(36.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 18, 34));
    addAndMakeVisible(titleLabel);
    titleLabel.setVisible(! titleImage.isValid());

    irStatusLabel.setJustificationType(juce::Justification::centredLeft);
    irStatusLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    addAndMakeVisible(irStatusLabel);

    setupHorizontalSlider(cabLowCutSlider, cabLowCutLabel, "LOW CUT");
    setupHorizontalSlider(cabHighCutSlider, cabHighCutLabel, "HIGH CUT");
    cabLowCutSlider.setRange(20.0, 500.0, 0.01);
    cabHighCutSlider.setRange(1000.0, 20000.0, 0.01);
    cabLowCutSlider.setTooltip("Rolls off the low end of the cab tone.");
    cabHighCutSlider.setTooltip("Rolls off the top end of the cab tone.");

    loadIRButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(168, 14, 28).withAlpha(0.9f));
    loadIRButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    loadIRButton.setTooltip("Load a cabinet IR file.");
    loadIRButton.onClick = [this]
    {
        auto safeThis = juce::Component::SafePointer<HexstackAudioProcessorEditor>(this);

        constexpr auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;

        irChooser = std::make_unique<juce::FileChooser>("Load Impulse Response",
                                                         juce::File(),
                                                         "*.wav;*.aiff;*.aif;*.flac");

        irChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();

            if (file.existsAsFile() && ! safeThis->audioProcessor.loadUserIRFromFile(file))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "IR Load Failed",
                                                       "Could not load that impulse response file.");
            }

            safeThis->refreshIRStatus();
        });
    };
    addAndMakeVisible(loadIRButton);

    clearIRButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(108, 16, 24).withAlpha(0.92f));
    clearIRButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    clearIRButton.setTooltip("Remove external IR");
    clearIRButton.onClick = [this]
    {
        audioProcessor.loadInfernoCabIR();
        refreshIRStatus();
        resized();
        repaint();
    };
    addAndMakeVisible(clearIRButton);
    clearIRButton.setVisible(false);

    saveHexButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(126, 16, 28).withAlpha(0.90f));
    saveHexButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    saveHexButton.setTooltip("Export current tone as a .hex preset file.");
    saveHexButton.onClick = [this]
    {
        auto safeThis = juce::Component::SafePointer<HexstackAudioProcessorEditor>(this);
        constexpr auto flags = juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting;

        // Suggest a unique timestamped filename so the dialog doesn't default to
        // the last-used name (which was historically "Default").
        const auto suggestedFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                       .getChildFile("HEXSTACK Presets")
                                       .getChildFile("Tone_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".hex");

        presetChooser = std::make_unique<juce::FileChooser>("Save HEXSTACK Tone",
                                                             suggestedFile,
                                                             "*.hex");

        presetChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
                return;

            if (! file.hasFileExtension(".hex"))
                file = file.withFileExtension(".hex");

            // Use the filename the user typed in the save dialog as the preset name —
            // not the combo text (which would just be "Default").
            const auto presetName = file.getFileNameWithoutExtension();

            if (! safeThis->audioProcessor.saveHexPresetToFile(file, presetName))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Preset Save Failed",
                                                       "Could not save the .hex tone preset.");
                return;
            }

            // Update the combo to show the saved preset name immediately,
            // so the user doesn't have to reload to see it.
            safeThis->addOrRefreshUserHexEntry(presetName, file);
        });
    };
    addAndMakeVisible(saveHexButton);

    loadHexButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(126, 16, 28).withAlpha(0.90f));
    loadHexButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    loadHexButton.setTooltip("Load a shared .hex tone preset file.");
    loadHexButton.onClick = [this]
    {
        auto safeThis = juce::Component::SafePointer<HexstackAudioProcessorEditor>(this);
        constexpr auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;

        presetChooser = std::make_unique<juce::FileChooser>("Load HEXSTACK Tone",
                                                             juce::File(),
                                                             "*.hex");

        presetChooser->launchAsync(flags, [safeThis](const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();
            if (file == juce::File())
                return;

            if (! safeThis->audioProcessor.loadHexPresetFromFile(file))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Preset Load Failed",
                                                       "Could not load that .hex tone preset.");
                return;
            }

            // Always derive the display name from the filename — the embedded
            // presetName XML attribute was historically saved as "Default" so
            // it cannot be trusted.
            const juce::String presetName = file.getFileNameWithoutExtension();

            safeThis->addOrRefreshUserHexEntry(presetName, file);
            safeThis->refreshIRStatus();
            safeThis->resized();
            safeThis->repaint();
        });
    };
    addAndMakeVisible(loadHexButton);

    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(26, 26, 30));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha(0.70f));
    helpButton.setTooltip("Visit myinferno.online");
    helpButton.onClick = []
    {
        juce::URL("https://myinferno.online/").launchInDefaultBrowser();
    };
    addAndMakeVisible(helpButton);

    voicingRhythmButton.setTooltip("Rhythm voicing.");
    voicingRhythmButton.onClick = [this]()
    {
        voicingLeadButton.setToggleState(false, juce::sendNotification);
        voicingRhythmButton.setToggleState(true, juce::dontSendNotification);
    };
    addAndMakeVisible(voicingRhythmButton);

    voicingLeadButton.setTooltip("Lead voicing.");
    voicingLeadButton.onStateChange = [this]()
    {
        voicingRhythmButton.setToggleState(!voicingLeadButton.getToggleState(), juce::dontSendNotification);
    };
    addAndMakeVisible(voicingLeadButton);

    lofiButton.setClickingTogglesState(true);
    lofiButton.setColour(juce::TextButton::buttonColourId, disengagedButtonColour);
    lofiButton.setColour(juce::TextButton::buttonOnColourId, engagedButtonColour);
    lofiButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    lofiButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    lofiButton.setTooltip("Turns the amp into a megaphone-style voice.");
    lofiButton.onStateChange = [this]
    {
        const bool on = lofiButton.getToggleState();
        lofiIntensityKnob.setVisible(on);
        lofiIntensityLabel.setVisible(on);
    };
    addAndMakeVisible(lofiButton);

    postEqEnableButton.setClickingTogglesState(true);
    postEqEnableButton.setColour(juce::TextButton::buttonColourId, disengagedButtonColour);
    postEqEnableButton.setColour(juce::TextButton::buttonOnColourId, engagedButtonColour);
    postEqEnableButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    postEqEnableButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    postEqEnableButton.setTooltip("Enable or bypass the post-cab equalizer.");
    postEqEnableButton.onStateChange = [this]
    {
        refreshPostEqControlState();
    };
    addAndMakeVisible(postEqEnableButton);

    setupKnob(lofiIntensityKnob, lofiIntensityLabel, "INTENSITY");
    lofiIntensityKnob.setTooltip("Controls the intensity of the megaphone effect.");

    setupKnob(stfuKnob, stfuLabel, "GATE");
    stfuKnob.setTooltip("Silences unwanted noise and feedback between notes.");

    setupKnob(tapeSatKnob, tapeSatLabel, "TAPE SAT");
    tapeSatKnob.setTooltip("Adds warmth and harmonic color to your tone.");
    addAndMakeVisible(tapeSatKnob);
    addAndMakeVisible(tapeSatLabel);

    setupKnob(limiterKnob, limiterLabel, "LIMITER");
    limiterKnob.setTooltip("Output limiter. Controls peak output level.");
    addAndMakeVisible(limiterKnob);
    addAndMakeVisible(limiterLabel);

    limiterGrLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(14.0f).withStyle("Bold")));
    limiterGrLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 100, 60));
    limiterGrLabel.setJustificationType(juce::Justification::centred);
    limiterGrLabel.setText("-0.0", juce::dontSendNotification);
    limiterGrLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(limiterGrLabel);

    setupEqBandSlider(makeupGainSlider, makeupGainLabel, "MAKEUP");
    makeupGainSlider.setRange(-24.0, 24.0, 0.1);
    makeupGainSlider.setValue(0.0);
    makeupGainSlider.setTooltip("Makeup gain applied after the limiter (+/-24 dB).");
    makeupGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(makeupGainSlider);
    addAndMakeVisible(makeupGainLabel);

    auto setupTabButton = [disengagedButtonColour, engagedButtonColour](juce::TextButton& button)
    {
        button.setColour(juce::TextButton::buttonColourId, disengagedButtonColour);
        button.setColour(juce::TextButton::buttonOnColourId, engagedButtonColour);
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        button.setClickingTogglesState(true);
    };

    setupTabButton(ampTabButton);
    setupTabButton(tunerTabButton);
    setupTabButton(fxTabButton);

    ampTabButton.setToggleState(true, juce::dontSendNotification);
    ampTabButton.onClick = [this] { activeTab = ActiveTab::amp; updateTabVisibility(); };
    tunerTabButton.onClick = [this] { activeTab = ActiveTab::tuner; updateTabVisibility(); };
    fxTabButton.onClick = [this] { activeTab = ActiveTab::fx; updateTabVisibility(); };

    addAndMakeVisible(ampTabButton);
    addAndMakeVisible(tunerTabButton);
    addAndMakeVisible(fxTabButton);

    presetCombo.setJustificationType(juce::Justification::centredLeft);
    presetCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
    presetCombo.setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
    presetCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(150, 20, 32).withAlpha(0.82f));
    presetCombo.setTooltip("Select a preset.");

    numBuiltInPresets = audioProcessor.getNumPrograms();
    loadUserHexList();
    rebuildPresetCombo();

    // Restore active selection — prefer the loaded hex file if the processor
    // recorded one (e.g. after a DAW session restore via setStateInformation).
    {
        const auto hexPath = audioProcessor.getActiveHexFilePath();
        bool restored = false;
        if (hexPath.isNotEmpty())
        {
            for (int i = 0; i < static_cast<int>(userHexPresets.size()); ++i)
            {
                if (userHexPresets[static_cast<size_t>(i)].file.getFullPathName() == hexPath)
                {
                    activeUserHexIndex = i;
                    presetCombo.setSelectedId(numBuiltInPresets + i + 1, juce::dontSendNotification);
                    restored = true;
                    break;
                }
            }
        }
        if (! restored)
            presetCombo.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    }
    // Initialise the reconciliation baseline so the first timer tick doesn't
    // treat the initial state as a DAW-restore change.
    lastKnownHexPath = audioProcessor.getActiveHexFilePath();

    presetCombo.onChange = [this]
    {
        const int selectedId = presetCombo.getSelectedId();
        if (selectedId <= 0)
            return;

        if (selectedId <= numBuiltInPresets)
        {
            // Built-in preset
            activeUserHexIndex = -1;
            lastKnownHexPath = {};
            audioProcessor.setCurrentProgram(selectedId - 1);
            refreshIRStatus();
            resized();
        }
        else
        {
            // User hex preset
            const int userIdx = selectedId - numBuiltInPresets - 1;
            if (userIdx >= 0 && userIdx < static_cast<int>(userHexPresets.size()))
            {
                const auto& entry = userHexPresets[static_cast<size_t>(userIdx)];
                if (audioProcessor.loadHexPresetFromFile(entry.file))
                {
                    activeUserHexIndex = userIdx;
                    refreshIRStatus();
                    resized();
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                           "Preset Load Failed",
                                                           "Could not load \"" + entry.name + "\".\n"
                                                           "The file may have been moved or deleted.");
                    // Leave the combo on this item so the user can see which one failed.
                }
            }
        }

        refreshPostEqControlState();
        refreshIRStatus();
        repaint();
    };

    addAndMakeVisible(presetCombo);

    tunerPanelLabel.setText("MUTED FOR TUNING", juce::dontSendNotification);
    tunerPanelLabel.setJustificationType(juce::Justification::centred);
    tunerPanelLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(235, 84, 96).withAlpha(0.96f));
    tunerPanelLabel.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    addAndMakeVisible(tunerPanelLabel);

    tunerRefLabel.setText("Ref", juce::dontSendNotification);
    tunerRefLabel.setJustificationType(juce::Justification::centredRight);
    tunerRefLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.95f));
    tunerRefLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(tunerRefLabel);

    tunerRefCombo.setJustificationType(juce::Justification::centred);
    tunerRefCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
    tunerRefCombo.setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
    tunerRefCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(120, 120, 128).withAlpha(0.7f));
    tunerRefCombo.addItem("432 Hz", 1);
    tunerRefCombo.addItem("440 Hz", 2);
    tunerRefCombo.addItem("442 Hz", 3);
    tunerRefCombo.setTooltip("Reference tuning frequency for A4.");
    addAndMakeVisible(tunerRefCombo);

    tunerRangeLabel.setText("Range", juce::dontSendNotification);
    tunerRangeLabel.setJustificationType(juce::Justification::centredRight);
    tunerRangeLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.95f));
    tunerRangeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(tunerRangeLabel);

    tunerRangeCombo.setJustificationType(juce::Justification::centred);
    tunerRangeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.45f));
    tunerRangeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
    tunerRangeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(120, 120, 128).withAlpha(0.7f));
    tunerRangeCombo.addItem("Wide", 1);
    tunerRangeCombo.addItem("Bass", 2);
    tunerRangeCombo.addItem("Guitar", 3);
    tunerRangeCombo.setTooltip("Detection range optimized for instrument.");
    addAndMakeVisible(tunerRangeCombo);

    tunerNoteLabel.setJustificationType(juce::Justification::centred);
    tunerNoteLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke);
    tunerNoteLabel.setFont(juce::FontOptions(44.0f, juce::Font::bold));
    addAndMakeVisible(tunerNoteLabel);

    tunerFreqLabel.setJustificationType(juce::Justification::centred);
    tunerFreqLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.92f));
    tunerFreqLabel.setFont(juce::FontOptions(18.0f, juce::Font::plain));
    addAndMakeVisible(tunerFreqLabel);

    tunerCentsLabel.setJustificationType(juce::Justification::centred);
    tunerCentsLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.92f));
    tunerCentsLabel.setFont(juce::FontOptions(16.0f, juce::Font::plain));
    addAndMakeVisible(tunerCentsLabel);

    tunerCentsMeter.setSliderStyle(juce::Slider::LinearHorizontal);
    tunerCentsMeter.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tunerCentsMeter.setRange(-50.0, 50.0, 0.1);
    tunerCentsMeter.setEnabled(false);
    tunerCentsMeter.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(18, 18, 20));
    tunerCentsMeter.setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(80, 12, 20));
    tunerCentsMeter.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(230, 50, 65));
    tunerCentsMeter.setVisible(false);
    addAndMakeVisible(tunerCentsMeter);

    fxPanelLabel.setText("FX CHAIN", juce::dontSendNotification);
    fxPanelLabel.setJustificationType(juce::Justification::centred);
    fxPanelLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.9f));
    fxPanelLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(fxPanelLabel);

    fxBackButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(110, 18, 28).withAlpha(0.88f));
    fxBackButton.setColour(juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    fxBackButton.setTooltip("Return to the full FX board.");
    fxBackButton.onClick = [this]
    {
        activeTab = ActiveTab::fx;
        updateTabVisibility();
    };
    addAndMakeVisible(fxBackButton);

    const std::array<juce::String, 5> pedalNames {
        "PITCH", "WAH", "OVERDRIVE", "REVERB", "DELAY"
    };

    for (size_t i = 0; i < pedalNames.size(); ++i)
    {
        auto& label = fxPedalLabels[i];
        label.setText(pedalNames[i], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.92f));
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    }

    setupKnob(inputSlider, inputLabel, "Input");
    setupKnob(driveSlider, driveLabel, "Drive");
    setupKnob(toneSlider, toneLabel, "Bass");
    setupKnob(micDistanceSlider, micDistanceLabel, "Mid");
    setupKnob(micBlendSlider, micBlendLabel, "Treble");
    setupKnob(outputSlider, outputLabel, "Presence");
    setupKnob(depthSlider, depthLabel, "Depth");
    setupKnob(mixSlider, mixLabel, "Master");

    for (size_t i = 0; i < postEqBandSliders.size(); ++i)
    {
        setupEqBandSlider(postEqBandSliders[i], postEqBandLabels[i], postEqDisplayNames[i]);
        postEqBandSliders[i].setRange(-12.0, 12.0, 0.01);
        postEqBandSliders[i].setDoubleClickReturnValue(true, 0.0);
        postEqBandSliders[i].setTextValueSuffix(" dB");
        postEqBandSliders[i].setNumDecimalPlacesToDisplay(1);

        juce::String tooltip = "Post-cab EQ band at ";
        tooltip << postEqDisplayNames[i];
        tooltip << ((i <= 4) ? " Hz." : ".");
        postEqBandSliders[i].setTooltip(tooltip);
    }

    setupKnob(fxPitchShiftSlider, fxPitchShiftLabel, "Shift");
    setupKnob(fxPitchMixSlider, fxPitchMixLabel, "Mix");
    setupKnob(fxPitchWidthSlider, fxPitchWidthLabel, "Width");
    setupKnob(fxWahFreqSlider, fxWahFreqLabel, "Sweep");
    setupKnob(fxWahQSlider, fxWahQLabel, "Q");
    setupKnob(fxWahMixSlider, fxWahMixLabel, "Mix");
    setupKnob(fxDriveAmountSlider, fxDriveAmountLabel, "Drive");
    setupKnob(fxDriveToneSlider, fxDriveToneLabel, "Tone");
    setupKnob(fxDriveLevelSlider, fxDriveLevelLabel, "Level");
    setupKnob(fxDriveMixSlider, fxDriveMixLabel, "Mix");
    setupKnob(fxDriveTightSlider, fxDriveTightLabel, "Tight");
    setupKnob(fxDelayTimeSlider, fxDelayTimeLabel, "Time");
    setupKnob(fxDelayFeedbackSlider, fxDelayFeedbackLabel, "Feedback");
    setupKnob(fxDelayToneSlider, fxDelayToneLabel, "Tone");
    setupKnob(fxDelayMixSlider, fxDelayMixLabel, "Mix");
    setupKnob(fxDelayWidthSlider, fxDelayWidthLabel, "Width");
    fxDelaySyncButton.setColour(juce::ToggleButton::textColourId, juce::Colours::whitesmoke);
    fxDelaySyncButton.setColour(juce::ToggleButton::tickColourId, juce::Colour::fromRGB(210, 34, 52));
    fxDelaySyncButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour::fromRGB(88, 88, 96));
    fxDelaySyncButton.setTooltip("Sync delay time to host tempo.");
    addAndMakeVisible(fxDelaySyncButton);
    setupKnob(fxReverbSizeSlider, fxReverbSizeLabel, "Size");
    setupKnob(fxReverbDampSlider, fxReverbDampLabel, "Damp");
    setupKnob(fxReverbMixSlider, fxReverbMixLabel, "Mix");
    setupKnob(fxReverbPredelaySlider, fxReverbPredelayLabel, "PreDelay");

    fxPitchShiftSlider.setRange(-24.0, 24.0, 1.0);
    fxPitchMixSlider.setRange(0.0, 1.0, 0.001);
    fxPitchWidthSlider.setRange(0.0, 1.0, 0.001);
    fxWahFreqSlider.setRange(0.0, 1.0, 0.001);
    fxWahQSlider.setRange(0.4, 8.0, 0.01);
    fxWahMixSlider.setRange(0.0, 1.0, 0.001);
    fxDriveAmountSlider.setRange(0.0, 1.0, 0.001);
    fxDriveToneSlider.setRange(0.0, 1.0, 0.001);
    fxDriveLevelSlider.setRange(-12.0, 12.0, 0.01);
    fxDriveMixSlider.setRange(0.0, 1.0, 0.001);
    fxDriveTightSlider.setRange(0.0, 1.0, 0.001);
    fxDelayTimeSlider.setRange(60.0, 900.0, 1.0);
    fxDelayFeedbackSlider.setRange(0.0, 0.92, 0.001);
    fxDelayToneSlider.setRange(0.0, 1.0, 0.001);
    fxDelayMixSlider.setRange(0.0, 1.0, 0.001);
    fxDelayWidthSlider.setRange(0.0, 1.0, 0.001);
    fxReverbSizeSlider.setRange(0.0, 1.0, 0.001);
    fxReverbDampSlider.setRange(0.0, 1.0, 0.001);
    fxReverbMixSlider.setRange(0.0, 1.0, 0.001);
    fxReverbPredelaySlider.setRange(0.0, 80.0, 1.0);

    fxPitchShiftSlider.setTooltip("Changes pitch amount in semitones.");
    fxPitchMixSlider.setTooltip("Blends dry and the mono pitch-shifted signal.");
    fxPitchWidthSlider.setTooltip("Controls stereo width and movement.");
    fxWahFreqSlider.setTooltip("Controls the wah sweep position (main wah effect).");
    fxWahQSlider.setTooltip("Sets the sharpness of the wah effect.");
    fxWahMixSlider.setTooltip("Blends dry signal and wah effect.");
    fxDriveAmountSlider.setTooltip("Sets overdrive gain amount.");
    fxDriveToneSlider.setTooltip("Sets overdrive brightness.");
    fxDriveLevelSlider.setTooltip("Sets overdrive output level.");
    fxDriveMixSlider.setTooltip("Blends clean and overdriven signal.");
    fxDriveTightSlider.setTooltip("Shapes the low-end character of the overdrive.");
    fxDelayTimeSlider.setTooltip("Sets the delay repeat time, or rhythmic division when sync is enabled.");
    fxDelayFeedbackSlider.setTooltip("Sets how many repeats are heard.");
    fxDelayToneSlider.setTooltip("Sets brightness of delay repeats.");
    fxDelayMixSlider.setTooltip("Blends dry signal and delay repeats.");
    fxDelayWidthSlider.setTooltip("Sets stereo width of delay repeats.");
    fxReverbSizeSlider.setTooltip("Sets room size.");
    fxReverbDampSlider.setTooltip("Controls the brightness of the reverb tail.");
    fxReverbMixSlider.setTooltip("Blends dry signal and reverb.");
    fxReverbPredelaySlider.setTooltip("Sets the delay before the reverb tail begins.");

    inputSlider.setTooltip("Input gain before the amp.");
    driveSlider.setTooltip("Main preamp gain amount.");
    toneSlider.setTooltip("Low-end response of the amp.");
    micDistanceSlider.setTooltip("Midrange body and cut.");
    micBlendSlider.setTooltip("High-end brightness and attack.");
    outputSlider.setTooltip("Upper harmonic presence control.");
    depthSlider.setTooltip("Adds low-end weight and thump.");
    mixSlider.setTooltip("Final output level.");

    cabLowCutSlider.textFromValueFunction = [](double value)
    {
        return juce::String(juce::roundToInt(value)) + " Hz";
    };
    cabLowCutSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.retainCharacters("0123456789.").getDoubleValue();
    };
    cabHighCutSlider.textFromValueFunction = [](double value)
    {
        if (value >= 1000.0)
            return juce::String(value / 1000.0, value >= 10000.0 ? 1 : 2) + " kHz";

        return juce::String(juce::roundToInt(value)) + " Hz";
    };
    cabHighCutSlider.valueFromTextFunction = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        const bool khz = trimmed.containsIgnoreCase("k");
        const double numeric = trimmed.retainCharacters("0123456789.").getDoubleValue();
        return khz ? numeric * 1000.0 : numeric;
    };

    fxDelayTimeSlider.textFromValueFunction = [this](double value)
    {
        const auto* syncValue = audioProcessor.getParametersState().getRawParameterValue(ParamIDs::fxDelaySync);
        const bool syncEnabled = syncValue != nullptr && syncValue->load() >= 0.5f;

        if (syncEnabled)
            return juce::String(delaySyncDisplayNames[static_cast<size_t>(getDelaySyncDivisionIndex(static_cast<float>(value)))]);

        return juce::String(juce::roundToInt(value)) + " ms";
    };

    fxDelayTimeSlider.valueFromTextFunction = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.contains("/") || trimmed.containsIgnoreCase("bar") || trimmed.containsIgnoreCase("t") || trimmed.containsIgnoreCase("d"))
        {
            for (size_t i = 0; i < delaySyncDisplayNames.size(); ++i)
            {
                if (trimmed.equalsIgnoreCase(delaySyncDisplayNames[i]))
                {
                    const float normalized = (delaySyncDisplayNames.size() > 1)
                        ? static_cast<float>(i) / static_cast<float>(delaySyncDisplayNames.size() - 1)
                        : 0.0f;
                    return 60.0 + normalized * (900.0 - 60.0);
                }
            }
        }

        return trimmed.retainCharacters("0123456789.").getDoubleValue();
    };

    fxDelaySyncButton.onStateChange = [this]
    {
        refreshDelaySyncUi();
    };

    auto& state = audioProcessor.getParametersState();
    inputAttachment = std::make_unique<SliderAttachment>(state, "input", inputSlider);
    driveAttachment = std::make_unique<SliderAttachment>(state, "gain", driveSlider);
    toneAttachment = std::make_unique<SliderAttachment>(state, "bass", toneSlider);
    micDistanceAttachment = std::make_unique<SliderAttachment>(state, "mids", micDistanceSlider);
    micBlendAttachment = std::make_unique<SliderAttachment>(state, "treble", micBlendSlider);
    outputAttachment = std::make_unique<SliderAttachment>(state, "presence", outputSlider);
    depthAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::depth, depthSlider);
    mixAttachment = std::make_unique<SliderAttachment>(state, "master", mixSlider);
    cabLowCutAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::cabLowCut, cabLowCutSlider);
    cabHighCutAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::cabHighCut, cabHighCutSlider);
    fxPitchShiftAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxPitchShift, fxPitchShiftSlider);
    fxPitchMixAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxPitchMix, fxPitchMixSlider);
    fxPitchWidthAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxPitchWidth, fxPitchWidthSlider);
    fxWahFreqAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxWahFreq, fxWahFreqSlider);
    fxWahQAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxWahQ, fxWahQSlider);
    fxWahMixAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxWahMix, fxWahMixSlider);
    fxDriveAmountAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDriveAmount, fxDriveAmountSlider);
    fxDriveToneAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDriveTone, fxDriveToneSlider);
    fxDriveLevelAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDriveLevel, fxDriveLevelSlider);
    fxDriveMixAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDriveMix, fxDriveMixSlider);
    fxDriveTightAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDriveTight, fxDriveTightSlider);
    fxDelayTimeAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDelayTimeMs, fxDelayTimeSlider);
    fxDelaySyncAttachment = std::make_unique<ButtonAttachment>(state, ParamIDs::fxDelaySync, fxDelaySyncButton);
    fxDelayFeedbackAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDelayFeedback, fxDelayFeedbackSlider);
    fxDelayToneAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDelayTone, fxDelayToneSlider);
    fxDelayMixAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDelayMix, fxDelayMixSlider);
    fxDelayWidthAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxDelayWidth, fxDelayWidthSlider);
    fxReverbSizeAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxReverbSize, fxReverbSizeSlider);
    fxReverbDampAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxReverbDamp, fxReverbDampSlider);
    fxReverbMixAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxReverbMix, fxReverbMixSlider);
    fxReverbPredelayAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::fxReverbPreDelayMs, fxReverbPredelaySlider);
    tunerRefAttachment = std::make_unique<ComboBoxAttachment>(state, ParamIDs::tunerReference, tunerRefCombo);
    tunerRangeAttachment = std::make_unique<ComboBoxAttachment>(state, ParamIDs::tunerRange, tunerRangeCombo);
    voicingLeadAttachment = std::make_unique<ButtonAttachment>(state, ParamIDs::voicing, voicingLeadButton);
    voicingRhythmButton.setToggleState(!voicingLeadButton.getToggleState(), juce::dontSendNotification);
    lofiAttachment = std::make_unique<ButtonAttachment>(state, ParamIDs::lofi, lofiButton);
    postEqEnableAttachment = std::make_unique<ButtonAttachment>(state, ParamIDs::postEqEnabled, postEqEnableButton);
    lofiIntensityAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::lofiIntensity, lofiIntensityKnob);
    stfuAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::stfu, stfuKnob);
    tapeSatAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::tapeSaturation, tapeSatKnob);
    limiterAttachment  = std::make_unique<SliderAttachment>(state, ParamIDs::limiter, limiterKnob);
    makeupGainAttachment = std::make_unique<SliderAttachment>(state, ParamIDs::makeupGain, makeupGainSlider);

    for (size_t i = 0; i < postEqBandAttachments.size(); ++i)
        postEqBandAttachments[i] = std::make_unique<SliderAttachment>(state, postEqParamIds[i], postEqBandSliders[i]);

    fxDelayTimeSlider.textFromValueFunction = [this](double value)
    {
        const auto* syncValue = audioProcessor.getParametersState().getRawParameterValue(ParamIDs::fxDelaySync);
        const bool syncEnabled = syncValue != nullptr && syncValue->load() >= 0.5f;

        if (syncEnabled)
            return juce::String(delaySyncDisplayNames[static_cast<size_t>(getDelaySyncDivisionIndex(static_cast<float>(value)))]);

        return juce::String(juce::roundToInt(value)) + " ms";
    };

    fxDelayTimeSlider.valueFromTextFunction = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.contains("/") || trimmed.containsIgnoreCase("bar") || trimmed.containsIgnoreCase("t") || trimmed.containsIgnoreCase("d"))
        {
            for (size_t i = 0; i < delaySyncDisplayNames.size(); ++i)
            {
                if (trimmed.equalsIgnoreCase(delaySyncDisplayNames[i]))
                {
                    const float normalized = (delaySyncDisplayNames.size() > 1)
                        ? static_cast<float>(i) / static_cast<float>(delaySyncDisplayNames.size() - 1)
                        : 0.0f;
                    return 60.0 + normalized * (900.0 - 60.0);
                }
            }
        }

        return trimmed.retainCharacters("0123456789.").getDoubleValue();
    };
    fxPitchWidthSlider.textFromValueFunction = [](double value)
    {
        if (std::abs(value) < 0.0005)
            return juce::String("Off");

        return juce::String(value, 2);
    };
    fxPitchWidthSlider.valueFromTextFunction = [](const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.equalsIgnoreCase("off"))
            return 0.0;

        return trimmed.retainCharacters("0123456789.").getDoubleValue();
    };
    fxDelayTimeSlider.updateText();
    fxPitchWidthSlider.updateText();
    cabLowCutSlider.updateText();
    cabHighCutSlider.updateText();

    syncFxPowerFromParameters();
    refreshDelaySyncUi();
    refreshPostEqControlState();

    updateTunerDisplay();
    updateTabVisibility();
    refreshIRStatus();
    resized();
    startTimerHz(30);
}

HexstackAudioProcessorEditor::~HexstackAudioProcessorEditor()
{
    audioProcessor.setTunerAnalysisEnabled(false);
    audioProcessor.setTunerOutputMuted(false);
}

void HexstackAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(200, 22, 38));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(48, 14, 18));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(244, 68, 86));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(114, 10, 20).withAlpha(0.9f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(14, 6, 8));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(236, 185, 192));
    addAndMakeVisible(label);
}

void HexstackAudioProcessorEditor::setupHorizontalSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 16);
    slider.setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(200, 22, 38));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(248, 88, 104));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(24, 10, 14));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(114, 10, 20).withAlpha(0.9f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(14, 6, 8));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(236, 185, 192));
    addAndMakeVisible(label);
}

void HexstackAudioProcessorEditor::setupEqBandSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 18);
    slider.setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(200, 22, 38));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(248, 88, 104));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(24, 10, 14));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(114, 10, 20).withAlpha(0.9f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(14, 6, 8));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(236, 185, 192));
    addAndMakeVisible(label);
}

void HexstackAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto drawTitleImage = [this, &g]()
    {
        if (! scaledTitleImage.isValid())
            return;

        const juce::DropShadow redShadow(juce::Colour::fromRGB(235, 20, 40).withAlpha(0.84f),
                                         16,
                                         juce::Point<int>(0, 3));

        juce::Graphics::ScopedSaveState scopedState(g);
        g.addTransform(juce::AffineTransform::translation(titleImageDrawBounds.getX(),
                                                          titleImageDrawBounds.getY()));
        redShadow.drawForImage(g, scaledTitleImage);
        g.drawImageAt(scaledTitleImage, 0, 0);
    };

    const auto drawTunerTitle = [this, &g]()
    {
        if (activeTab != ActiveTab::tuner)
            return;

        const auto bounds = tunerPanelLabel.getBounds().toFloat();
        if (bounds.isEmpty())
            return;

        auto plateBounds = bounds.reduced(18.0f, juce::jmax(12.0f, bounds.getHeight() * 0.31f));
        const float cornerSize = juce::jmin(22.0f, plateBounds.getHeight() * 0.48f);

        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(plateBounds.translated(0.0f, 3.0f), cornerSize);

        g.setColour(juce::Colour::fromRGB(18, 18, 22).withAlpha(0.84f));
        g.fillRoundedRectangle(plateBounds, cornerSize);

        g.setColour(juce::Colour::fromRGB(230, 234, 240).withAlpha(0.10f));
        g.drawRoundedRectangle(plateBounds.reduced(0.8f), cornerSize - 1.0f, 1.0f);

        const auto textBounds = plateBounds.toNearestInt();
        const auto shadowColour = juce::Colours::black.withAlpha(0.72f);
        g.setFont(tunerPanelLabel.getFont());
        g.setColour(shadowColour);
        g.drawFittedText(tunerPanelLabel.getText(), textBounds.translated(0, 3), juce::Justification::centred, 1);
        g.drawFittedText(tunerPanelLabel.getText(), textBounds.translated(1, 2), juce::Justification::centred, 1);

        g.setColour(tunerPanelLabel.findColour(juce::Label::textColourId));
        g.drawFittedText(tunerPanelLabel.getText(), textBounds, juce::Justification::centred, 1);
    };

    g.fillAll(juce::Colour::fromRGB(8, 8, 11));

    auto uiBounds = getLocalBounds().toFloat().reduced(10.0f);
    const float controlsHeight = juce::jmax(210.0f, uiBounds.getHeight() * 0.58f);
    auto topVisualArea = uiBounds.removeFromTop(uiBounds.getHeight() - controlsHeight);
    auto controlsArea = uiBounds;

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(controlsArea, 10.0f);

    if (backgroundImage.isValid())
    {
        const auto targetW = topVisualArea.getWidth();
        const auto targetH = topVisualArea.getHeight();
        const auto imageW = static_cast<float>(backgroundImage.getWidth());
        const auto imageH = static_cast<float>(backgroundImage.getHeight());

        const float coverScale = juce::jmax(targetW / imageW, targetH / imageH);
        const float drawW = imageW * coverScale;
        const float drawH = imageH * coverScale;
        const float drawX = topVisualArea.getX() + (targetW - drawW) * 0.5f;
        const float drawY = topVisualArea.getY() + (targetH - drawH) * 0.5f;

        g.drawImage(backgroundImage,
                juce::roundToInt(drawX),
                juce::roundToInt(drawY),
                juce::roundToInt(drawW),
                juce::roundToInt(drawH),
                0,
                0,
                backgroundImage.getWidth(),
                backgroundImage.getHeight());
    }
    else
    {
        juce::ColourGradient metalGrad(
            juce::Colour::fromRGB(42, 42, 49), topVisualArea.getX(), topVisualArea.getY(),
            juce::Colour::fromRGB(20, 20, 24), topVisualArea.getRight(), topVisualArea.getBottom(), false);
        g.setGradientFill(metalGrad);
        g.fillRoundedRectangle(topVisualArea, 12.0f);
    }

    if (activeTab == ActiveTab::tuner && ! tunerMeterDrawBounds.isEmpty())
    {
        const auto meterBounds = tunerMeterDrawBounds.toFloat();
        const auto tunerVisualBounds = meterBounds.getUnion(tunerNoteLabel.getBounds().toFloat())
                                                .getUnion(tunerFreqLabel.getBounds().toFloat())
                                                .getUnion(tunerCentsLabel.getBounds().toFloat())
                                                .expanded(12.0f, 10.0f);

        g.setColour(juce::Colour::fromRGB(10, 10, 12));
        g.fillRoundedRectangle(meterBounds, 9.0f);
        g.setColour(juce::Colour::fromRGB(236, 240, 246).withAlpha(0.16f));
        g.drawRoundedRectangle(meterBounds.reduced(0.6f), 8.6f, 1.1f);

        auto zone = meterBounds.reduced(6.0f, 8.0f);
        const float centerX = zone.getCentreX();

        const auto fillZone = [&g, &zone](float x0, float x1, juce::Colour c)
        {
            g.setColour(c.withAlpha(0.75f));
            g.fillRect(juce::Rectangle<float>(x0, zone.getY(), x1 - x0, zone.getHeight()));
        };

        fillZone(zone.getX(), zone.getX() + zone.getWidth() * 0.25f, juce::Colour::fromRGB(188, 28, 42));
        fillZone(zone.getX() + zone.getWidth() * 0.25f, zone.getX() + zone.getWidth() * 0.42f, juce::Colour::fromRGB(204, 154, 28));
        fillZone(zone.getX() + zone.getWidth() * 0.42f, zone.getX() + zone.getWidth() * 0.58f, juce::Colour::fromRGB(40, 202, 108));
        fillZone(zone.getX() + zone.getWidth() * 0.58f, zone.getX() + zone.getWidth() * 0.75f, juce::Colour::fromRGB(204, 154, 28));
        fillZone(zone.getX() + zone.getWidth() * 0.75f, zone.getRight(), juce::Colour::fromRGB(188, 28, 42));

        const float clampedCents = juce::jlimit(-50.0f, 50.0f, tunerDisplayCentsUi);
        const auto centerZone = juce::Rectangle<float>(zone.getX() + zone.getWidth() * 0.42f,
                                                       zone.getY(),
                                                       zone.getWidth() * 0.16f,
                                                       zone.getHeight());
        if (tunerSignalPresentUi && tunerInTuneUi)
        {
            const float centerAccuracy = 1.0f - juce::jlimit(0.0f, 1.0f, std::abs(clampedCents) / 5.0f);
            const float pulsePhase = static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.0105);
            const float pulseAmount = 0.5f + 0.5f * std::sin(pulsePhase);
            const float pulseBlend = juce::jmap(centerAccuracy, 0.55f, 1.0f, 0.0f, 1.0f);
            const float glowStrength = juce::jlimit(0.18f,
                                                    1.0f,
                                                    0.18f + tunerGlowStrengthUi * (0.42f + 0.38f * centerAccuracy)
                                                        + pulseAmount * 0.22f * pulseBlend);
            const auto centerGlow = juce::Colour::fromRGB(68, 240, 124);
            g.setColour(centerGlow.withAlpha((0.08f + 0.18f * centerAccuracy) * glowStrength));
            g.fillRoundedRectangle(centerZone.expanded(8.0f, 5.0f), 10.0f);
            g.setColour(centerGlow.withAlpha((0.18f + 0.24f * centerAccuracy) * glowStrength));
            g.fillRoundedRectangle(centerZone.expanded(3.0f, 2.0f), 8.0f);
            g.setColour(centerGlow.withAlpha(0.74f + 0.22f * centerAccuracy));
            g.fillRoundedRectangle(centerZone, 6.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(0.25f));
        for (int i = -5; i <= 5; ++i)
        {
            const float x = centerX + (zone.getWidth() * 0.5f) * (static_cast<float>(i) / 5.0f);
            g.drawLine(x, zone.getY(), x, zone.getBottom(), (i == 0 ? 2.4f : 1.0f));
        }

        g.setColour(juce::Colour::fromRGB(248, 248, 252).withAlpha(0.78f));
        g.drawLine(centerX, zone.getY() - 1.0f, centerX, zone.getBottom() + 1.0f, 3.8f);

        juce::Path centerNotch;
        centerNotch.startNewSubPath(centerX - 5.5f, zone.getY() - 4.5f);
        centerNotch.lineTo(centerX + 5.5f, zone.getY() - 4.5f);
        centerNotch.lineTo(centerX, zone.getY() + 5.8f);
        centerNotch.closeSubPath();
        g.setColour(juce::Colour::fromRGB(12, 12, 14).withAlpha(0.92f));
        g.fillPath(centerNotch);
        g.setColour(juce::Colour::fromRGB(240, 244, 252).withAlpha(0.96f));
        g.strokePath(centerNotch, juce::PathStrokeType(1.2f));

        const float normalized = (clampedCents + 50.0f) / 100.0f;
        const float indicatorX = zone.getX() + zone.getWidth() * normalized;
        const float indicatorY = zone.getCentreY();

        const float absC = std::abs(clampedCents);
        const juce::Colour indicatorGlowColour = (absC <= 5.0f)
            ? juce::Colour::fromRGB(58, 224, 104)
            : ((absC <= 15.0f)
                ? juce::Colour::fromRGB(226, 178, 26)
                : juce::Colour::fromRGB(220, 38, 48));
        const juce::Colour indicatorFillColour = juce::Colour::fromRGB(236, 244, 255);
        const juce::Colour indicatorOutlineColour = juce::Colour::fromRGB(12, 12, 14).withAlpha(0.96f);

        const float glowAlpha = tunerSignalPresentUi ? juce::jlimit(0.35f, 0.95f, 0.35f + tunerGlowStrengthUi * 0.6f) : 0.20f;
        g.setColour(indicatorGlowColour.withAlpha(glowAlpha * 0.22f));
        g.fillEllipse(indicatorX - 16.0f, indicatorY - 16.0f, 32.0f, 32.0f);
        g.setColour(indicatorGlowColour.withAlpha(glowAlpha * 0.55f));
        g.fillEllipse(indicatorX - 9.0f, indicatorY - 9.0f, 18.0f, 18.0f);

        const auto indicatorOutlineBounds = juce::Rectangle<float>(indicatorX - 3.2f,
                                       zone.getY() - 1.0f,
                           6.4f,
                                       zone.getHeight() + 2.0f);
        const auto indicatorFillBounds = indicatorOutlineBounds.reduced(1.4f, 1.0f);
        const auto indicatorCapOutlineBounds = juce::Rectangle<float>(indicatorX - 5.0f,
                          zone.getCentreY() - 5.0f,
                          10.0f,
                          10.0f);
        const auto indicatorCapFillBounds = indicatorCapOutlineBounds.reduced(1.9f, 1.9f);

        g.setColour(indicatorOutlineColour.withAlpha(tunerSignalPresentUi ? 0.92f : 0.48f));
        g.fillRoundedRectangle(indicatorOutlineBounds, 3.5f);
        g.fillEllipse(indicatorCapOutlineBounds);

        g.setColour(indicatorFillColour.withAlpha(tunerSignalPresentUi ? 0.98f : 0.62f));
        g.fillRoundedRectangle(indicatorFillBounds, 2.4f);
        g.fillEllipse(indicatorCapFillBounds);

        const auto drawLed = [&g](float x, float y, juce::Colour colour, bool on)
        {
            const float r = 5.0f;
            g.setColour(colour.withAlpha(on ? 0.95f : 0.20f));
            g.fillEllipse(x - r, y - r, 2.0f * r, 2.0f * r);
            if (on)
            {
                g.setColour(colour.withAlpha(0.28f));
                g.fillEllipse(x - 10.0f, y - 10.0f, 20.0f, 20.0f);
            }
        };

        const float ledY = meterBounds.getY() - 8.0f;
        const float ledStep = meterBounds.getWidth() / 4.0f;
        const bool redOn = tunerSignalPresentUi && (absC > 15.0f);
        const bool yellowOn = tunerSignalPresentUi && (absC > 5.0f && absC <= 15.0f);
        const bool greenOn = tunerSignalPresentUi && (absC <= 5.0f);

        drawLed(meterBounds.getX(), ledY, juce::Colour::fromRGB(220, 38, 48), redOn);
        drawLed(meterBounds.getX() + ledStep, ledY, juce::Colour::fromRGB(226, 178, 26), yellowOn);
        drawLed(meterBounds.getCentreX(), ledY, juce::Colour::fromRGB(58, 224, 104), greenOn);
        drawLed(meterBounds.getRight() - ledStep, ledY, juce::Colour::fromRGB(226, 178, 26), yellowOn);
        drawLed(meterBounds.getRight(), ledY, juce::Colour::fromRGB(220, 38, 48), redOn);
    }

    // Input level bar — always visible on tuner screen so the user can see
    // whether signal is actually arriving at the plugin.
    if (activeTab == ActiveTab::tuner && ! tunerInputLevelBarBounds.isEmpty())
    {
        const auto bar = tunerInputLevelBarBounds.toFloat();
        // Background track
        g.setColour(juce::Colour::fromRGB(10, 10, 12));
        g.fillRoundedRectangle(bar, 4.0f);
        g.setColour(juce::Colour::fromRGB(236, 240, 246).withAlpha(0.12f));
        g.drawRoundedRectangle(bar.reduced(0.6f), 3.6f, 1.0f);

        // Fill proportional to smoothed level (maps -60 → -6 dBFS to 0 → 1)
        const float fillFraction = juce::jlimit(0.0f, 1.0f,
            juce::jmap(tunerInputLevelSmoothedUi, -60.0f, -6.0f, 0.0f, 1.0f));
        if (fillFraction > 0.001f)
        {
            const auto fill = bar.reduced(2.0f, 2.0f).withWidth(bar.reduced(2.0f, 2.0f).getWidth() * fillFraction);
            const juce::Colour barColour = fillFraction > 0.8f
                ? juce::Colour::fromRGB(220, 38, 48)   // red: very hot
                : fillFraction > 0.4f
                    ? juce::Colour::fromRGB(58, 210, 100)  // green: healthy signal
                    : juce::Colour::fromRGB(80, 100, 200); // blue: weak signal
            g.setColour(barColour.withAlpha(0.82f));
            g.fillRoundedRectangle(fill, 3.0f);
        }

        // Label
        g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
        g.setColour(juce::Colours::whitesmoke.withAlpha(0.38f));
        g.drawFittedText("INPUT", tunerInputLevelBarBounds.withLeft(tunerInputLevelBarBounds.getRight() + 4),
                         juce::Justification::centredLeft, 1);
    }

    if (activeTab == ActiveTab::fx && ! fxPedalAreaBounds.isEmpty())
    {
        const std::array<const juce::Image*, 5> pedalOrder {
            &expPedalRedImage,
            &expPedalBlackImage,
            &pedalStompImage,
            &pedalStompImage,
            &pedalStompImage
        };

        for (size_t i = 0; i < pedalOrder.size(); ++i)
        {
            const auto& bounds = fxPedalDrawBounds[i];
            const auto* image = pedalOrder[i];
            const bool powerOn = fxPedalPowerOn[i];

            g.setColour(juce::Colour::fromRGB(12, 12, 14));
            g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

            if (image != nullptr && image->isValid())
            {
                g.drawImageWithin(*image,
                                  bounds.getX(),
                                  bounds.getY(),
                                  bounds.getWidth(),
                                  bounds.getHeight(),
                                  juce::RectanglePlacement::centred,
                                  false);
            }
            else
            {
                g.setColour(juce::Colours::whitesmoke.withAlpha(0.65f));
                g.drawFittedText("Missing PNG",
                                 bounds,
                                 juce::Justification::centred,
                                 2);
            }

            if (const auto powerBounds = fxPowerDrawBounds[i]; ! powerBounds.isEmpty())
            {
                const auto p = powerBounds.toFloat();
                const float glow = powerOn ? 0.85f : 0.24f;
                const auto colour = powerOn ? juce::Colour::fromRGB(64, 238, 108)
                                            : juce::Colour::fromRGB(190, 36, 46);

                g.setColour(colour.withAlpha(glow * 0.22f));
                g.fillEllipse(p.expanded(5.0f));

                g.setColour(juce::Colour::fromRGB(18, 18, 20));
                g.fillEllipse(p);

                g.setColour(colour.withAlpha(glow));
                g.drawEllipse(p.reduced(1.2f), 1.8f);

                g.setColour(colour.withAlpha(powerOn ? 0.95f : 0.55f));
                g.fillEllipse(p.reduced(5.0f));
            }
        }
    }

    if (activeTab == ActiveTab::fxPedal && ! fxDetailPowerBounds.isEmpty())
    {
        const size_t index = static_cast<size_t>(juce::jlimit(0,
                                                              static_cast<int>(fxPedalPowerOn.size()) - 1,
                                                              selectedFxPedalIndex));
        const bool powerOn = fxPedalPowerOn[index];
        const auto p = fxDetailPowerBounds.toFloat();
        const float glow = powerOn ? 0.85f : 0.24f;
        const auto colour = powerOn ? juce::Colour::fromRGB(64, 238, 108)
                                    : juce::Colour::fromRGB(190, 36, 46);

        g.setColour(colour.withAlpha(glow * 0.22f));
        g.fillEllipse(p.expanded(7.0f));

        g.setColour(juce::Colour::fromRGB(18, 18, 20));
        g.fillEllipse(p);

        g.setColour(colour.withAlpha(glow));
        g.drawEllipse(p.reduced(1.2f), 2.0f);

        g.setColour(colour.withAlpha(powerOn ? 0.95f : 0.55f));
        g.fillEllipse(p.reduced(5.0f));
    }

    // Always render AMP artwork last on the amp tab so nothing paints over it.
    if (activeTab == ActiveTab::amp && ampImage.isValid())
    {
        auto ampImageArea = topVisualArea.withTrimmedTop(topVisualArea.getHeight() * 0.12f)
                           .withTrimmedLeft(topVisualArea.getWidth() * 0.50f)
                           .withTrimmedBottom(topVisualArea.getHeight() * 0.02f)
                                       .reduced(1.0f, 0.0f);

        ampImageArea = ampImageArea.translated(topVisualArea.getWidth() * 0.05f, 0.0f)
                                 .withWidth(ampImageArea.getWidth() * 1.18f)
                                 .withHeight(ampImageArea.getHeight() * 1.08f)
                     .translated(topVisualArea.getWidth() * 0.06f,
                                             topVisualArea.getHeight() * 0.14f);

        g.setOpacity(1.0f);

        // Pre-scale with high-quality resampling (avoids blurry/pixelated downscale)
        const int drawW = juce::jmax(1, juce::roundToInt(ampImageArea.getWidth()));
        const int drawH = juce::jmax(1, juce::roundToInt(ampImageArea.getHeight()));
        if (! scaledAmpImage.isValid()
            || scaledAmpImage.getWidth()  != drawW
            || scaledAmpImage.getHeight() != drawH)
        {
            const float imgAspect  = static_cast<float>(ampImage.getWidth())
                                   / static_cast<float>(juce::jmax(1, ampImage.getHeight()));
            const float areaAspect = static_cast<float>(drawW) / static_cast<float>(drawH);
            int scaledW, scaledH;
            if (imgAspect > areaAspect) { scaledW = drawW; scaledH = juce::jmax(1, juce::roundToInt(drawW / imgAspect)); }
            else                        { scaledH = drawH; scaledW = juce::jmax(1, juce::roundToInt(drawH * imgAspect)); }
            scaledAmpImage = ampImage.rescaled(scaledW, scaledH, juce::Graphics::highResamplingQuality);
        }

        const int drawX = juce::roundToInt(ampImageArea.getCentreX() - scaledAmpImage.getWidth()  * 0.5f);
        const int drawY = juce::roundToInt(ampImageArea.getCentreY() - scaledAmpImage.getHeight() * 0.5f);
        g.drawImageAt(scaledAmpImage, drawX, drawY);
    }

    drawTunerTitle();

    // Render title last/top-most as well, except on the tuner tab.
    if (activeTab != ActiveTab::tuner)
        drawTitleImage();

}

void HexstackAudioProcessorEditor::loadFxPedalImages()
{
    expPedalRedImage = trimTransparentImage(loadImageFromMemory(BinaryData::EXP_PEDAL_RED_png,
                                                                BinaryData::EXP_PEDAL_RED_pngSize));
    expPedalBlackImage = trimTransparentImage(loadImageFromMemory(BinaryData::EXP_PEDAL_BLACK_png,
                                                                  BinaryData::EXP_PEDAL_BLACK_pngSize));
    pedalStompImage = trimTransparentImage(loadImageFromMemory(BinaryData::pedal_stomp_png,
                                                               BinaryData::pedal_stomp_pngSize));

    if (! expPedalRedImage.isValid())
        expPedalRedImage = loadImageFromSearchPaths({ "EXP PEDAL RED.png" }, true);

    if (! expPedalBlackImage.isValid())
        expPedalBlackImage = loadImageFromSearchPaths({ "EXP PEDAL BLACK.png" }, true);

    if (! pedalStompImage.isValid())
        pedalStompImage = loadImageFromSearchPaths({ "pedal stomp.png" }, true);
}

void HexstackAudioProcessorEditor::loadBackgroundImage()
{
    backgroundImage = loadImageFromMemory(BinaryData::BACKGROUND_jpeg,
                                          BinaryData::BACKGROUND_jpegSize);

    if (! backgroundImage.isValid())
        backgroundImage = loadImageFromSearchPaths({ "BACKGROUND.jpeg", "BACKGROUND.jpg", "BACKGROUND.png" }, false);
}

void HexstackAudioProcessorEditor::loadTitleImage()
{
    titleImage = trimTransparentImage(loadImageFromMemory(BinaryData::TITLE_png,
                                                          BinaryData::TITLE_pngSize),
                                      8,
                                      24);

    if (! titleImage.isValid())
        titleImage = loadImageFromSearchPaths({ "TITLE.png" }, true, 24);
}

void HexstackAudioProcessorEditor::loadAmpImage()
{
    ampImage = trimTransparentImage(loadImageFromMemory(BinaryData::AMP_png,
                                                        BinaryData::AMP_pngSize),
                                    0,
                                    0);

    if (! ampImage.isValid())
        ampImage = loadImageFromSearchPaths({ "AMP.png", "AMP.jpg", "AMP.jpeg" }, true);

    scaledAmpImage = {};
}

void HexstackAudioProcessorEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(baseEditorWidth);
    const auto S = [scale](int value)
    {
        return juce::jmax(1, juce::roundToInt(static_cast<float>(value) * scale));
    };

    auto area = getLocalBounds().reduced(S(14));

    // Help button — fixed at the bottom-right corner of the editor
    helpButton.setBounds(S(4), getHeight() - S(24) - S(4), S(28), S(24));

    const int controlsHeight = juce::jmax(S(272), static_cast<int>(area.getHeight() * 0.64f));
    auto topArea = area.removeFromTop(area.getHeight() - controlsHeight);
    auto controlsArea = area;

    auto topTabsArea = topArea.removeFromTop(S(24));
    const auto tunerGlobalControlsRow = topTabsArea;
    const auto visualAreaBelowTabs = topArea;
    loadIRButton.setBounds(topTabsArea.removeFromLeft(S(130)));
    {
        const int stfuWidth = S(100);
        const int stfuHeight = S(72);
        const int stfuX = loadIRButton.getBounds().getCentreX() - stfuWidth / 2;
        stfuKnob.setBounds(stfuX, loadIRButton.getBottom() + S(2), stfuWidth, stfuHeight);
        stfuLabel.setBounds(stfuX, loadIRButton.getBottom() + S(2) + stfuHeight, stfuWidth, S(18));
        const int tapeSatX = stfuX + stfuWidth + S(8);
        tapeSatKnob.setBounds(tapeSatX, loadIRButton.getBottom() + S(2), stfuWidth, stfuHeight);
        tapeSatLabel.setBounds(tapeSatX, loadIRButton.getBottom() + S(2) + stfuHeight, stfuWidth, S(18));
        const int limiterX = tapeSatX + stfuWidth + S(8);
        limiterKnob.setBounds(limiterX, loadIRButton.getBottom() + S(2), stfuWidth, stfuHeight);
        limiterLabel.setBounds(limiterX, loadIRButton.getBottom() + S(2) + stfuHeight, stfuWidth, S(18));
        const int limiterGrX = limiterX + stfuWidth / 2 - S(24);
        const int limiterGrY = loadIRButton.getBottom() + S(2) + stfuHeight / 2 - S(9);
        limiterGrLabel.setBounds(limiterGrX, limiterGrY, S(48), S(18));
        // Makeup gain vertical slider to the right of the limiter knob
        const int makeupX = limiterX + stfuWidth - S(26);
        const int makeupSliderH = stfuHeight + S(18); // same total height as knob+label
        makeupGainLabel.setBounds(makeupX, loadIRButton.getBottom() + S(2), S(52), S(14));
        makeupGainSlider.setBounds(makeupX, loadIRButton.getBottom() + S(2) + S(14), S(52), makeupSliderH - S(14));
    }
    topTabsArea.removeFromLeft(S(6));
    saveHexButton.setBounds(topTabsArea.removeFromLeft(S(98)));
    topTabsArea.removeFromLeft(S(6));
    loadHexButton.setBounds(topTabsArea.removeFromLeft(S(98)));
    auto tabsRight = topTabsArea.removeFromRight(S(330));
    auto lofiButtonBounds = tabsRight.removeFromLeft(S(86));
    lofiButton.setBounds(lofiButtonBounds);
    const auto lofiKnobX = lofiButtonBounds.getX();
    const auto lofiKnobY = lofiButtonBounds.getBottom() + S(2);
    lofiIntensityKnob.setBounds(lofiKnobX - S(7), lofiKnobY, S(100), S(72));
    lofiIntensityLabel.setBounds(lofiKnobX - S(7), lofiKnobY + S(72), S(100), S(18));
    tabsRight.removeFromLeft(S(10));
    ampTabButton.setBounds(tabsRight.removeFromLeft(S(70)));
    tabsRight.removeFromLeft(S(6));
    tunerTabButton.setBounds(tabsRight.removeFromLeft(S(82)));
    tabsRight.removeFromLeft(S(6));
    fxTabButton.setBounds(tabsRight.removeFromLeft(S(56)));

    presetCombo.setBounds(tunerGlobalControlsRow.withSizeKeepingCentre(S(220), S(24)));

    auto tunerGlobalLeft = tunerGlobalControlsRow;
    tunerRefLabel.setBounds(tunerGlobalLeft.removeFromLeft(S(38)));
    tunerRefCombo.setBounds(tunerGlobalLeft.removeFromLeft(S(84)));
    tunerGlobalLeft.removeFromLeft(S(10));
    tunerRangeLabel.setBounds(tunerGlobalLeft.removeFromLeft(S(52)));
    tunerRangeCombo.setBounds(tunerGlobalLeft.removeFromLeft(S(108)));

    topArea.removeFromTop(S(2));

    auto titleArea = topArea.reduced(S(6), S(6));

    if (titleImage.isValid())
    {
        const float imageW = static_cast<float>(titleImage.getWidth());
        const float imageH = static_cast<float>(titleImage.getHeight());
        const float imageAspect = imageW / juce::jmax(1.0f, imageH);

        auto fitArea = titleArea.toFloat().expanded(static_cast<float>(S(10)),
                                                    static_cast<float>(S(6)));
        float drawW = fitArea.getWidth();
        float drawH = drawW / imageAspect;

        if (drawH > fitArea.getHeight())
        {
            drawH = fitArea.getHeight();
            drawW = drawH * imageAspect;
        }

        const auto drawX = fitArea.getCentreX() - drawW * 0.5f;
        const auto drawY = fitArea.getCentreY() - drawH * 0.5f + static_cast<float>(S(24));

        titleImageDrawBounds = juce::Rectangle<float>(drawX, drawY, drawW, drawH);

        const int scaledW = juce::jmax(1, juce::roundToInt(drawW));
        const int scaledH = juce::jmax(1, juce::roundToInt(drawH));

        if (! scaledTitleImage.isValid()
            || scaledTitleImage.getWidth()  != scaledW
            || scaledTitleImage.getHeight() != scaledH)
        {
            scaledTitleImage = titleImage.rescaled(
                scaledW, scaledH, juce::Graphics::mediumResamplingQuality);
        }

        titleLabel.setVisible(false);
    }
    else
    {
        titleLabel.setVisible(true);
        scaledTitleImage = {};
        titleImageDrawBounds = {};
        titleLabel.setBounds(titleArea);
        titleLabel.setFont(juce::FontOptions(36.0f * scale, juce::Font::bold));
    }

    auto tunerTitleBounds = titleArea;
    if (! titleImageDrawBounds.isEmpty())
        tunerTitleBounds = titleImageDrawBounds.getSmallestIntegerContainer().expanded(S(8), S(6));

    tunerTitleBounds = tunerTitleBounds.translated(0, -S(8));

    tunerPanelLabel.setBounds(tunerTitleBounds);
    tunerPanelLabel.setFont(juce::FontOptions(22.0f * scale, juce::Font::bold));

    auto fullTabArea = controlsArea.reduced(S(6));

    if (activeTab == ActiveTab::amp)
    {
        auto ampArea = fullTabArea;

        ampArea.removeFromTop(S(4));
        auto ampHeader = ampArea.removeFromTop(S(24));
        auto clearIRArea = ampHeader.removeFromLeft(S(18));
        clearIRButton.setBounds(clearIRArea.reduced(0, S(2)));
        ampHeader.removeFromLeft(S(4));
        irStatusLabel.setBounds(ampHeader);
        irStatusLabel.setFont(juce::FontOptions(13.0f * scale, juce::Font::plain));

        auto postEqRow = ampArea.removeFromTop(S(34));
        ampArea.removeFromTop(S(4));

        // Voicing checkboxes — right-aligned in the postEqRow strip
        {
            const int voicingH = S(20);
            const int voicingW = S(76);
            const int voicingY = postEqRow.getY() + (postEqRow.getHeight() - voicingH) / 2;
            const int rightEdge = postEqRow.getRight() + S(30);
            voicingLeadButton.setBounds  (rightEdge - voicingW,            voicingY, voicingW, voicingH);
            voicingRhythmButton.setBounds(rightEdge - voicingW * 2 - S(4), voicingY, voicingW, voicingH);
        }

        auto ampPreviewArea = visualAreaBelowTabs.toFloat().withTrimmedTop(visualAreaBelowTabs.getHeight() * 0.12f)
                                                  .withTrimmedLeft(visualAreaBelowTabs.getWidth() * 0.50f)
                                                  .withTrimmedBottom(visualAreaBelowTabs.getHeight() * 0.02f)
                                                  .reduced(1.0f, 0.0f);

        ampPreviewArea = ampPreviewArea.translated(visualAreaBelowTabs.getWidth() * 0.05f, 0.0f)
                                     .withWidth(ampPreviewArea.getWidth() * 1.18f)
                                     .withHeight(ampPreviewArea.getHeight() * 1.08f)
                         .translated(visualAreaBelowTabs.getWidth() * 0.06f,
                                                 visualAreaBelowTabs.getHeight() * 0.14f);

        const int postEqButtonWidth = S(100);
        const int postEqButtonHeight = S(24);
        const auto titleReferenceBounds = titleImageDrawBounds.isEmpty()
            ? titleLabel.getBounds().toFloat()
            : titleImageDrawBounds;
        const int gapLeft = juce::roundToInt(titleReferenceBounds.getRight()) + S(14);
        const int gapRight = juce::roundToInt(ampPreviewArea.getX()) - S(14);
        const int preferredX = gapLeft + juce::jmax(0, (gapRight - gapLeft - postEqButtonWidth) / 2) + S(34);
        const int postEqButtonX = juce::jlimit(fullTabArea.getX(),
                                               fullTabArea.getRight() - postEqButtonWidth,
                                               preferredX);
        const int minimumSafeY = lofiIntensityKnob.isVisible()
            ? lofiIntensityLabel.getBottom() + S(18)
            : postEqRow.getY();
        const int preferredY = juce::jmax(minimumSafeY,
                                          postEqRow.getCentreY() - postEqButtonHeight / 2);
        const int postEqButtonY = juce::jlimit(postEqRow.getY(),
                                               postEqRow.getBottom() - postEqButtonHeight,
                                               preferredY);
        postEqEnableButton.setBounds(postEqButtonX, postEqButtonY, postEqButtonWidth, postEqButtonHeight);

        const int cabCutMaxRight = juce::jmax(fullTabArea.getX() + S(150), postEqEnableButton.getX() - S(10));
        const int cabCutWidth = juce::jlimit(S(150), S(240), cabCutMaxRight - fullTabArea.getX());
        auto cabCutArea = juce::Rectangle<int>(fullTabArea.getX(), postEqRow.getY(), cabCutWidth, postEqRow.getHeight());
        auto lowCutRow = cabCutArea.removeFromTop(S(16));
        cabCutArea.removeFromTop(S(2));
        auto highCutRow = cabCutArea.removeFromTop(S(16));

        auto lowCutLabelArea = lowCutRow.removeFromLeft(S(42));
        cabLowCutLabel.setBounds(lowCutLabelArea);
        cabLowCutLabel.setFont(juce::FontOptions(10.5f * scale, juce::Font::bold));
        cabLowCutSlider.setBounds(lowCutRow);
        cabLowCutSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, S(58), S(16));

        auto highCutLabelArea = highCutRow.removeFromLeft(S(42));
        cabHighCutLabel.setBounds(highCutLabelArea);
        cabHighCutLabel.setFont(juce::FontOptions(10.5f * scale, juce::Font::bold));
        cabHighCutSlider.setBounds(highCutRow);
        cabHighCutSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, S(58), S(16));

        auto eqArea = ampArea.removeFromTop(S(106));
        ampArea.removeFromTop(S(6));
        auto knobArea = ampArea.reduced(S(1), 0);

        const int itemWidth = knobArea.getWidth() / 8;

        juce::Slider* sliders[] = { &inputSlider, &driveSlider, &toneSlider, &micDistanceSlider, &micBlendSlider, &outputSlider, &depthSlider, &mixSlider };
        juce::Label* labels[] = { &inputLabel, &driveLabel, &toneLabel, &micDistanceLabel, &micBlendLabel, &outputLabel, &depthLabel, &mixLabel };

        for (int i = 0; i < 8; ++i)
        {
            auto item = knobArea.removeFromLeft(itemWidth).reduced(S(1));
            labels[i]->setBounds(item.removeFromTop(S(18)));
            labels[i]->setFont(juce::FontOptions(13.5f * scale, juce::Font::bold));
            sliders[i]->setBounds(item);

            sliders[i]->setTextBoxStyle(juce::Slider::TextBoxBelow, false, S(68), S(20));
        }

        const int eqItemWidth = juce::jmax(S(30), eqArea.getWidth() / static_cast<int>(postEqBandSliders.size()));
        for (size_t i = 0; i < postEqBandSliders.size(); ++i)
        {
            auto item = eqArea.removeFromLeft(eqItemWidth).reduced(S(1), 0);
            postEqBandLabels[i].setBounds(item.removeFromTop(S(14)));
            postEqBandLabels[i].setFont(juce::FontOptions(10.5f * scale, juce::Font::bold));
            postEqBandSliders[i].setBounds(item);
            postEqBandSliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, S(40), S(16));
        }
    }

    auto tunerBounds = controlsArea.reduced(S(2));

    tunerMeterDrawBounds = tunerBounds.removeFromTop(S(44)).reduced(S(24), 0);
    tunerCentsMeter.setBounds(tunerMeterDrawBounds);

    tunerBounds.removeFromTop(S(6));

    tunerBounds.removeFromTop(S(8));
    tunerNoteLabel.setBounds(tunerBounds.removeFromTop(S(84)));
    tunerFreqLabel.setBounds(tunerBounds.removeFromTop(S(32)));
    tunerCentsLabel.setBounds(tunerBounds.removeFromTop(S(26)));
    tunerBounds.removeFromTop(S(8));
    tunerInputLevelBarBounds = tunerBounds.removeFromTop(S(10)).reduced(S(24), 0);

    auto fxBounds = fullTabArea.reduced(S(4));
    fxPanelLabel.setBounds(fxBounds.removeFromTop(S(24)));
    fxBounds.removeFromTop(S(2));

    fxBackButton.setBounds(fxBounds.removeFromTop(S(24)).removeFromRight(S(118)));
    fxBounds.removeFromTop(S(2));

    fxPedalAreaBounds = fxBounds;
    const int gap = S(12);
    const int pedalWidth = (fxBounds.getWidth() - gap * 4) / 5;
    int x = fxBounds.getX();

    for (int i = 0; i < 5; ++i)
    {
        auto slot = juce::Rectangle<int>(x,
                                         fxBounds.getY(),
                                         pedalWidth,
                                         fxBounds.getHeight()).reduced(S(2));

        fxPedalLabels[static_cast<size_t>(i)].setBounds(slot.removeFromTop(S(16)));
        slot.removeFromTop(S(2));
        fxPowerDrawBounds[static_cast<size_t>(i)] = slot.removeFromBottom(S(20)).withSizeKeepingCentre(S(20), S(20));
        slot.removeFromBottom(S(2));

        fxPedalDrawBounds[static_cast<size_t>(i)] = slot;
        x += pedalWidth + gap;
    }

    auto detailArea = fxBounds.reduced(S(20), S(4));
    auto detailHeader = detailArea.removeFromTop(S(30));
    fxDetailPowerBounds = detailHeader.withSizeKeepingCentre(S(24), S(24));
    detailArea.removeFromTop(S(6));

    auto fxControlsArea = detailArea;
    fxControlsArea.removeFromBottom(S(6));

    fxDetailPedalBounds = {};

    const auto clearKnob = [](juce::Slider& slider, juce::Label& label)
    {
        slider.setBounds({});
        label.setBounds({});
    };

    clearKnob(fxPitchShiftSlider, fxPitchShiftLabel);
    clearKnob(fxPitchMixSlider, fxPitchMixLabel);
    clearKnob(fxPitchWidthSlider, fxPitchWidthLabel);
    clearKnob(fxWahFreqSlider, fxWahFreqLabel);
    clearKnob(fxWahQSlider, fxWahQLabel);
    clearKnob(fxWahMixSlider, fxWahMixLabel);
    clearKnob(fxDriveAmountSlider, fxDriveAmountLabel);
    clearKnob(fxDriveToneSlider, fxDriveToneLabel);
    clearKnob(fxDriveLevelSlider, fxDriveLevelLabel);
    clearKnob(fxDriveMixSlider, fxDriveMixLabel);
    clearKnob(fxDriveTightSlider, fxDriveTightLabel);
    clearKnob(fxDelayTimeSlider, fxDelayTimeLabel);
    clearKnob(fxDelayFeedbackSlider, fxDelayFeedbackLabel);
    clearKnob(fxDelayToneSlider, fxDelayToneLabel);
    clearKnob(fxDelayMixSlider, fxDelayMixLabel);
    clearKnob(fxDelayWidthSlider, fxDelayWidthLabel);
    fxDelaySyncButton.setBounds({});
    clearKnob(fxReverbSizeSlider, fxReverbSizeLabel);
    clearKnob(fxReverbDampSlider, fxReverbDampLabel);
    clearKnob(fxReverbMixSlider, fxReverbMixLabel);
    clearKnob(fxReverbPredelaySlider, fxReverbPredelayLabel);

    const auto layoutKnobs = [&](juce::Rectangle<int> area,
                                 std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs)
    {
        if (knobs.size() == 0)
            return;

        const int itemWidth = area.getWidth() / static_cast<int>(knobs.size());

        for (const auto& [slider, label] : knobs)
        {
            auto item = area.removeFromLeft(itemWidth).reduced(S(2));
            label->setBounds(item.removeFromTop(S(20)));
            label->setFont(juce::FontOptions(13.0f * scale, juce::Font::bold));
            slider->setBounds(item);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, S(72), S(22));
        }
    };

    switch (selectedFxPedalIndex)
    {
        case 0:
            layoutKnobs(fxControlsArea,
                        { { &fxPitchShiftSlider, &fxPitchShiftLabel },
                                                    { &fxPitchMixSlider, &fxPitchMixLabel },
                                                    { &fxPitchWidthSlider, &fxPitchWidthLabel } });
            break;
        case 1:
            layoutKnobs(fxControlsArea,
                        { { &fxWahFreqSlider, &fxWahFreqLabel },
                          { &fxWahQSlider, &fxWahQLabel },
                          { &fxWahMixSlider, &fxWahMixLabel } });
            break;
        case 2:
            layoutKnobs(fxControlsArea,
                        { { &fxDriveAmountSlider, &fxDriveAmountLabel },
                          { &fxDriveToneSlider, &fxDriveToneLabel },
                          { &fxDriveLevelSlider, &fxDriveLevelLabel },
                          { &fxDriveMixSlider, &fxDriveMixLabel },
                          { &fxDriveTightSlider, &fxDriveTightLabel } });
            break;
        case 3:
            layoutKnobs(fxControlsArea,
                        { { &fxReverbSizeSlider, &fxReverbSizeLabel },
                          { &fxReverbDampSlider, &fxReverbDampLabel },
                          { &fxReverbMixSlider, &fxReverbMixLabel },
                          { &fxReverbPredelaySlider, &fxReverbPredelayLabel } });
            break;
        case 4:
        default:
        {
            auto delayControlArea = fxControlsArea;
            auto syncRow = delayControlArea.removeFromTop(S(22));
            fxDelaySyncButton.setBounds(syncRow.removeFromLeft(S(168)));
            delayControlArea.removeFromTop(S(4));
            layoutKnobs(delayControlArea,
                        { { &fxDelayTimeSlider, &fxDelayTimeLabel },
                          { &fxDelayFeedbackSlider, &fxDelayFeedbackLabel },
                          { &fxDelayToneSlider, &fxDelayToneLabel },
                          { &fxDelayMixSlider, &fxDelayMixLabel },
                          { &fxDelayWidthSlider, &fxDelayWidthLabel } });
            break;
        }
    }
}

void HexstackAudioProcessorEditor::refreshIRStatus()
{
    irStatusLabel.setText("Cab IR: " + audioProcessor.getCurrentIRName(), juce::dontSendNotification);
    clearIRButton.setVisible(activeTab == ActiveTab::amp && audioProcessor.isUsingExternalIR());
}

void HexstackAudioProcessorEditor::refreshDelaySyncUi()
{
    const auto* syncValue = audioProcessor.getParametersState().getRawParameterValue(ParamIDs::fxDelaySync);
    const bool syncEnabled = syncValue != nullptr && syncValue->load() >= 0.5f;
    lastDelaySyncUiState = syncEnabled;

    constexpr double sliderMin = 60.0;
    constexpr double sliderMax = 900.0;
    const double syncStep = delaySyncDisplayNames.size() > 1
        ? (sliderMax - sliderMin) / static_cast<double>(delaySyncDisplayNames.size() - 1)
        : 1.0;

    fxDelayTimeLabel.setText(syncEnabled ? "Division" : "Time", juce::dontSendNotification);
    fxDelayTimeSlider.setRange(sliderMin, sliderMax, syncEnabled ? syncStep : 1.0);

    if (syncEnabled)
    {
        const int divisionIndex = getDelaySyncDivisionIndex(static_cast<float>(fxDelayTimeSlider.getValue()));
        const double snappedValue = sliderMin + syncStep * static_cast<double>(divisionIndex);
        if (! juce::approximatelyEqual(fxDelayTimeSlider.getValue(), snappedValue))
            fxDelayTimeSlider.setValue(snappedValue, juce::sendNotificationSync);
    }

    fxDelayTimeSlider.setNumDecimalPlacesToDisplay(syncEnabled ? 0 : 0);
    fxDelayTimeSlider.updateText();
    fxDelayTimeSlider.repaint();
    fxDelayTimeLabel.repaint();
}

void HexstackAudioProcessorEditor::refreshPostEqControlState()
{
    const bool enabled = postEqEnableButton.getToggleState();

    for (size_t i = 0; i < postEqBandSliders.size(); ++i)
    {
        postEqBandSliders[i].setEnabled(enabled);
        postEqBandSliders[i].setAlpha(enabled ? 1.0f : 0.48f);
        postEqBandLabels[i].setAlpha(enabled ? 1.0f : 0.62f);
    }
}

void HexstackAudioProcessorEditor::updateTabVisibility()
{
    const bool showAmp = activeTab == ActiveTab::amp;
    const bool showTuner = activeTab == ActiveTab::tuner;
    const bool showFxBoard = activeTab == ActiveTab::fx;
    const bool showFxPedal = activeTab == ActiveTab::fxPedal;
    const bool showFx = showFxBoard || showFxPedal;

    audioProcessor.setTunerAnalysisEnabled(showTuner);
    audioProcessor.setTunerOutputMuted(showTuner);

    ampTabButton.setToggleState(showAmp, juce::dontSendNotification);
    tunerTabButton.setToggleState(showTuner, juce::dontSendNotification);
    fxTabButton.setToggleState(showFx, juce::dontSendNotification);

    titleLabel.setVisible(! titleImage.isValid() && ! showTuner);

    juce::Component* ampControls[] = {
        &inputSlider, &driveSlider, &toneSlider, &micDistanceSlider, &micBlendSlider, &outputSlider, &depthSlider, &mixSlider,
        &inputLabel, &driveLabel, &toneLabel, &micDistanceLabel, &micBlendLabel, &outputLabel, &depthLabel, &mixLabel,
        &loadIRButton, &clearIRButton, &saveHexButton, &loadHexButton, &voicingRhythmButton, &voicingLeadButton, &lofiButton, &postEqEnableButton, &irStatusLabel,
        &cabLowCutSlider, &cabHighCutSlider, &cabLowCutLabel, &cabHighCutLabel
    };

    for (auto* c : ampControls)
        c->setVisible(showAmp);

    for (size_t i = 0; i < postEqBandSliders.size(); ++i)
    {
        postEqBandSliders[i].setVisible(showAmp);
        postEqBandLabels[i].setVisible(showAmp);
    }

    lofiButton.setVisible(true);
    lofiIntensityKnob.setVisible(lofiButton.getToggleState());
    lofiIntensityLabel.setVisible(lofiButton.getToggleState());
    stfuKnob.setVisible(showAmp);
    stfuLabel.setVisible(showAmp);
    tapeSatKnob.setVisible(showAmp);
    tapeSatLabel.setVisible(showAmp);
    limiterKnob.setVisible(showAmp);
    limiterLabel.setVisible(showAmp);
    limiterGrLabel.setVisible(showAmp);
    makeupGainSlider.setVisible(showAmp);
    makeupGainLabel.setVisible(showAmp);

    tunerPanelLabel.setVisible(false);
    tunerRefLabel.setVisible(showTuner);
    tunerRefCombo.setVisible(showTuner);
    tunerRangeLabel.setVisible(showTuner);
    tunerRangeCombo.setVisible(showTuner);
    tunerNoteLabel.setVisible(showTuner);
    tunerFreqLabel.setVisible(showTuner);
    tunerCentsLabel.setVisible(showTuner);
    tunerCentsMeter.setVisible(false);
    fxPanelLabel.setVisible(showFx);
    fxBackButton.setVisible(showFxPedal);

    if (showFxPedal)
    {
        const int idx = juce::jlimit(0, static_cast<int>(fxPedalLabels.size()) - 1, selectedFxPedalIndex);
        fxPanelLabel.setText(fxPedalLabels[static_cast<size_t>(idx)].getText() + " PEDAL",
                             juce::dontSendNotification);
    }
    else
    {
        fxPanelLabel.setText("FX CHAIN", juce::dontSendNotification);
    }

    for (size_t i = 0; i < fxPedalLabels.size(); ++i)
    {
        fxPedalLabels[i].setVisible(showFxBoard);
    }

    auto setFxControlVisible = [showFxPedal](juce::Slider& slider, juce::Label& label, bool on)
    {
        slider.setVisible(showFxPedal && on);
        label.setVisible(showFxPedal && on);
    };

    const int fxIndex = juce::jlimit(0, 4, selectedFxPedalIndex);
    setFxControlVisible(fxPitchShiftSlider, fxPitchShiftLabel, fxIndex == 0);
    setFxControlVisible(fxPitchMixSlider, fxPitchMixLabel, fxIndex == 0);
    setFxControlVisible(fxPitchWidthSlider, fxPitchWidthLabel, fxIndex == 0);
    setFxControlVisible(fxWahFreqSlider, fxWahFreqLabel, fxIndex == 1);
    setFxControlVisible(fxWahQSlider, fxWahQLabel, fxIndex == 1);
    setFxControlVisible(fxWahMixSlider, fxWahMixLabel, fxIndex == 1);
    setFxControlVisible(fxDriveAmountSlider, fxDriveAmountLabel, fxIndex == 2);
    setFxControlVisible(fxDriveToneSlider, fxDriveToneLabel, fxIndex == 2);
    setFxControlVisible(fxDriveLevelSlider, fxDriveLevelLabel, fxIndex == 2);
    setFxControlVisible(fxDriveMixSlider, fxDriveMixLabel, fxIndex == 2);
    setFxControlVisible(fxDriveTightSlider, fxDriveTightLabel, fxIndex == 2);
    setFxControlVisible(fxReverbSizeSlider, fxReverbSizeLabel, fxIndex == 3);
    setFxControlVisible(fxReverbDampSlider, fxReverbDampLabel, fxIndex == 3);
    setFxControlVisible(fxReverbMixSlider, fxReverbMixLabel, fxIndex == 3);
    setFxControlVisible(fxReverbPredelaySlider, fxReverbPredelayLabel, fxIndex == 3);
    setFxControlVisible(fxDelayTimeSlider, fxDelayTimeLabel, fxIndex == 4);
    setFxControlVisible(fxDelayFeedbackSlider, fxDelayFeedbackLabel, fxIndex == 4);
    setFxControlVisible(fxDelayToneSlider, fxDelayToneLabel, fxIndex == 4);
    setFxControlVisible(fxDelayMixSlider, fxDelayMixLabel, fxIndex == 4);
    setFxControlVisible(fxDelayWidthSlider, fxDelayWidthLabel, fxIndex == 4);
    fxDelaySyncButton.setVisible(showFxPedal && fxIndex == 4);

    if (showAmp)
        refreshIRStatus();

    refreshDelaySyncUi();

    if (showTuner)
        repaint(tunerMeterDrawBounds.expanded(18));

    repaint();
}

void HexstackAudioProcessorEditor::mouseUp(const juce::MouseEvent& event)
{
    if (activeTab == ActiveTab::fx)
    {
        const auto pos = event.getPosition();
        bool handledPowerToggle = false;

        for (size_t i = 0; i < fxPowerDrawBounds.size(); ++i)
        {
            if (fxPowerDrawBounds[i].contains(pos))
            {
                setFxPowerParameterValue(i, ! fxPedalPowerOn[i]);
                syncFxPowerFromParameters();
                repaint(fxPowerDrawBounds[i].expanded(10));
                handledPowerToggle = true;
                break;
            }
        }

        if (! handledPowerToggle)
        {
            for (size_t i = 0; i < fxPedalDrawBounds.size(); ++i)
            {
                if (fxPedalDrawBounds[i].contains(pos))
                {
                    selectedFxPedalIndex = static_cast<int>(i);
                    activeTab = ActiveTab::fxPedal;
                    resized();
                    updateTabVisibility();
                    break;
                }
            }
        }
    }
    else if (activeTab == ActiveTab::fxPedal)
    {
        const auto pos = event.getPosition();
        if (fxDetailPowerBounds.contains(pos))
        {
            const size_t index = static_cast<size_t>(juce::jlimit(0,
                                                                  static_cast<int>(fxPedalPowerOn.size()) - 1,
                                                                  selectedFxPedalIndex));
            setFxPowerParameterValue(index, ! fxPedalPowerOn[index]);
            syncFxPowerFromParameters();
            repaint(fxDetailPowerBounds.expanded(14));
        }
    }

    AudioProcessorEditor::mouseUp(event);
}

void HexstackAudioProcessorEditor::updateTunerDisplay()
{
    static constexpr const char* noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    const auto tuner = audioProcessor.getTunerData();

    if (! tuner.hasSignal || tuner.midiNote < 0)
    {
        tunerNoteLabel.setText("--", juce::dontSendNotification);
        tunerFreqLabel.setText("", juce::dontSendNotification);

        // Show input level so the user can tell whether signal is reaching the tuner.
        const juce::String levelText = tuner.levelDb > -99.0f
            ? "No note detected  •  " + juce::String(tuner.levelDb, 1) + " dB"
            : "Waiting for signal";
        tunerCentsLabel.setText(levelText, juce::dontSendNotification);
        tunerCentsLabel.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.65f));
        tunerCentsMeter.setValue(0.0, juce::dontSendNotification);
        tunerCentsMeter.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(120, 120, 128));

        tunerTargetCentsUi = 0.0f;
        tunerDisplayCentsUi += (0.0f - tunerDisplayCentsUi) * 0.42f;
        if (std::abs(tunerDisplayCentsUi) < 0.05f)
            tunerDisplayCentsUi = 0.0f;
        tunerSignalPresentUi = false;
        tunerInTuneUi = false;
        tunerGlowStrengthUi += (0.0f - tunerGlowStrengthUi) * 0.35f;
        if (tunerGlowStrengthUi < 0.01f)
            tunerGlowStrengthUi = 0.0f;

        // Decay the input level bar toward silence when no note is detected.
        tunerInputLevelSmoothedUi += (tuner.levelDb - tunerInputLevelSmoothedUi)
                                     * ((tuner.levelDb > tunerInputLevelSmoothedUi) ? 0.80f : 0.12f);

        repaint(tunerMeterDrawBounds.expanded(18));
        repaint(tunerInputLevelBarBounds.expanded(4));
        return;
    }

    const int noteClass = ((tuner.midiNote % 12) + 12) % 12;
    const int octave = tuner.midiNote / 12 - 1;
    const float cents = juce::jlimit(-50.0f, 50.0f, tuner.centsOffset);
    const bool inTune = std::abs(cents) <= 5.0f;
    const float refHz = audioProcessor.getTunerReferenceHz();

    tunerNoteLabel.setText(juce::String(noteNames[noteClass]) + juce::String(octave), juce::dontSendNotification);
    tunerFreqLabel.setText(juce::String(tuner.frequencyHz, 2) + " Hz", juce::dontSendNotification);

    juce::String tuningState;
    juce::Colour tuningStateColour = juce::Colour::fromRGB(226, 178, 26);
    if (inTune)
    {
        tuningState = "IN TUNE";
        tuningStateColour = juce::Colour::fromRGB(58, 224, 104);
    }
    else if (cents < 0.0f)
    {
        tuningState = "FLAT " + juce::String(std::abs(cents), 1) + "¢";
        tuningStateColour = (std::abs(cents) > 15.0f)
            ? juce::Colour::fromRGB(220, 38, 48)
            : juce::Colour::fromRGB(226, 178, 26);
    }
    else
    {
        tuningState = "SHARP +" + juce::String(cents, 1) + "¢";
        tuningStateColour = (std::abs(cents) > 15.0f)
            ? juce::Colour::fromRGB(220, 38, 48)
            : juce::Colour::fromRGB(226, 178, 26);
    }

    tunerCentsLabel.setText(tuningState + "   •   A4 " + juce::String(refHz, 1) + " Hz",
                            juce::dontSendNotification);
    tunerCentsLabel.setColour(juce::Label::textColourId, tuningStateColour);

    tunerNoteLabel.setColour(juce::Label::textColourId,
                             inTune ? juce::Colour::fromRGB(92, 230, 132) : juce::Colours::whitesmoke);
    tunerCentsMeter.setColour(juce::Slider::thumbColourId,
                              inTune ? juce::Colour::fromRGB(92, 230, 132)
                                     : juce::Colour::fromRGB(230, 50, 65));

    tunerTargetCentsUi = cents;
    tunerDisplayCentsUi += (tunerTargetCentsUi - tunerDisplayCentsUi) * 0.34f;
    if (std::abs(tunerTargetCentsUi - tunerDisplayCentsUi) < 0.025f)
        tunerDisplayCentsUi = tunerTargetCentsUi;

    // Drive the meter from the UI-smoothed value so it doesn't jerk on
    // every analysis frame — the raw atomic can still change every ~93ms.
    tunerCentsMeter.setValue(static_cast<double>(tunerDisplayCentsUi), juce::dontSendNotification);
    tunerSignalPresentUi = true;
    tunerInTuneUi = inTune;
    const float targetGlowStrength = juce::jlimit(0.0f, 1.0f, juce::jmap(tuner.levelDb, -60.0f, -12.0f, 0.0f, 1.0f));
    tunerGlowStrengthUi += (targetGlowStrength - tunerGlowStrengthUi) * 0.28f;

    // Animate the input level bar (fast attack, moderate release)
    const float levelTarget = tuner.levelDb;
    const float levelCoeff = (levelTarget > tunerInputLevelSmoothedUi) ? 0.80f : 0.12f;
    tunerInputLevelSmoothedUi += (levelTarget - tunerInputLevelSmoothedUi) * levelCoeff;

    repaint(tunerMeterDrawBounds.expanded(18));
    repaint(tunerInputLevelBarBounds.expanded(4));
}

void HexstackAudioProcessorEditor::timerCallback()
{
    syncFxPowerFromParameters();

    // Reconcile combo selection with processor state so that DAW undo/redo and
    // project recall update the combo even when the editor was already open.
    {
        const juce::String currentHexPath = audioProcessor.getActiveHexFilePath();
        if (currentHexPath != lastKnownHexPath)
        {
            lastKnownHexPath = currentHexPath;
            if (currentHexPath.isNotEmpty())
            {
                bool found = false;
                for (int i = 0; i < static_cast<int>(userHexPresets.size()); ++i)
                {
                    if (userHexPresets[static_cast<size_t>(i)].file.getFullPathName() == currentHexPath)
                    {
                        activeUserHexIndex = i;
                        presetCombo.setSelectedId(numBuiltInPresets + i + 1, juce::dontSendNotification);
                        found = true;
                        break;
                    }
                }
                if (! found)
                    activeUserHexIndex = -1;
            }
            else
            {
                activeUserHexIndex = -1;
                presetCombo.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
            }
            refreshIRStatus();
            refreshPostEqControlState();
            repaint();
        }
    }

    const auto* syncValue = audioProcessor.getParametersState().getRawParameterValue(ParamIDs::fxDelaySync);
    const bool syncEnabled = syncValue != nullptr && syncValue->load() >= 0.5f;
    if (syncEnabled != lastDelaySyncUiState)
        refreshDelaySyncUi();

    if (activeTab == ActiveTab::tuner)
        updateTunerDisplay();

    // Update limiter GR readout regardless of tab so the value is fresh when user
    // switches to the amp tab mid-playing.
    {
        const float grDb = audioProcessor.getLimiterGainReductionDb();
        if (grDb < -0.05f)
            limiterGrLabel.setText("-" + juce::String(std::abs(grDb), 1), juce::dontSendNotification);
        else
            limiterGrLabel.setText("-0.0", juce::dontSendNotification);
    }
}

void HexstackAudioProcessorEditor::syncFxPowerFromParameters()
{
    for (size_t i = 0; i < fxPedalPowerOn.size(); ++i)
        fxPedalPowerOn[i] = getFxPowerParameterValue(i);
}

bool HexstackAudioProcessorEditor::getFxPowerParameterValue(size_t index) const
{
    constexpr std::array<const char*, 5> ids {
        ParamIDs::fxPower1,
        ParamIDs::fxPower2,
        ParamIDs::fxPower3,
        ParamIDs::fxPower4,
        ParamIDs::fxPower5
    };

    if (index >= ids.size())
        return true;

    if (const auto* value = audioProcessor.getParametersState().getRawParameterValue(ids[index]))
        return value->load() >= 0.5f;

    return true;
}

void HexstackAudioProcessorEditor::setFxPowerParameterValue(size_t index, bool enabled)
{
    constexpr std::array<const char*, 5> ids {
        ParamIDs::fxPower1,
        ParamIDs::fxPower2,
        ParamIDs::fxPower3,
        ParamIDs::fxPower4,
        ParamIDs::fxPower5
    };

    if (index >= ids.size())
        return;

    if (auto* parameter = audioProcessor.getParametersState().getParameter(ids[index]))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        parameter->endChangeGesture();
    }
}

// ── User hex preset history ───────────────────────────────────────────────────

void HexstackAudioProcessorEditor::rebuildPresetCombo(int targetSelectedId)
{
    // If a target is given, use it; otherwise preserve the current selection.
    const int idToSelect = (targetSelectedId > 0) ? targetSelectedId
                                                   : presetCombo.getSelectedId();

    presetCombo.clear(juce::dontSendNotification);

    for (int i = 0; i < numBuiltInPresets; ++i)
        presetCombo.addItem(audioProcessor.getProgramName(i), i + 1);

    if (! userHexPresets.empty())
    {
        presetCombo.addSeparator();
        for (int i = 0; i < static_cast<int>(userHexPresets.size()); ++i)
            presetCombo.addItem(userHexPresets[static_cast<size_t>(i)].name,
                                numBuiltInPresets + i + 1);
    }

    // Set the final selection once — no intermediate state, no deferred-paint race.
    if (idToSelect > 0)
        presetCombo.setSelectedId(idToSelect, juce::dontSendNotification);
}

void HexstackAudioProcessorEditor::addOrRefreshUserHexEntry(const juce::String& name,
                                                             const juce::File& file)
{
    // If this file is already in the list, update its display name in place.
    for (int i = 0; i < static_cast<int>(userHexPresets.size()); ++i)
    {
        if (userHexPresets[static_cast<size_t>(i)].file == file)
        {
            userHexPresets[static_cast<size_t>(i)].name = name;
            activeUserHexIndex = i;
            rebuildPresetCombo(numBuiltInPresets + i + 1);
            saveUserHexList();
            return;
        }
    }

    // New entry — cap at 1000 user presets (remove the oldest when over the limit).
    constexpr int maxUserPresets = 1000;
    if (static_cast<int>(userHexPresets.size()) >= maxUserPresets)
        userHexPresets.erase(userHexPresets.begin());

    userHexPresets.push_back({ name, file });
    activeUserHexIndex = static_cast<int>(userHexPresets.size()) - 1;

    rebuildPresetCombo(numBuiltInPresets + activeUserHexIndex + 1);
    saveUserHexList();
}

void HexstackAudioProcessorEditor::loadUserHexList()
{
    userHexPresets.clear();

    const auto xmlFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("HEXSTACK")
                             .getChildFile("userHexPresets.xml");

    if (! xmlFile.existsAsFile())
        return;

    const auto xml = juce::XmlDocument::parse(xmlFile);
    if (xml == nullptr || ! xml->hasTagName("userHexPresets"))
        return;

    for (auto* entry : xml->getChildIterator())
    {
        const juce::File file (entry->getStringAttribute("path"));
        if (! file.existsAsFile())
            continue;

        // Prefer the XML-stored display name if it is non-empty and not the old
        // "Default" sentinel that an earlier bug baked into every saved preset.
        juce::String name = entry->getStringAttribute("name");
        if (name.isEmpty() || name.equalsIgnoreCase("Default"))
            name = file.getFileNameWithoutExtension();

        userHexPresets.push_back({ name, file });
    }

    // Deduplicate display names: when several entries share the same name
    // (e.g. all files are called "Default.hex"), suffix them so the user can
    // tell them apart — "Default (1)", "Default (2)", etc.
    std::unordered_map<juce::String, int> nameCount;
    for (const auto& e : userHexPresets)
        ++nameCount[e.name];

    std::unordered_map<juce::String, int> nameSeen;
    for (auto& e : userHexPresets)
    {
        if (nameCount[e.name] > 1)
        {
            const int idx = ++nameSeen[e.name];
            e.name = e.name + " (" + juce::String(idx) + ")";
        }
    }
}

void HexstackAudioProcessorEditor::saveUserHexList()
{
    const auto xmlDir  = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                             .getChildFile("HEXSTACK");
    const auto xmlFile = xmlDir.getChildFile("userHexPresets.xml");

    if (! xmlDir.isDirectory())
        xmlDir.createDirectory();

    juce::XmlElement xml("userHexPresets");
    for (const auto& entry : userHexPresets)
    {
        auto* child = xml.createNewChildElement("preset");
        child->setAttribute("name", entry.name);
        child->setAttribute("path", entry.file.getFullPathName());
    }
    xml.writeTo(xmlFile);
}
