#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{
    struct BiquadCoefficients
    {
        float b0 { 1.0f };
        float b1 { 0.0f };
        float b2 { 0.0f };
        float a1 { 0.0f };
        float a2 { 0.0f };
    };

    BiquadCoefficients normalizeBiquad(float b0, float b1, float b2, float a0, float a1, float a2)
    {
        const float invA0 = 1.0f / a0;
        return { b0 * invA0, b1 * invA0, b2 * invA0, a1 * invA0, a2 * invA0 };
    }

    BiquadCoefficients makePeak(float sampleRate, float frequency, float q, float gainDb)
    {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const float sinO = std::sin(omega);
        const float cosO = std::cos(omega);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float alpha = sinO / (2.0f * q);

        return normalizeBiquad(1.0f + alpha * A,
                               -2.0f * cosO,
                               1.0f - alpha * A,
                               1.0f + alpha / A,
                               -2.0f * cosO,
                               1.0f - alpha / A);
    }

    BiquadCoefficients makeLowShelf(float sampleRate, float frequency, float q, float gainDb)
    {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const float sinO = std::sin(omega);
        const float cosO = std::cos(omega);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float alpha = sinO / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f);
        const float twoRootAAlpha = 2.0f * std::sqrt(A) * alpha;

        return normalizeBiquad(A * ((A + 1.0f) - (A - 1.0f) * cosO + twoRootAAlpha),
                               2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosO),
                               A * ((A + 1.0f) - (A - 1.0f) * cosO - twoRootAAlpha),
                               (A + 1.0f) + (A - 1.0f) * cosO + twoRootAAlpha,
                               -2.0f * ((A - 1.0f) + (A + 1.0f) * cosO),
                               (A + 1.0f) + (A - 1.0f) * cosO - twoRootAAlpha);
    }

    BiquadCoefficients makeHighShelf(float sampleRate, float frequency, float q, float gainDb)
    {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const float sinO = std::sin(omega);
        const float cosO = std::cos(omega);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float alpha = sinO / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / q - 1.0f) + 2.0f);
        const float twoRootAAlpha = 2.0f * std::sqrt(A) * alpha;

        return normalizeBiquad(A * ((A + 1.0f) + (A - 1.0f) * cosO + twoRootAAlpha),
                               -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosO),
                               A * ((A + 1.0f) + (A - 1.0f) * cosO - twoRootAAlpha),
                               (A + 1.0f) - (A - 1.0f) * cosO + twoRootAAlpha,
                               2.0f * ((A - 1.0f) - (A + 1.0f) * cosO),
                               (A + 1.0f) - (A - 1.0f) * cosO - twoRootAAlpha);
    }

    BiquadCoefficients makeLowPass(float sampleRate, float frequency, float q)
    {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const float sinO = std::sin(omega);
        const float cosO = std::cos(omega);
        const float alpha = sinO / (2.0f * q);

        return normalizeBiquad((1.0f - cosO) * 0.5f,
                               1.0f - cosO,
                               (1.0f - cosO) * 0.5f,
                               1.0f + alpha,
                               -2.0f * cosO,
                               1.0f - alpha);
    }

    BiquadCoefficients makeHighPass(float sampleRate, float frequency, float q)
    {
        const float omega = 2.0f * juce::MathConstants<float>::pi * frequency / sampleRate;
        const float sinO = std::sin(omega);
        const float cosO = std::cos(omega);
        const float alpha = sinO / (2.0f * q);

        return normalizeBiquad((1.0f + cosO) * 0.5f,
                               -(1.0f + cosO),
                               (1.0f + cosO) * 0.5f,
                               1.0f + alpha,
                               -2.0f * cosO,
                               1.0f - alpha);
    }

    void applyBiquad(std::vector<float>& samples, const BiquadCoefficients& c)
    {
        float x1 = 0.0f;
        float x2 = 0.0f;
        float y1 = 0.0f;
        float y2 = 0.0f;

        for (auto& x : samples)
        {
            const float y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            x = y;
        }
    }

    void normalizeImpulsePeak(std::vector<float>& samples, float targetPeak)
    {
        float peak = 0.0f;
        for (const auto sample : samples)
            peak = juce::jmax(peak, std::abs(sample));

        if (peak <= 1.0e-6f)
            return;

        const float gain = targetPeak / peak;
        for (auto& sample : samples)
            sample *= gain;
    }

    void addImpulseTap(std::vector<float>& samples, int index, float amplitude)
    {
        if (samples.empty())
            return;

        const auto addSample = [&samples](int i, float amount)
        {
            if (i >= 0 && i < static_cast<int>(samples.size()))
                samples[static_cast<size_t>(i)] += amount;
        };

        addSample(index - 1, amplitude * 0.22f);
        addSample(index, amplitude);
        addSample(index + 1, amplitude * 0.22f);
    }

    std::vector<float> shiftImpulseFractional(const std::vector<float>& source, float shiftSamples)
    {
        if (source.empty())
            return source;

        std::vector<float> shifted(source.size(), 0.0f);

        for (int target = 0; target < static_cast<int>(shifted.size()); ++target)
        {
            const float sourceIndex = static_cast<float>(target) - shiftSamples;
            if (sourceIndex < 0.0f || sourceIndex >= static_cast<float>(source.size() - 1))
                continue;

            const int index0 = static_cast<int>(std::floor(sourceIndex));
            const int index1 = juce::jmin(index0 + 1, static_cast<int>(source.size()) - 1);
            const float frac = sourceIndex - static_cast<float>(index0);
            shifted[static_cast<size_t>(target)] = juce::jmap(frac,
                                                              source[static_cast<size_t>(index0)],
                                                              source[static_cast<size_t>(index1)]);
        }

        return shifted;
    }

    struct MicProfile
    {
        float proximityNearDb;
        float proximityFarDb;
        float bodyFreq;
        float bodyQ;
        float bodyDb;
        float presenceFreq;
        float presenceQ;
        float presenceNearDb;
        float presenceFarDb;
        float biteFreq;
        float biteQ;
        float biteNearDb;
        float biteFarDb;
        float airFreq;
        float airNearDb;
        float airFarDb;
        float lowPassNearHz;
        float lowPassFarHz;
        float highPassHz;
        float roomAmount;
        float directLevel;
    };

    MicProfile getFixedCabProfile()
    {
        // proximityNearDb boosted to 2.0 for more low-end proximity effect (balls)
        // bodyDb raised from -0.9 to 0.6 for more chest/body in the 625 Hz range
        // presenceNearDb/Far reduced from 2.8/1.2 to 0.6/0.3 to tame harsh 3kHz
        // biteNearDb reduced from 1.25 to 0.2 to tame harsh 4.7kHz bite
        // airNearDb reduced from -0.05 to -1.8 for more natural air rolloff
        // lowPassNear/Far reduced from 8000/6125 to 5500/4800 Hz for a realistic SM57 rolloff
        return { 2.0f, 0.8f, 625.0f, 0.86f, 0.6f, 3125.0f, 0.90f, 0.6f, 0.3f,
                 4725.0f, 0.96f, 0.2f, -0.85f, 5900.0f, -1.8f, -3.0f,
                 5500.0f, 4800.0f, 80.0f, 0.17f, 0.95f };
    }

    std::vector<float> createCabIRFromProfile(const MicProfile& profile, float micDistance)
    {
        constexpr float sampleRate = 48000.0f;
        constexpr int numSamples = 2048;

        std::vector<float> samples(static_cast<size_t>(numSamples), 0.0f);

        constexpr float pi = 3.14159265359f;
        const float distance = juce::jlimit(0.0f, 1.0f, micDistance);
        const float close = 1.0f - distance;

        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float>(i) / sampleRate;
            const float lowCab = std::sin(2.0f * pi * 106.0f * t + 0.20f) * std::exp(-(20.0f + 4.0f * distance) * t) * 0.46f;
            const float thump = std::sin(2.0f * pi * 148.0f * t + 0.33f) * std::exp(-(25.0f + 6.0f * distance) * t) * 0.36f;
            const float wood = std::sin(2.0f * pi * 248.0f * t + 0.54f) * std::exp(-(30.0f + 8.0f * distance) * t) * 0.11f;
            const float box = std::sin(2.0f * pi * 382.0f * t + 0.88f) * std::exp(-(35.0f + 10.0f * distance) * t) * 0.11f;
            const float cone = std::sin(2.0f * pi * 1425.0f * t + 0.42f) * std::exp(-(55.0f + 16.0f * distance) * t) * 0.11f;
            const float bark = std::sin(2.0f * pi * 2280.0f * t + 0.61f) * std::exp(-(70.0f + 24.0f * distance) * t) * 0.086f;
            const float bite = std::sin(2.0f * pi * 3475.0f * t + 1.10f) * std::exp(-(110.0f + 36.0f * distance) * t) * 0.027f;
            const float air = std::sin(2.0f * pi * 5250.0f * t + 0.76f) * std::exp(-(164.0f + 72.0f * distance) * t) * 0.007f;
            const float roomTail = std::sin(2.0f * pi * 1710.0f * t + 0.31f)
                                 * std::exp(-(15.0f - 4.0f * profile.roomAmount + 6.0f * close) * t)
                                 * 0.020f
                                 * profile.roomAmount
                                 * (0.22f + 0.78f * distance);

            const float impulse = (i == 0 ? profile.directLevel : 0.0f)
                                + lowCab
                                + thump
                                + wood
                                + box
                                + cone
                                + bark
                                + bite
                                + air
                                + roomTail;

            samples[static_cast<size_t>(i)] = juce::jlimit(-1.0f, 1.0f, impulse);
        }

        const int earlyReflection = 18 + juce::roundToInt(11.0f * distance);
        const int cabEdgeReflection = 52 + juce::roundToInt(18.0f * distance);
        const int roomReflection = 104 + juce::roundToInt(46.0f * distance);
        const int lateReflection = 176 + juce::roundToInt(84.0f * distance);

        addImpulseTap(samples, earlyReflection, 0.020f * (0.40f + 0.60f * profile.roomAmount));
        addImpulseTap(samples, cabEdgeReflection, -0.013f * (0.65f + 0.35f * close));
        addImpulseTap(samples, roomReflection, 0.012f * (0.20f + 0.80f * profile.roomAmount));
        addImpulseTap(samples, lateReflection, -0.008f * (0.12f + 0.88f * profile.roomAmount));

        applyBiquad(samples, makeHighPass(sampleRate, profile.highPassHz, 0.707f));
        applyBiquad(samples, makeLowShelf(sampleRate,
                                          145.0f,
                                          0.78f,
                                          juce::jmap(distance, profile.proximityNearDb, profile.proximityFarDb)));
        applyBiquad(samples, makePeak(sampleRate, 140.0f, 0.92f, 0.6f - 0.35f * distance));
        applyBiquad(samples, makePeak(sampleRate, profile.bodyFreq, profile.bodyQ, profile.bodyDb));
        applyBiquad(samples, makePeak(sampleRate, 360.0f, 1.10f, -1.1f + 0.3f * distance));
        applyBiquad(samples, makePeak(sampleRate,
                                      profile.presenceFreq,
                                      profile.presenceQ,
                                      juce::jmap(distance, profile.presenceNearDb, profile.presenceFarDb)));
        applyBiquad(samples, makePeak(sampleRate,
                                      profile.biteFreq,
                                      profile.biteQ,
                                      juce::jmap(distance, profile.biteNearDb, profile.biteFarDb)));
        applyBiquad(samples, makePeak(sampleRate, 2150.0f, 1.02f, 0.75f + 0.30f * close));
        applyBiquad(samples, makePeak(sampleRate, 2780.0f, 0.96f, 0.60f + 0.25f * close));
        applyBiquad(samples, makeHighShelf(sampleRate,
                                           profile.airFreq,
                                           0.74f,
                                           juce::jmap(distance, profile.airNearDb, profile.airFarDb)));
        applyBiquad(samples, makeLowPass(sampleRate,
                                         juce::jmap(distance, profile.lowPassNearHz, profile.lowPassFarHz),
                                         0.72f));
        applyBiquad(samples, makePeak(sampleRate, 2450.0f, 1.14f, -0.3f - 0.5f * distance));
        applyBiquad(samples, makePeak(sampleRate, 3950.0f, 1.08f, -0.95f - 0.95f * close));

        normalizeImpulsePeak(samples, 0.90f);

        return samples;
    }
}

namespace ParamIDs
{
    static constexpr auto input = "input";
    static constexpr auto gain = "gain";
    static constexpr auto bass = "bass";
    static constexpr auto mids = "mids";
    static constexpr auto treble = "treble";
    static constexpr auto presence = "presence";
    static constexpr auto depth = "depth";
    static constexpr auto master = "master";
    static constexpr auto cabLowCut = "cabLowCut";
    static constexpr auto cabHighCut = "cabHighCut";
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
    static constexpr auto tunerReference = "tunerReference";
    static constexpr auto tunerRange = "tunerRange";
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
    static constexpr auto voicing = "voicing";  // 0 = Rhythm, 1 = Lead
    static constexpr auto lofi = "lofi";
    static constexpr auto lofiIntensity = "lofiIntensity";
    static constexpr auto stfu = "stfu";
    static constexpr auto tapeSaturation = "tapeSaturation";
    static constexpr auto limiter = "limiter";
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

    constexpr std::array<const char*, 10> postEqBandNames {
        "31 Hz",
        "62 Hz",
        "125 Hz",
        "250 Hz",
        "500 Hz",
        "1 kHz",
        "2 kHz",
        "4 kHz",
        "8 kHz",
        "16 kHz"
    };
}

namespace StateKeys
{
    static constexpr auto irSource = "irSource";
    static constexpr auto irPath = "irPath";
    static constexpr auto programIndex = "programIndex";
}

namespace BuiltInCab
{
    static constexpr auto name = "HEXSTACK OS V30 4X12 (SM57)";
}

namespace HexPresetKeys
{
    static constexpr auto rootTag = "HEXSTACKPreset";
    static constexpr auto schemaVersion = "schemaVersion";
    static constexpr auto presetName = "presetName";
    static constexpr auto pluginStateTag = "PluginState";
}

namespace PresetData
{
    struct Preset
    {
        const char* name;
        float input;
        float gain;
        float bass;
        float mids;
        float treble;
        float presence;
        float depth;
        float master;
        bool pitchOn;
        bool wahOn;
        bool overdriveOn;
        bool reverbOn;
        bool delayOn;
        float gate;
        float driveAmount;
        float driveTone;
        float driveLevel;
        float driveMix;
        float driveTight;
        float delayTimeMs;
        float delayFeedback;
        float delayTone;
        float delayMix;
        float delayWidth;
        float reverbSize;
        float reverbDamp;
        float reverbMix;
        float reverbPreDelayMs;
        std::array<float, 10> postEq;
    };

    static const std::array<Preset, 3> presets {
        Preset { "Default", 11.0f, 0.548f, 3.65f, -3.64f, 1.71f, 1.68f, 0.796f, -15.0f,
                 false, false, false, false, false,
                 0.25f,
                 0.12f, 0.42f, 6.2f, 1.0f, 0.96f,
                 360.0f, 0.16f, 0.24f, 0.18f, 0.08f,
                 0.16f, 0.72f, 0.18f, 8.0f,
                 { -12.0f, -1.71f, 3.58f, -1.09f, -2.03f, 1.4f, 4.52f, -0.47f, -3.58f, -12.0f } },
        Preset { "DIRTY CLEAN BOY", 4.0f, 0.0f, 3.65f, -3.54f, 3.25f, 2.06f, 0.796f, -15.0f,
                 true, false, false, true, true,
                 0.0f,
                 0.12f, 0.42f, 6.2f, 1.0f, 0.96f,
                 540.0f, 0.521f, 0.088f, 0.116f, 0.08f,
                 0.296f, 0.72f, 0.38f, 12.0f,
                 { -12.0f, -1.71f, 3.58f, -1.09f, -2.03f, 1.4f, 4.52f, -0.47f, -3.58f, -12.0f } },
        Preset { "LEADer of Cola", 5.0f, 0.708f, 3.07f, -3.64f, 1.71f, 1.68f, 0.656f, -13.5f,
                 false, false, true, true, true,
                 0.37f,
                 0.644f, 0.436f, -4.36f, 1.0f, 0.0f,
                 540.0f, 0.499f, 0.248f, 0.156f, 0.0f,
                 0.492f, 0.98f, 0.284f, 12.0f,
                 { -12.0f, -1.71f, 3.58f, -0.78f, -1.4f, 1.4f, 1.4f, -0.16f, -0.78f, -12.0f } }
    };
}

namespace
{
    int inferProgramIndexFromState(const juce::ValueTree& /*tree*/)
    {
        // Default to program 0 when the saved index cannot be inferred.
        return 0;
    }
}

HexstackAudioProcessor::HexstackAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setCurrentProgram(0);
}

HexstackAudioProcessor::~HexstackAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout HexstackAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    const auto& defaultPreset = PresetData::presets.front();

    const auto makeFloatParam = [] (const char* id,
                                    const char* name,
                                    juce::NormalisableRange<float> range,
                                    float defaultValue,
                                    juce::AudioParameterFloatAttributes attributes = {})
    {
        return std::make_unique<juce::AudioParameterFloat>(id,
                                                           name,
                                                           std::move(range),
                                                           defaultValue,
                                                           attributes);
    };

    layout.push_back(makeFloatParam(ParamIDs::input,
                                    "Input",
                                    juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
                                    defaultPreset.input,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::gain,
                                    "Drive",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.gain));

    layout.push_back(makeFloatParam(ParamIDs::bass,
                                    "Bass",
                                    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                    defaultPreset.bass,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::mids,
                                    "Mid",
                                    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                    defaultPreset.mids,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::treble,
                                    "Treble",
                                    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                    defaultPreset.treble,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::presence,
                                    "Presence",
                                    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                    defaultPreset.presence,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::depth,
                                    "Depth",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.depth));

    layout.push_back(makeFloatParam(ParamIDs::master,
                                    "Master",
                                    juce::NormalisableRange<float>(-36.0f, 12.0f, 0.01f),
                                    defaultPreset.master,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));

    layout.push_back(makeFloatParam(ParamIDs::cabLowCut,
                                    "Cab Low Cut",
                                    juce::NormalisableRange<float>(20.0f, 500.0f, 0.01f, 0.35f),
                                    20.0f,
                                    juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(makeFloatParam(ParamIDs::cabHighCut,
                                    "Cab High Cut",
                                    juce::NormalisableRange<float>(1000.0f, 20000.0f, 0.01f, 0.25f),
                                    20000.0f,
                                    juce::AudioParameterFloatAttributes().withLabel("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::postEqEnabled,
                                                                 "Post EQ Enabled",
                                                                 true));

    for (size_t i = 0; i < postEqParamIds.size(); ++i)
    {
        layout.push_back(makeFloatParam(postEqParamIds[i],
                                        postEqBandNames[i],
                                        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                        defaultPreset.postEq[i],
                                        juce::AudioParameterFloatAttributes().withLabel("dB")));
    }

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::tunerReference,
        "Tuner Reference",
        juce::StringArray { "432 Hz", "440 Hz", "442 Hz" },
        1));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::tunerRange,
        "Tuner Range",
        juce::StringArray { "Wide", "Bass", "Guitar" },
        0));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxPower1, "FX PITCH Power", defaultPreset.pitchOn));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxPower2, "FX WAH Power", defaultPreset.wahOn));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxPower3, "FX OVERDRIVE Power", defaultPreset.overdriveOn));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxPower4, "FX REVERB Power", defaultPreset.reverbOn));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxPower5, "FX DELAY Power", defaultPreset.delayOn));

    layout.push_back(makeFloatParam(ParamIDs::fxPitchShift,
                                    "FX PITCH Shift",
                                    juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f),
                                    0.0f,
                                    juce::AudioParameterFloatAttributes().withLabel("st")));
    layout.push_back(makeFloatParam(ParamIDs::fxPitchMix,
                                    "FX PITCH Mix",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    1.0f));
    layout.push_back(makeFloatParam(ParamIDs::fxPitchWidth,
                                    "FX PITCH Width",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                        0.0f));
    layout.push_back(makeFloatParam(ParamIDs::fxWahFreq,
                                    "FX WAH Sweep",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    0.42f));
    layout.push_back(makeFloatParam(ParamIDs::fxWahQ,
                                    "FX WAH Q",
                                    juce::NormalisableRange<float>(0.4f, 8.0f, 0.01f),
                                    2.8f));
    layout.push_back(makeFloatParam(ParamIDs::fxWahMix,
                                    "FX WAH Mix",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    1.0f));

    layout.push_back(makeFloatParam(ParamIDs::fxDriveAmount,
                                    "FX OVERDRIVE Drive",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.driveAmount));
    layout.push_back(makeFloatParam(ParamIDs::fxDriveTone,
                                    "FX OVERDRIVE Tone",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.driveTone));
    layout.push_back(makeFloatParam(ParamIDs::fxDriveLevel,
                                    "FX OVERDRIVE Level",
                                    juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f),
                                    defaultPreset.driveLevel,
                                    juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.push_back(makeFloatParam(ParamIDs::fxDriveMix,
                                    "FX OVERDRIVE Mix",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.driveMix));
    layout.push_back(makeFloatParam(ParamIDs::fxDriveTight,
                                    "FX OVERDRIVE Tight",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.driveTight));

    layout.push_back(makeFloatParam(ParamIDs::fxDelayTimeMs,
                                    "FX DELAY Time",
                                    juce::NormalisableRange<float>(60.0f, 900.0f, 1.0f),
                                    defaultPreset.delayTimeMs,
                                    juce::AudioParameterFloatAttributes().withLabel("ms")));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::fxDelaySync,
                                                                 "FX DELAY Sync",
                                                                 false));
    layout.push_back(makeFloatParam(ParamIDs::fxDelayFeedback,
                                    "FX DELAY Feedback",
                                    juce::NormalisableRange<float>(0.0f, 0.92f, 0.001f),
                                    defaultPreset.delayFeedback));
    layout.push_back(makeFloatParam(ParamIDs::fxDelayTone,
                                    "FX DELAY Tone",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.delayTone));
    layout.push_back(makeFloatParam(ParamIDs::fxDelayMix,
                                    "FX DELAY Mix",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    0.18f));
    layout.push_back(makeFloatParam(ParamIDs::fxDelayWidth,
                                    "FX DELAY Width",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.delayWidth));

    layout.push_back(makeFloatParam(ParamIDs::fxReverbSize,
                                    "FX REVERB Size",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.reverbSize));
    layout.push_back(makeFloatParam(ParamIDs::fxReverbDamp,
                                    "FX REVERB Damp",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    defaultPreset.reverbDamp));
    layout.push_back(makeFloatParam(ParamIDs::fxReverbMix,
                                    "FX REVERB Mix",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                    0.18f));
    layout.push_back(makeFloatParam(ParamIDs::fxReverbPreDelayMs,
                                    "FX REVERB PreDelay",
                                    juce::NormalisableRange<float>(0.0f, 80.0f, 1.0f),
                                    defaultPreset.reverbPreDelayMs,
                                    juce::AudioParameterFloatAttributes().withLabel("ms")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::voicing,
                                                                 "Voicing",
                                                                 false));  // false = Rhythm, true = Lead

    layout.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::lofi,
                                                                 "LOFI",
                                                                 false));

    layout.push_back(makeFloatParam(ParamIDs::lofiIntensity,
                                    "LOFI Intensity",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                    0.0f));

    layout.push_back(makeFloatParam(ParamIDs::stfu,
                                    "Gate",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                    defaultPreset.gate));

    layout.push_back(makeFloatParam(ParamIDs::tapeSaturation,
                                    "Tape Sat",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                    0.0f));

    layout.push_back(makeFloatParam(ParamIDs::limiter,
                                    "Limiter",
                                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                    0.0f));

    return { layout.begin(), layout.end() };
}

const juce::String HexstackAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HexstackAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool HexstackAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool HexstackAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double HexstackAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HexstackAudioProcessor::getNumPrograms()
{
    return static_cast<int>(PresetData::presets.size());
}

int HexstackAudioProcessor::getCurrentProgram()
{
    return currentProgramIndex;
}

void HexstackAudioProcessor::setCurrentProgram(int index)
{
    const auto clamped = juce::jlimit(0, getNumPrograms() - 1, index);
    currentProgramIndex = clamped;
    activeHexFilePath = {}; // user chose a built-in preset; clear any loaded hex path

    const auto& preset = PresetData::presets[static_cast<size_t>(clamped)];

    const auto setFloatParam = [this](const char* id, float value)
    {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };

    const auto setBoolParam = [this](const char* id, bool enabled)
    {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
    };

    setFloatParam(ParamIDs::input, preset.input);
    setFloatParam(ParamIDs::gain, preset.gain);
    setFloatParam(ParamIDs::bass, preset.bass);
    setFloatParam(ParamIDs::mids, preset.mids);
    setFloatParam(ParamIDs::treble, preset.treble);
    setFloatParam(ParamIDs::presence, preset.presence);
    setFloatParam(ParamIDs::depth, preset.depth);
    setFloatParam(ParamIDs::master, preset.master);
    setFloatParam(ParamIDs::cabLowCut, 70.0f);
    setFloatParam(ParamIDs::cabHighCut, 5000.0f);
    setBoolParam(ParamIDs::postEqEnabled, true);
    setBoolParam(ParamIDs::fxPower1, preset.pitchOn);
    setBoolParam(ParamIDs::fxPower2, preset.wahOn);
    setBoolParam(ParamIDs::fxPower3, preset.overdriveOn);
    setBoolParam(ParamIDs::fxPower4, preset.reverbOn);
    setBoolParam(ParamIDs::fxPower5, preset.delayOn);
    setBoolParam(ParamIDs::voicing, false);  // default to Rhythm
    setBoolParam(ParamIDs::lofi, false);
    setFloatParam(ParamIDs::stfu, preset.gate);
    setFloatParam(ParamIDs::tapeSaturation, 0.0f);
    setFloatParam(ParamIDs::limiter, 0.0f);
    setFloatParam(ParamIDs::lofiIntensity, 0.0f);
    setFloatParam(ParamIDs::fxPitchShift, 0.0f);
    setFloatParam(ParamIDs::fxPitchMix, 1.0f);
    setFloatParam(ParamIDs::fxPitchWidth, 0.0f);
    setFloatParam(ParamIDs::fxWahFreq, 0.42f);
    setFloatParam(ParamIDs::fxWahQ, 2.8f);
    setFloatParam(ParamIDs::fxWahMix, 1.0f);
    setFloatParam(ParamIDs::fxDriveAmount, preset.driveAmount);
    setFloatParam(ParamIDs::fxDriveTone, preset.driveTone);
    setFloatParam(ParamIDs::fxDriveLevel, preset.driveLevel);
    setFloatParam(ParamIDs::fxDriveMix, preset.driveMix);
    setFloatParam(ParamIDs::fxDriveTight, preset.driveTight);
    setFloatParam(ParamIDs::fxDelayTimeMs, preset.delayTimeMs);
    setBoolParam(ParamIDs::fxDelaySync, false);
    setFloatParam(ParamIDs::fxDelayFeedback, preset.delayFeedback);
    setFloatParam(ParamIDs::fxDelayTone, preset.delayTone);
    setFloatParam(ParamIDs::fxDelayMix, preset.delayOn ? preset.delayMix : 0.18f);
    setFloatParam(ParamIDs::fxDelayWidth, preset.delayWidth);
    setFloatParam(ParamIDs::fxReverbSize, preset.reverbSize);
    setFloatParam(ParamIDs::fxReverbDamp, preset.reverbDamp);
    setFloatParam(ParamIDs::fxReverbMix, preset.reverbOn ? preset.reverbMix : 0.18f);
    setFloatParam(ParamIDs::fxReverbPreDelayMs, preset.reverbPreDelayMs);

    for (size_t i = 0; i < postEqParamIds.size(); ++i)
        setFloatParam(postEqParamIds[i], preset.postEq[i]);

    // Preset changes should not touch the current cabinet selection.
    // If a user loaded a third-party IR, keep it active across preset switches.
    // If the built-in cab is active, just keep its display name consistent.
    if (! currentIRFile.existsAsFile())
        currentIRName = BuiltInCab::name;

    presetChangeResetPending.store(true, std::memory_order_release);
}

void HexstackAudioProcessor::resetRuntimeVoicingState()
{
    for (auto& filter : inputHighPassFilters) filter.reset();
    for (auto& filter : preampTightFilters) filter.reset();
    for (auto& filter : bassShelfFilters) filter.reset();
    for (auto& filter : midPeakFilters) filter.reset();
    for (auto& filter : trebleShelfFilters) filter.reset();
    for (auto& filter : presenceShelfFilters) filter.reset();
    for (auto& filter : outputLowPassFilters) filter.reset();
    for (auto& filter : cabLowCutFilters) filter.reset();
    for (auto& filter : cabHighCutFilters) filter.reset();
    for (auto& filter : thumpResonanceFilters) filter.reset();
    for (auto& filter : depthShelfFilters) filter.reset();
    for (auto& filter : wahBandPassFilters) filter.reset();
    for (auto& bandFilters : postEqFilters)
        for (auto& filter : bandFilters)
            filter.reset();

    // NOTE: primaryCabConvolution.reset() is intentionally omitted here.
    // Calling reset() on the audio thread while JUCE's background IR-loading
    // thread is mid-crossfade causes a race condition that produces the
    // "toilet bowl" artifact.  The 50 ms fade-in ramp below already silences
    // any transition transient, so the convolution can manage its own crossfade.
    reverb.reset();

    if (pitchBuffer.getNumSamples() > 0)
        pitchBuffer.clear();
    if (delayBuffer.getNumSamples() > 0)
        delayBuffer.clear();
    if (reverbBuffer.getNumSamples() > 0)
        reverbBuffer.clear();
    if (reverbPreDelayBuffer.getNumSamples() > 0)
        reverbPreDelayBuffer.clear();

    delayWritePosition = 0;
    pitchWritePosition = 0;
    reverbPreDelayWritePosition = 0;
    pitchReadOffset = 0.0f;

    overdriveToneState.fill(0.0f);
    overdriveTightState.fill(0.0f);
    delayToneState.fill(0.0f);
    delayHighPassState.fill(0.0f);
    delayDuckEnvelope.fill(0.0f);
    reverbHighPassState.fill(0.0f);
    reverbLowPassState.fill(0.0f);
    reverbDuckEnvelope.fill(0.0f);
    lofiHighPassState.fill(0.0f);
    lofiLowPassState.fill(0.0f);
    lofiMidState.fill(0.0f);
    lofiSampleHoldCounter.fill(0);
    lofiHeldSample.fill(0.0f);
    stfuEnvelope.fill(0.0f);
    stfuGain.fill(1.0f);
    limiterEnvelope.fill(0.0f);
    stfuHoldCounter.fill(0);
    stfuHpfX.fill(0.0f);
    stfuHpfY.fill(0.0f);
    pickTransientState.fill(0.0f);
    pitchChorusPhase  = 0.0f;
    pitchChorusPhase2 = juce::MathConstants<float>::pi;
    tunerCommittedMidiNote = -1;
    std::fill(std::begin(tunerRawFreqHistory), std::end(tunerRawFreqHistory), 0.0f);
    tunerRawFreqHistoryPos   = 0;
    tunerRawFreqHistoryCount = 0;
    // 50 ms fade-in — covers JUCE convolution crossfade time and any single-block
    // parameter-transition artifact that slips through before the flag is consumed.
    presetTransitionRampRemainingSamples = juce::jmax(0, juce::roundToInt(currentSampleRate * 0.050));
}

void HexstackAudioProcessor::requestCabTransitionReset()
{
    presetChangeResetPending.store(true, std::memory_order_release);
}

const juce::String HexstackAudioProcessor::getProgramName(int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};

    return PresetData::presets[static_cast<size_t>(index)].name;
}

void HexstackAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void HexstackAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentMaxBlockSize = samplesPerBlock;
    currentNumChannels = juce::jmax(1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    for (auto& filter : inputHighPassFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : preampTightFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : bassShelfFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : midPeakFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : trebleShelfFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : presenceShelfFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : outputLowPassFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : cabLowCutFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : cabHighCutFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : thumpResonanceFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : depthShelfFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& filter : wahBandPassFilters) { filter.reset(); filter.prepare(spec); }
    for (auto& bandFilters : postEqFilters)
        for (auto& filter : bandFilters) { filter.reset(); filter.prepare(spec); }

    reverb.reset();

    pitchBuffer.setSize(1,
                        juce::jmax(samplesPerBlock * 8,
                                   juce::roundToInt(currentSampleRate * 0.12)));
    pitchBuffer.clear();
    delayBuffer.setSize(currentNumChannels,
                        juce::jmax(samplesPerBlock * 2,
                                   juce::roundToInt(currentSampleRate * 2.5)));
    delayBuffer.clear();
    reverbBuffer.setSize(currentNumChannels, samplesPerBlock);
    reverbBuffer.clear();
    reverbPreDelayBuffer.setSize(currentNumChannels,
                                 juce::jmax(samplesPerBlock * 2,
                                            juce::roundToInt(currentSampleRate * 0.35)));
    reverbPreDelayBuffer.clear();
    delayWritePosition = 0;
    pitchWritePosition = 0;
    reverbPreDelayWritePosition = 0;
    overdriveToneState.fill(0.0f);
    overdriveTightState.fill(0.0f);
    pitchReadOffset   = 0.0f;
    pitchChorusPhase  = 0.0f;
    pitchChorusPhase2 = juce::MathConstants<float>::pi;
    delayToneState.fill(0.0f);
    delayHighPassState.fill(0.0f);
    delayDuckEnvelope.fill(0.0f);
    reverbHighPassState.fill(0.0f);
    reverbLowPassState.fill(0.0f);
    reverbDuckEnvelope.fill(0.0f);
    lofiHighPassState.fill(0.0f);
    lofiLowPassState.fill(0.0f);
    lofiMidState.fill(0.0f);
    lofiSampleHoldCounter.fill(0);
    lofiHeldSample.fill(0.0f);
    stfuEnvelope.fill(0.0f);
    stfuGain.fill(1.0f);
    limiterEnvelope.fill(0.0f);
    stfuHoldCounter.fill(0);
    stfuHpfX.fill(0.0f);
    stfuHpfY.fill(0.0f);
    pickTransientState.fill(0.0f);

    constexpr int tunerCaptureSize = 16384;
    constexpr int tunerWindowSize = 8192;
    tunerCaptureBuffer.assign(tunerCaptureSize, 0.0f);
    tunerWindowBuffer.assign(tunerWindowSize, 0.0f);
    // Pre-allocate YIN work buffers at half-window size to avoid audio-thread heap alloc.
    const int yinHalf = tunerWindowSize / 2;
    yinDiffBuf.assign(static_cast<size_t>(yinHalf + 1), 0.0f);
    yinNormBuf.assign(static_cast<size_t>(yinHalf + 1), 0.0f);
    tunerCaptureWritePos = 0;
    tunerCaptureValidSamples = 0;
    tunerSamplesSinceLastAnalysis = 0;
    tunerSmoothedFrequencyHz = 0.0f;
    tunerLastMidiNote = -1;
    tunerStableFrames = 0;
    tunerSilenceFrames = 0;
    tunerCommittedMidiNote = -1;
    std::fill(std::begin(tunerRawFreqHistory), std::end(tunerRawFreqHistory), 0.0f);
    tunerRawFreqHistoryPos   = 0;
    tunerRawFreqHistoryCount = 0;

    lastPrimaryMicParamIndex = -1;
    lastTunerReferenceIndex = -1;
    lastTunerRangeParamIndex = -1;

    prepareCabinetConvolution();
    applyAutomatableControlState(true);
    presetChangeResetPending.store(false, std::memory_order_release);
    presetTransitionRampRemainingSamples = 0;
}

void HexstackAudioProcessor::releaseResources()
{
    pitchBuffer.setSize(0, 0);
    delayBuffer.setSize(0, 0);
    reverbBuffer.setSize(0, 0);
    reverbPreDelayBuffer.setSize(0, 0);
    tunerCaptureBuffer.clear();
    tunerWindowBuffer.clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HexstackAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}
#endif

void HexstackAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;

    if (presetChangeResetPending.exchange(false, std::memory_order_acq_rel))
        resetRuntimeVoicingState();

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Treat the amp front-end like a mono guitar input so the hosted plugin
    // behaves consistently on stereo DAW tracks instead of distorting the left
    // and right channels independently.
    if (totalNumInputChannels > 1)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float mono = 0.0f;

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
                mono += buffer.getReadPointer(channel)[sample];

            mono /= static_cast<float>(totalNumInputChannels);

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
                buffer.getWritePointer(channel)[sample] = mono;
        }
    }

    applyAutomatableControlState(false);

    if (tunerAnalysisEnabled.load(std::memory_order_relaxed))
        analyzeTunerInput(buffer);

    if (tunerOutputMuted.load(std::memory_order_relaxed))
    {
        buffer.clear();
        return;
    }

    const auto inputDb = parameters.getRawParameterValue(ParamIDs::input)->load();
    const auto gain = parameters.getRawParameterValue(ParamIDs::gain)->load();
    const auto bassDb = parameters.getRawParameterValue(ParamIDs::bass)->load();
    const auto midsDb = parameters.getRawParameterValue(ParamIDs::mids)->load();
    const auto trebleDb = parameters.getRawParameterValue(ParamIDs::treble)->load();
    const auto presenceDb = parameters.getRawParameterValue(ParamIDs::presence)->load();
    const auto depth = parameters.getRawParameterValue(ParamIDs::depth)->load();
    const auto masterDb = parameters.getRawParameterValue(ParamIDs::master)->load();
    const auto cabLowCutHz = parameters.getRawParameterValue(ParamIDs::cabLowCut)->load();
    const auto cabHighCutHz = parameters.getRawParameterValue(ParamIDs::cabHighCut)->load();
    const bool postEqEnabled = getParameterValue(ParamIDs::postEqEnabled, 1.0f) >= 0.5f;
    const bool pitchOn = isFxPedalEnabled(0);
    const bool wahOn = isFxPedalEnabled(1);
    const bool overdriveOn = isFxPedalEnabled(2);
    const bool reverbOn = isFxPedalEnabled(3);
    const bool delayOn = isFxPedalEnabled(4);
    const bool lofiOn = getParameterValue(ParamIDs::lofi, 0.0f) >= 0.5f;
    const float stfuKnobVal = getParameterValue(ParamIDs::stfu, 0.0f);
    const float tapeSatIntensity = juce::jlimit(0.0f, 1.0f, getParameterValue(ParamIDs::tapeSaturation, 0.0f));

    const float pitchShift = getParameterValue(ParamIDs::fxPitchShift, -2.0f);
    const float pitchMix = getParameterValue(ParamIDs::fxPitchMix, 0.82f);
    const float pitchWidth = getParameterValue(ParamIDs::fxPitchWidth, 0.0f);
    const float wahFreq = getParameterValue(ParamIDs::fxWahFreq, 0.42f);
    const float wahQ = getParameterValue(ParamIDs::fxWahQ, 2.8f);
    const float wahMix = getParameterValue(ParamIDs::fxWahMix, 0.98f);
    const float overdriveAmount = getParameterValue(ParamIDs::fxDriveAmount, 0.54f);
    const float overdriveTone = getParameterValue(ParamIDs::fxDriveTone, 0.42f);
    const float overdriveLevelDb = getParameterValue(ParamIDs::fxDriveLevel, 2.2f);
    const float overdriveMix = getParameterValue(ParamIDs::fxDriveMix, 0.88f);
    const float overdriveTight = getParameterValue(ParamIDs::fxDriveTight, 0.76f);
    const float delayTimeMs = getParameterValue(ParamIDs::fxDelayTimeMs, 420.0f);
    const float delayFeedback = getParameterValue(ParamIDs::fxDelayFeedback, 0.30f);
    const float delayTone = getParameterValue(ParamIDs::fxDelayTone, 0.36f);
    const float delayMix = getParameterValue(ParamIDs::fxDelayMix, 0.25f);
    const float delayWidth = getParameterValue(ParamIDs::fxDelayWidth, 0.48f);
    const float reverbSize = getParameterValue(ParamIDs::fxReverbSize, 0.28f);
    const float reverbDamp = getParameterValue(ParamIDs::fxReverbDamp, 0.62f);
    const float reverbMix = getParameterValue(ParamIDs::fxReverbMix, 0.14f);
    const float reverbPreDelayMs = getParameterValue(ParamIDs::fxReverbPreDelayMs, 38.0f);

    std::array<float, 10> postEqBandGainDb {};
    for (size_t i = 0; i < postEqParamIds.size(); ++i)
        postEqBandGainDb[i] = getParameterValue(postEqParamIds[i], 0.0f);

    if (pitchOn)
        processPitchEffect(buffer, pitchShift, pitchMix, pitchWidth);

    const bool presetIsLead   = getParameterValue(ParamIDs::voicing, 0.0f) >= 0.5f;
    const bool presetIsRhythm = !presetIsLead;
    const float rhythmBias = presetIsRhythm ? 1.0f : 0.0f;
    const float leadBias   = presetIsLead   ? 1.0f : 0.0f;

    const float inputGain = juce::Decibels::decibelsToGain(inputDb);
    const float outputGain = juce::Decibels::decibelsToGain(masterDb);
    const bool modernHighGainVoicing = overdriveOn || gain >= 0.58f || presetIsRhythm || presetIsLead;
    const float ampFrontEndBoostDb = juce::jmap(gain,
                                                modernHighGainVoicing ? (4.0f + 0.8f * rhythmBias - 0.3f * leadBias) : 0.6f,
                                                modernHighGainVoicing ? (10.0f + 1.4f * rhythmBias - 0.5f * leadBias) : 3.0f);
    const float ampInputGain = inputGain * juce::Decibels::decibelsToGain(ampFrontEndBoostDb);
    const float preampGain = juce::jmap(gain,
                                                     modernHighGainVoicing ? 4.4f : 2.5f,
                                                     modernHighGainVoicing ? 29.5f : 22.0f);
    const float stage2Gain = juce::jmap(gain,
                                                     modernHighGainVoicing ? 3.1f : 2.1f,
                                                     modernHighGainVoicing ? 13.6f : 10.0f);
    const float powerAmpDrive = juce::jmap(gain,
                                                         modernHighGainVoicing ? 1.08f : 1.18f,
                                                         modernHighGainVoicing ? 2.28f : 3.02f);
     const float ampLevelCompensation = juce::jmap(gain,
                                                                  modernHighGainVoicing ? 1.01f : 1.05f,
                                                                  modernHighGainVoicing ? 0.82f : 0.88f);
    const float tightLowCutHz = juce::jmap(gain,
                                                         modernHighGainVoicing ? (110.0f + 14.0f * rhythmBias - 6.0f * leadBias) : 82.0f,
                                                         modernHighGainVoicing ? (170.0f + 24.0f * rhythmBias - 10.0f * leadBias) : 122.0f);
    const float preampTightCutHz = juce::jmap(gain,
                                                             modernHighGainVoicing ? (215.0f + 20.0f * rhythmBias - 12.0f * leadBias) : 142.0f,
                                                             modernHighGainVoicing ? (360.0f + 28.0f * rhythmBias - 16.0f * leadBias) : 255.0f);
    const float overdriveDriveCurve = std::pow(juce::jlimit(0.0f, 1.0f, overdriveAmount), 0.78f);
    const float overdriveToneCurve = std::pow(juce::jlimit(0.0f, 1.0f, overdriveTone), 0.88f);
    const float overdriveTightCurve = std::pow(juce::jlimit(0.0f, 1.0f, overdriveTight), 0.92f);
    const float overdriveGain = juce::jmap(overdriveDriveCurve,
                                                                                     1.65f,
                                                                                     13.8f + 0.5f * rhythmBias);
    const float overdriveLevelGain = juce::Decibels::decibelsToGain(overdriveLevelDb);
    const float overdriveToneHz = juce::jmap(overdriveToneCurve,
                                                                                         880.0f - 70.0f * rhythmBias,
                                                                                         3350.0f + 120.0f * leadBias);
    const float overdriveToneAlpha = std::exp(-2.0f * juce::MathConstants<float>::pi * overdriveToneHz
                                              / static_cast<float>(currentSampleRate));
        const float overdriveTightHz = juce::jmap(overdriveTightCurve, 320.0f, 1180.0f);
    const float overdriveTightAlpha = std::exp(-2.0f * juce::MathConstants<float>::pi * overdriveTightHz
                                               / static_cast<float>(currentSampleRate));

    // STFU noise suppressor — downward expander with HPF sidechain
    // The sidechain runs through an 80 Hz high-pass filter so low-frequency
    // pickup/amp hum doesn't hold the gate open between notes.
    // Hysteresis: opens at threshold, doesn't start closing until -4 dB below.
    if (stfuKnobVal > 0.005f)
    {
        const float openThreshDb  = -70.0f + stfuKnobVal * 55.0f + 1.5f * rhythmBias - 2.0f * leadBias;
        const float openThresh    = juce::Decibels::decibelsToGain(openThreshDb);
        const float closeThresh   = openThresh * juce::Decibels::decibelsToGain(-4.0f); // 4 dB hysteresis

        const float envWindowSeconds  = 0.0048f - 0.0008f * rhythmBias + 0.0008f * leadBias;
        const float attackSeconds     = 0.0016f - 0.0003f * rhythmBias + 0.0002f * leadBias;
        const float releaseSeconds    = juce::jmap(stfuKnobVal, 0.180f, 0.065f) + 0.060f * leadBias;
        const float holdSeconds       = 0.080f + 0.010f * leadBias;
        const int   holdSamples       = juce::roundToInt(holdSeconds * static_cast<float>(currentSampleRate));
        const float expansionPow      = 2.5f + 4.5f * stfuKnobVal;
        // Minimum gain floor: -80 dB — never fully mutes to avoid hard click artefacts.
        const float minGain           = juce::Decibels::decibelsToGain(-80.0f);

        const float envCoeff    = std::exp(-1.0f / (envWindowSeconds * static_cast<float>(currentSampleRate)));
        const float attackCoeff = std::exp(-1.0f / (attackSeconds    * static_cast<float>(currentSampleRate)));
        const float releaseCoeff= std::exp(-1.0f / (releaseSeconds   * static_cast<float>(currentSampleRate)));

        // Sidechain HPF: 1-pole bilinear at 80 Hz to reject pickup/amp hum.
        const float hpfFc   = 80.0f;
        const float hpfR    = 1.0f / std::tan(juce::MathConstants<float>::pi * hpfFc / static_cast<float>(currentSampleRate));
        const float hpfAlpha = hpfR / (hpfR + 1.0f);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            const auto ch = static_cast<size_t>(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                // --- Sidechain: HPF the input to remove low-freq hum ---
                const float xIn       = data[sample];
                const float hpfOut    = hpfAlpha * (stfuHpfY[ch] + xIn - stfuHpfX[ch]);
                stfuHpfX[ch]          = xIn;
                stfuHpfY[ch]          = hpfOut;

                const float envIn = std::abs(hpfOut);
                stfuEnvelope[ch] = envCoeff * stfuEnvelope[ch] + (1.0f - envCoeff) * envIn;

                float targetGain;
                if (stfuEnvelope[ch] >= openThresh)
                {
                    // Above open threshold: gate fully open, reset hold timer.
                    targetGain = 1.0f;
                    stfuHoldCounter[ch] = holdSamples;
                }
                else if (stfuHoldCounter[ch] > 0)
                {
                    // Hold phase: wait before releasing so note tail rings out.
                    targetGain = 1.0f;
                    --stfuHoldCounter[ch];
                }
                else if (stfuEnvelope[ch] >= closeThresh)
                {
                    // Hysteresis zone: between closeThresh and openThresh — hold open.
                    targetGain = 1.0f;
                }
                else
                {
                    // Release phase: downward expansion with floor.
                    const float r = stfuEnvelope[ch] / juce::jmax(1e-9f, closeThresh);
                    targetGain = juce::jmax(minGain, std::pow(r, expansionPow));
                }

                const float coeff = (targetGain > stfuGain[ch]) ? attackCoeff : releaseCoeff;
                stfuGain[ch] = coeff * stfuGain[ch] + (1.0f - coeff) * targetGain;
                data[sample] *= stfuGain[ch];
            }
        }
    }

    const auto inputHP = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, tightLowCutHz, 0.707f);
    const float midPeakFreq = 870.0f + 110.0f * leadBias + 55.0f * rhythmBias;
    const float trebleShelfFreq = 3400.0f + 220.0f * leadBias - 40.0f * rhythmBias;
    const float presenceFreq = 3850.0f - 210.0f * leadBias - 20.0f * rhythmBias;
    const float presenceScale = 0.60f + 0.08f * leadBias + 0.03f * rhythmBias;
    const float outputLpStart = 9800.0f + 620.0f * leadBias - 100.0f * rhythmBias;
    const float outputLpEnd = 6600.0f + 520.0f * leadBias - 40.0f * rhythmBias;
    const float thumpResonanceFreq = 96.0f + 8.0f * rhythmBias - 4.0f * leadBias;
    const float thumpResonanceDb = juce::jlimit(0.0f,
                                                4.6f,
                                                1.0f + 1.20f * depth + 0.90f * gain + 0.45f * rhythmBias - 0.18f * leadBias);
    const float depthShelfDb = juce::jmap(juce::jlimit(0.0f, 1.0f, depth),
                                          -0.4f + 0.6f * rhythmBias,
                                          6.2f + 0.8f * rhythmBias + 0.7f * leadBias);
    const float pickTransientHz = 940.0f + 170.0f * rhythmBias - 85.0f * leadBias;
    const float pickTransientAlpha = std::exp(-2.0f * juce::MathConstants<float>::pi * pickTransientHz
                                              / static_cast<float>(currentSampleRate));
    const float powerBloomDrive = juce::jmap(gain,
                                             modernHighGainVoicing ? (0.98f + 0.04f * rhythmBias) : 1.04f,
                                             modernHighGainVoicing ? (1.18f + 0.10f * rhythmBias + 0.04f * leadBias) : 1.28f);
    const float powerBloomBlend = juce::jlimit(0.0f,
                                               0.38f,
                                               0.10f + 0.05f * rhythmBias + 0.03f * leadBias + 0.06f * depth);
    const float cabPunchDrive = 1.06f + 0.10f * gain + 0.08f * depth + 0.04f * rhythmBias;
    const float cabPunchBlend = juce::jlimit(0.0f,
                                             0.38f,
                                             0.12f + 0.10f * depth + 0.04f * rhythmBias + 0.02f * leadBias);

    const auto bassShelf = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate, 115.0f, 0.72f, juce::Decibels::decibelsToGain(bassDb));
    const auto midPeak = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, midPeakFreq, 0.82f, juce::Decibels::decibelsToGain(midsDb));
    const auto trebleShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate, trebleShelfFreq, 0.72f, juce::Decibels::decibelsToGain(trebleDb));
    const auto presenceShelf = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                                    presenceFreq,
                                                                                    0.78f,
                                                                                    juce::Decibels::decibelsToGain(presenceDb * presenceScale));
    const auto thumpResonance = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                                     thumpResonanceFreq,
                                                                                     0.88f,
                                                                                     juce::Decibels::decibelsToGain(thumpResonanceDb));
    const auto cabLowCut = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate,
                                                                              juce::jlimit(20.0f, 500.0f, cabLowCutHz),
                                                                              0.707f);
    const auto cabHighCut = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate,
                                                                              juce::jlimit(1000.0f, 20000.0f, cabHighCutHz),
                                                                              0.707f);
    const auto preampTight = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, preampTightCutHz, 0.66f);
    const auto depthShelf = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate,
                                                                               110.0f,
                                                                               0.68f,
                                                                               juce::Decibels::decibelsToGain(depthShelfDb));
    const auto outputLP = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate,
                                                                           juce::jmap(gain,
                                                                                                                                                                            modernHighGainVoicing ? outputLpStart : 9800.0f,
                                                                                                                                                                            modernHighGainVoicing ? outputLpEnd : 6800.0f),
                                                                           0.707f);
        const auto wahCenterHz = 450.0f + std::pow(juce::jlimit(0.0f, 1.0f, wahFreq), 1.08f) * (1600.0f - 450.0f);
    const auto wahBandPass = juce::dsp::IIR::Coefficients<float>::makeBandPass(currentSampleRate,
                                                                                wahCenterHz,
                                                                                                                                                                juce::jlimit(1.2f, 5.5f, wahQ));

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        inputHighPassFilters[channel].coefficients = inputHP;
        preampTightFilters[channel].coefficients = preampTight;
        bassShelfFilters[channel].coefficients = bassShelf;
        midPeakFilters[channel].coefficients = midPeak;
        trebleShelfFilters[channel].coefficients = trebleShelf;
        presenceShelfFilters[channel].coefficients = presenceShelf;
        outputLowPassFilters[channel].coefficients = outputLP;
        cabLowCutFilters[channel].coefficients = cabLowCut;
        cabHighCutFilters[channel].coefficients = cabHighCut;
        thumpResonanceFilters[channel].coefficients = thumpResonance;
        depthShelfFilters[channel].coefficients = depthShelf;
        wahBandPassFilters[channel].coefficients = wahBandPass;

        auto* channelData = buffer.getWritePointer(channel);

        // Gain ramp start values — computed once per channel from the saved previous-block values
        // so both channels follow the same ramp trajectory (identical t for same sample index).
        const int numBlockSamples = buffer.getNumSamples();
        const float rampScale = numBlockSamples > 1 ? 1.0f / static_cast<float>(numBlockSamples - 1) : 0.0f;
        const float ampInputGainStart  = prevAmpInputGainLinear;
        const float preampGainStart    = prevPreampGain;
        const float stage2GainStart    = prevStage2Gain;
        const float powerAmpDriveStart = prevPowerAmpDrive;

        for (int sample = 0; sample < numBlockSamples; ++sample)
        {
            const float t = static_cast<float>(sample) * rampScale;
            const float smoothedAmpInputGain  = ampInputGainStart  + t * (ampInputGain  - ampInputGainStart);
            const float smoothedPreampGain    = preampGainStart    + t * (preampGain    - preampGainStart);
            const float smoothedStage2Gain    = stage2GainStart    + t * (stage2Gain    - stage2GainStart);
            const float smoothedPowerAmpDrive = powerAmpDriveStart + t * (powerAmpDrive - powerAmpDriveStart);
            float pre = channelData[sample];

            if (wahOn)
            {
                const float dimeInput = juce::jlimit(-1.0f, 1.0f, pre * 0.98f);
                const float wahWet = wahBandPassFilters[channel].processSample(dimeInput);
                const float wahDrive = juce::jmap(juce::jlimit(1.2f, 5.5f, wahQ), 1.18f, 1.55f);
                const float wahVoiced = juce::jlimit(-1.0f,
                                                     1.0f,
                                                     std::tanh(wahWet * wahDrive) * 0.90f + pre * 0.18f);
                pre = juce::jmap(juce::jlimit(0.0f, 1.0f, wahMix), pre, wahVoiced);
            }

            if (overdriveOn)
            {
                overdriveTightState[static_cast<size_t>(channel)] = overdriveTightAlpha * overdriveTightState[static_cast<size_t>(channel)]
                                                                  + (1.0f - overdriveTightAlpha) * pre;
                const float tightened = pre - overdriveTightState[static_cast<size_t>(channel)];
                const float dryKeep = juce::jmap(overdriveTightCurve, 0.16f, 0.30f);
                const float screamerInput = tightened * (1.20f + 0.12f * overdriveDriveCurve)
                                          + pre * dryKeep
                                          + (0.018f + 0.014f * overdriveDriveCurve);
                const float clipped = std::tanh(screamerInput * overdriveGain)
                                    - std::tanh(0.016f * overdriveGain);
                const float filtered = (1.0f - overdriveToneAlpha) * clipped
                                     + overdriveToneAlpha * overdriveToneState[static_cast<size_t>(channel)];
                overdriveToneState[static_cast<size_t>(channel)] = filtered;

                const float midPush = juce::jlimit(-1.0f,
                                                   1.0f,
                                                   filtered * (1.22f + 0.10f * overdriveDriveCurve)
                                                   + clipped * 0.08f
                                                   + pre * (0.05f - 0.015f * overdriveToneCurve));
                const float ratEdge = std::tanh((midPush * (1.06f + 0.08f * overdriveToneCurve) + clipped * 0.05f)
                                                * juce::jmap(overdriveDriveCurve, 0.98f, 1.08f));
                const float brightBlend = juce::jmap(overdriveToneCurve, 0.06f, 0.42f);
                const float voiced = juce::jmap(brightBlend,
                                                midPush,
                                                ratEdge * (0.96f + 0.03f * overdriveToneCurve));
                const float driven = std::tanh(voiced * (1.00f + 0.08f * overdriveDriveCurve)) * overdriveLevelGain;
                pre = juce::jmap(juce::jlimit(0.0f, 1.0f, overdriveMix), pre, driven);
            }

            float x = inputHighPassFilters[channel].processSample(pre * smoothedAmpInputGain);
            x = preampTightFilters[channel].processSample(x);

            pickTransientState[static_cast<size_t>(channel)] = pickTransientAlpha * pickTransientState[static_cast<size_t>(channel)]
                                                              + (1.0f - pickTransientAlpha) * x;
            const float pickTransient = x - pickTransientState[static_cast<size_t>(channel)];
            x += pickTransient * (0.08f + 0.04f * rhythmBias - 0.01f * leadBias);

            const float asym = x + 0.15f * juce::jlimit(-1.0f, 1.0f, x) + 0.028f * x * x - 0.010f * x * x * x;
            const float stage1 = std::tanh((asym + 0.045f * asym * asym) * smoothedPreampGain);
            const float stage2Input = 0.96f * stage1 + 0.24f * stage1 * stage1 - 0.15f * stage1 * stage1 * stage1;
            const float stage2 = std::tanh(stage2Input * smoothedStage2Gain);
            const float grindStage = std::tanh((1.04f * stage2 + 0.16f * stage1 - 0.08f * stage2 * stage2 * stage2)
                                               * (1.08f + 0.10f * rhythmBias));
            const float stage3Seed = 0.68f * grindStage + 0.24f * stage2 + 0.16f * stage1;
            const float stage3 = std::tanh((stage3Seed - 0.06f * stage3Seed * stage3Seed * stage3Seed) * smoothedPowerAmpDrive);
            const float powerBloom = std::tanh((0.82f * stage3 + 0.22f * stage2 + 0.08f * stage1)
                                               * powerBloomDrive);
            float shaped = 0.24f * stage1 + 0.54f * stage2 + 0.24f * stage3 + 0.22f * grindStage + powerBloomBlend * powerBloom;

            const float powerCompression = 1.0f / (1.0f + (0.14f + 0.14f * gain - 0.01f * leadBias) * std::abs(shaped));
            shaped *= powerCompression;
            shaped *= ampLevelCompensation;

            shaped = bassShelfFilters[channel].processSample(shaped);
            shaped = midPeakFilters[channel].processSample(shaped);
            shaped = trebleShelfFilters[channel].processSample(shaped);
            shaped = presenceShelfFilters[channel].processSample(shaped);
            shaped = outputLowPassFilters[channel].processSample(shaped);

            channelData[sample] = juce::jlimit(-1.08f, 1.08f, shaped);
        }
    }

    // Commit smoothed gain targets after processing all channels.
    prevAmpInputGainLinear = ampInputGain;
    prevPreampGain         = preampGain;
    prevStage2Gain         = stage2Gain;
    prevPowerAmpDrive      = powerAmpDrive;

    juce::dsp::AudioBlock<float> wetBlock(buffer);
    juce::dsp::ProcessContextReplacing<float> wetContext(wetBlock);
    primaryCabConvolution.process(wetContext);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float cabSample = data[sample];
            const float cabPushed = std::tanh((cabSample + 0.06f * cabSample * cabSample) * cabPunchDrive) * 1.02f;
            float speakerVoiced = juce::jmap(cabPunchBlend, cabSample, cabPushed);

            // Tape saturation — applied before the cab low/high-cut filters so that
            // any added harmonics are shaped by the same frequency roll-off as the cab.
            // Uses tanh soft-clipping with a small 2nd-harmonic asymmetry bias to
            // mimic the even-order distortion characteristic of ferric tape saturation.
            if (tapeSatIntensity > 0.005f)
            {
                const float drive      = 1.0f + tapeSatIntensity * 4.0f;  // 1.0 – 5.0
                const float asymBias   = 0.06f * tapeSatIntensity;        // small 2nd-harmonic term
                const float driven     = speakerVoiced * drive
                                       + asymBias * speakerVoiced * speakerVoiced;
                // Normalise: tanh(drive) is the saturated output for a unity input;
                // dividing by it keeps perceived level approximately constant.
                const float makeupGain = 1.0f / std::tanh(drive);
                speakerVoiced = juce::jlimit(-1.25f, 1.25f, std::tanh(driven) * makeupGain);
            }

            const float cabTrimmed = cabHighCutFilters[channel].processSample(cabLowCutFilters[channel].processSample(speakerVoiced));
            const float thumped = thumpResonanceFilters[channel].processSample(cabTrimmed);
            data[sample] = juce::jlimit(-1.25f, 1.25f, depthShelfFilters[channel].processSample(thumped));
        }
    }

    if (postEqEnabled)
    {
        const auto eq31 = juce::dsp::IIR::Coefficients<float>::makeLowShelf(currentSampleRate,
                                                                            45.0f,
                                                                            0.72f,
                                                                            juce::Decibels::decibelsToGain(postEqBandGainDb[0]));
        const auto eq62 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                              62.5f,
                                                                              1.10f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[1]));
        const auto eq125 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                               125.0f,
                                                                               1.10f,
                                                                               juce::Decibels::decibelsToGain(postEqBandGainDb[2]));
        const auto eq250 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                               250.0f,
                                                                               1.15f,
                                                                               juce::Decibels::decibelsToGain(postEqBandGainDb[3]));
        const auto eq500 = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                               500.0f,
                                                                               1.15f,
                                                                               juce::Decibels::decibelsToGain(postEqBandGainDb[4]));
        const auto eq1k = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                              1000.0f,
                                                                              1.18f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[5]));
        const auto eq2k = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                              2000.0f,
                                                                              1.20f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[6]));
        const auto eq4k = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                              4000.0f,
                                                                              1.22f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[7]));
        const auto eq8k = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate,
                                                                              8000.0f,
                                                                              1.05f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[8]));
        const auto eq16k = juce::dsp::IIR::Coefficients<float>::makeHighShelf(currentSampleRate,
                                                                              12000.0f,
                                                                              0.72f,
                                                                              juce::Decibels::decibelsToGain(postEqBandGainDb[9]));

        const std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 10> eqCoefficients {
            eq31, eq62, eq125, eq250, eq500, eq1k, eq2k, eq4k, eq8k, eq16k
        };

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            for (size_t band = 0; band < eqCoefficients.size(); ++band)
                postEqFilters[band][static_cast<size_t>(channel)].coefficients = eqCoefficients[band];

            auto* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                float y = data[sample];

                for (size_t band = 0; band < eqCoefficients.size(); ++band)
                    y = postEqFilters[band][static_cast<size_t>(channel)].processSample(y);

                data[sample] = juce::jlimit(-1.3f, 1.3f, y);
            }
        }
    }

    if (delayOn && delayBuffer.getNumSamples() > 8)
    {
        const int delayBufferSize = delayBuffer.getNumSamples();
        const bool delaySync = getParameterValue(ParamIDs::fxDelaySync, 0.0f) >= 0.5f;
        const float effectiveDelayTimeMs = delaySync ? getHostSyncedDelayTimeMs(delayTimeMs)
                                                     : delayTimeMs;
        const int delaySamples = juce::jlimit(1,
                                              delayBufferSize - 1,
                                              juce::roundToInt(effectiveDelayTimeMs * 0.001f
                                                               * static_cast<float>(currentSampleRate)));
        const float feedback = juce::jlimit(0.0f, 0.92f, delayFeedback);
        const float mix = juce::jlimit(0.0f, 1.0f, delayMix);
        const float width = juce::jlimit(0.0f, 1.0f, delayWidth);
        const float delayToneHz = juce::jmap(delayTone, 1500.0f, 5200.0f);
        const float delayToneAlpha = std::exp(-2.0f * juce::MathConstants<float>::pi * delayToneHz
                                              / static_cast<float>(currentSampleRate));
        const float delayHighPassHz = juce::jlimit(100.0f, 220.0f, 150.0f + delayFeedback * 60.0f);
        const float delayHpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * delayHighPassHz
                            / static_cast<float>(currentSampleRate));

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const int readPos = (delayWritePosition + delayBufferSize - delaySamples) % delayBufferSize;

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                auto* data = buffer.getWritePointer(channel);
                auto* delayData = delayBuffer.getWritePointer(channel);
                const auto* oppositeDelayData = delayBuffer.getReadPointer(totalNumInputChannels > 1 ? 1 - channel : channel);

                const float dry = data[sample];
                const float delayed = delayData[readPos];
                const float oppositeDelayed = oppositeDelayData[readPos];
                delayHighPassState[static_cast<size_t>(channel)] = delayHpCoeff * delayHighPassState[static_cast<size_t>(channel)]
                                                                 + (1.0f - delayHpCoeff) * delayed;
                const float highPassedDelayed = delayed - delayHighPassState[static_cast<size_t>(channel)];
                const float stereoDelayed = juce::jmap(width, delayed, oppositeDelayed);
                const float filteredDelayed = (1.0f - delayToneAlpha) * highPassedDelayed
                                            + delayToneAlpha * delayToneState[static_cast<size_t>(channel)];
                delayToneState[static_cast<size_t>(channel)] = filteredDelayed;

                delayDuckEnvelope[static_cast<size_t>(channel)] = juce::jmax(std::abs(dry),
                                                                              delayDuckEnvelope[static_cast<size_t>(channel)] * 0.992f);
                const float duck = juce::jmap(juce::jlimit(0.0f, 1.0f,
                                                           delayDuckEnvelope[static_cast<size_t>(channel)] * 1.8f),
                                              1.0f,
                                              0.46f);

                const float shapedStereoDelayed = juce::jmap(width, filteredDelayed, stereoDelayed * 0.86f);
                const float delayWet = std::tanh(shapedStereoDelayed * 1.08f) * 0.96f;
                const float duckedDelayWet = delayWet * duck;

                delayData[delayWritePosition] = juce::jlimit(-2.2f,
                                                             2.2f,
                                                             dry + duckedDelayWet * feedback);
                data[sample] = juce::jmap(mix, dry, duckedDelayWet);
            }

            delayWritePosition = (delayWritePosition + 1) % delayBufferSize;
        }
    }

    if (reverbOn)
    {
        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, reverbSize);
        reverbParams.damping = juce::jlimit(0.0f, 1.0f, reverbDamp);
        reverbParams.width = 0.88f;
        reverbParams.freezeMode = 0.0f;
        reverbParams.wetLevel = 1.0f;
        reverbParams.dryLevel = 0.0f;
        reverb.setParameters(reverbParams);

        reverbBuffer.makeCopyOf(buffer, true);

        if (reverbPreDelayBuffer.getNumSamples() > 2)
        {
            const int preDelayBufferSize = reverbPreDelayBuffer.getNumSamples();
            const int preDelaySamples = juce::jlimit(0,
                                                     preDelayBufferSize - 1,
                                                     juce::roundToInt(juce::jlimit(0.0f, 80.0f, reverbPreDelayMs)
                                                                      * 0.001f
                                                                      * static_cast<float>(currentSampleRate)));

            if (preDelaySamples <= 0)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    for (int channel = 0; channel < totalNumInputChannels; ++channel)
                    {
                        auto* preDelayData = reverbPreDelayBuffer.getWritePointer(channel);
                        auto* wetData = reverbBuffer.getWritePointer(channel);
                        preDelayData[reverbPreDelayWritePosition] = wetData[sample];
                    }

                    reverbPreDelayWritePosition = (reverbPreDelayWritePosition + 1) % preDelayBufferSize;
                }
            }
            else
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    const int readPos = (reverbPreDelayWritePosition + preDelayBufferSize - preDelaySamples) % preDelayBufferSize;
                    for (int channel = 0; channel < totalNumInputChannels; ++channel)
                    {
                        auto* preDelayData = reverbPreDelayBuffer.getWritePointer(channel);
                        auto* wetData = reverbBuffer.getWritePointer(channel);
                        const float input = wetData[sample];
                        wetData[sample] = preDelayData[readPos];
                        preDelayData[reverbPreDelayWritePosition] = input;
                    }

                    reverbPreDelayWritePosition = (reverbPreDelayWritePosition + 1) % preDelayBufferSize;
                }
            }
        }

        if (totalNumInputChannels >= 2)
            reverb.processStereo(reverbBuffer.getWritePointer(0), reverbBuffer.getWritePointer(1), buffer.getNumSamples());
        else if (totalNumInputChannels == 1)
            reverb.processMono(reverbBuffer.getWritePointer(0), buffer.getNumSamples());

        const float mix = juce::jlimit(0.0f, 1.0f, reverbMix);
        const float reverbHpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 180.0f
                                             / static_cast<float>(currentSampleRate));
        const float reverbLpCutoff = juce::jmap(juce::jlimit(0.0f, 1.0f, reverbDamp), 6200.0f, 2600.0f);
        const float reverbLpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * reverbLpCutoff
                                             / static_cast<float>(currentSampleRate));
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* dryData = buffer.getWritePointer(channel);
            auto* wetData = reverbBuffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float dry = dryData[sample];
                reverbDuckEnvelope[static_cast<size_t>(channel)] = juce::jmax(std::abs(dry),
                                                                               reverbDuckEnvelope[static_cast<size_t>(channel)] * 0.9978f);
                const float duck = juce::jmap(juce::jlimit(0.0f, 1.0f,
                                                           reverbDuckEnvelope[static_cast<size_t>(channel)] * 0.95f),
                                              1.0f,
                                              0.72f);

                reverbHighPassState[static_cast<size_t>(channel)] = reverbHpCoeff * reverbHighPassState[static_cast<size_t>(channel)]
                                                                   + (1.0f - reverbHpCoeff) * wetData[sample];
                const float highPassed = wetData[sample] - reverbHighPassState[static_cast<size_t>(channel)];

                reverbLowPassState[static_cast<size_t>(channel)] = reverbLpCoeff * reverbLowPassState[static_cast<size_t>(channel)]
                                                                  + (1.0f - reverbLpCoeff) * highPassed;
                const float wetShaped = std::tanh((0.88f * reverbLowPassState[static_cast<size_t>(channel)]
                                                   + 0.12f * wetData[sample]) * 1.04f) * 0.95f * duck;
                const float dryGain = 1.0f - 0.54f * mix;
                const float wetGain = mix;

                dryData[sample] = juce::jlimit(-1.25f,
                                               1.25f,
                                               dry * dryGain + wetShaped * wetGain);
            }
        }
    }

    if (lofiOn)
    {
        const float lofiIntensity = juce::jlimit(0.0f, 1.0f, getParameterValue(ParamIDs::lofiIntensity, 0.0f));
        const float lofiTexture = std::pow(lofiIntensity, 0.95f);
        const float lofiHighPassHz = juce::jmap(lofiTexture, 420.0f, 980.0f);
        const float lofiHpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * lofiHighPassHz
                                           / static_cast<float>(currentSampleRate));
        const float lofiLpCutoff = juce::jmap(lofiTexture, 3600.0f, 2100.0f);
        const float lofiLpCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * lofiLpCutoff
                                           / static_cast<float>(currentSampleRate));
        const float lofiMidHz = juce::jmap(lofiTexture, 1250.0f, 2050.0f);
        const float lofiMidCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * lofiMidHz
                                            / static_cast<float>(currentSampleRate));
        const float drive = juce::jmap(lofiTexture, 1.70f, 3.40f);
        const float hornAmount = juce::jmap(lofiTexture, 0.34f, 1.10f);
        const float barkAmount = juce::jmap(lofiTexture, 0.12f, 0.52f);
        const float compressedBlend = juce::jmap(lofiTexture, 0.34f, 0.78f);
        const float wetMix = juce::jmap(lofiTexture, 0.60f, 0.98f);
        const float outputTrim = juce::jmap(lofiTexture, 0.88f, 0.68f);

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* data = buffer.getWritePointer(channel);
            const auto ch = static_cast<size_t>(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float x = data[sample];
                lofiHighPassState[ch] = lofiHpCoeff * lofiHighPassState[ch]
                                      + (1.0f - lofiHpCoeff) * x;
                const float highPassed = x - lofiHighPassState[ch];

                lofiLowPassState[ch] = lofiLpCoeff * lofiLowPassState[ch]
                                     + (1.0f - lofiLpCoeff) * highPassed;
                lofiMidState[ch] = lofiMidCoeff * lofiMidState[ch]
                                 + (1.0f - lofiMidCoeff) * lofiLowPassState[ch];

                const float bandLimited = lofiLowPassState[ch];
                const float hornBand = bandLimited - lofiMidState[ch];
                const float megaphoneVoice = bandLimited * (0.74f + 0.08f * (1.0f - lofiTexture))
                                           + hornBand * hornAmount
                                           + std::tanh(bandLimited * (1.2f + 0.4f * lofiTexture)) * barkAmount;
                const float clipped = std::tanh(megaphoneVoice * drive);
                const float compressed = juce::jlimit(-1.0f,
                                                      1.0f,
                                                      clipped * (0.82f - 0.08f * lofiTexture)
                                                      + megaphoneVoice * (0.24f - 0.03f * lofiTexture));
                const float degraded = juce::jlimit(-1.0f,
                                                    1.0f,
                                                    juce::jmap(compressedBlend,
                                                               megaphoneVoice,
                                                               compressed) * outputTrim);
                data[sample] = juce::jlimit(-1.0f,
                                            1.0f,
                                            juce::jmap(wetMix, x, degraded));
            }
        }
    }

    // Ramp output gain across the block to eliminate master-volume zipper noise.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        buffer.applyGainRamp(channel, 0, buffer.getNumSamples(), prevOutputGainLinear, outputGain);
    prevOutputGainLinear = outputGain;

    if (presetTransitionRampRemainingSamples > 0)
    {
        // totalRampSamples MUST match the value set in resetRuntimeVoicingState()
        // (currently 0.050 * sr).  If it is smaller, (remaining / total) > 1 and
        // rampGain becomes negative — inverting and amplifying the signal by up to
        // 6× for ~42ms.  That was the real "toilet bowl goblin" root cause.
        const int totalRampSamples = juce::jmax(1, juce::roundToInt(currentSampleRate * 0.050));

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const int rampSamplesBefore = presetTransitionRampRemainingSamples;
            if (rampSamplesBefore <= 0)
                break;

            const float rampGain = juce::jlimit(0.0f, 1.0f,
                1.0f - (static_cast<float>(rampSamplesBefore) / static_cast<float>(totalRampSamples)));

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
                buffer.getWritePointer(channel)[sample] *= rampGain;

            --presetTransitionRampRemainingSamples;
        }
    }

    // Output peak limiter — absolute last stage.
    // Ceiling is auto-set to the master volume level so it acts as a true-peak
    // brickwall at whatever level the user has dialled in on the master knob.
    // If master is above 0 dBFS we still cap at 0.99 to prevent DAW-side clipping.
    const float limIntensity = juce::jlimit(0.0f, 1.0f, getParameterValue(ParamIDs::limiter, 0.0f));
    if (limIntensity > 0.005f)
    {
        // Threshold sweeps from -6 dB (knob at min) to -30 dB (knob at max).
        // This makes even small knob movements produce noticeable gain reduction.
        const float threshDb   = juce::jmap(limIntensity, 0.0f, 1.0f, -6.0f, -30.0f);
        const float ceilingLin = juce::Decibels::decibelsToGain(threshDb);
        const float limAttCoeff = std::expf(-1.0f / (0.0001f * static_cast<float>(currentSampleRate)));  // ~0.1 ms attack
        const float limRelCoeff = std::expf(-1.0f / (0.100f  * static_cast<float>(currentSampleRate)));  // ~100 ms release

        float minGainApplied = 1.0f;
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float absSample = std::abs(data[sample]);
                if (absSample > limiterEnvelope[channel])
                    limiterEnvelope[channel] = limAttCoeff * limiterEnvelope[channel] + (1.0f - limAttCoeff) * absSample;
                else
                    limiterEnvelope[channel] = limRelCoeff * limiterEnvelope[channel] + (1.0f - limRelCoeff) * absSample;

                if (limiterEnvelope[channel] > ceilingLin)
                {
                    const float gainApplied = ceilingLin / juce::jmax(limiterEnvelope[channel], 1e-6f);
                    data[sample] *= gainApplied;
                    minGainApplied = juce::jmin(minGainApplied, gainApplied);
                }
            }
        }

        const float grDb = (minGainApplied < 0.999f)
                           ? juce::Decibels::gainToDecibels(minGainApplied)
                           : 0.0f;
        limiterGainReductionDb.store(grDb, std::memory_order_relaxed);
    }
    else
    {
        limiterEnvelope.fill(0.0f);
        limiterGainReductionDb.store(0.0f, std::memory_order_relaxed);
    }

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
}

void HexstackAudioProcessor::applyAutomatableControlState(bool forceReload)
{
    juce::ignoreUnused(forceReload);

    lastPrimaryMicParamIndex = 0;

    auto getChoiceIndex = [this](const char* parameterId, int fallback, int maxChoiceIndex)
    {
        if (const auto* value = parameters.getRawParameterValue(parameterId))
            return juce::jlimit(0, maxChoiceIndex, juce::roundToInt(value->load()));

        return juce::jlimit(0, maxChoiceIndex, fallback);
    };

    const int tunerReferenceIndex = getChoiceIndex(ParamIDs::tunerReference,
                                                   1,
                                                   2);
    if (forceReload || tunerReferenceIndex != lastTunerReferenceIndex)
    {
        constexpr std::array<float, 3> tunerReferenceChoices { 432.0f, 440.0f, 442.0f };
        setTunerReferenceHz(tunerReferenceChoices[static_cast<size_t>(tunerReferenceIndex)]);
        lastTunerReferenceIndex = tunerReferenceIndex;
    }

    const int tunerRangeIndex = getChoiceIndex(ParamIDs::tunerRange,
                                               0,
                                               2);
    if (forceReload || tunerRangeIndex != lastTunerRangeParamIndex)
    {
        switch (tunerRangeIndex)
        {
            case 1:
                setTunerRangeMode(TunerRangeMode::bass);
                break;
            case 2:
                setTunerRangeMode(TunerRangeMode::guitar);
                break;
            case 0:
            default:
                setTunerRangeMode(TunerRangeMode::wide);
                break;
        }

        lastTunerRangeParamIndex = tunerRangeIndex;
    }
}

void HexstackAudioProcessor::processPitchEffect(juce::AudioBuffer<float>& buffer,
                                                float pitchShift,
                                                float pitchMix,
                                                float pitchWidth)
{
    if (pitchBuffer.getNumChannels() <= 0 || pitchBuffer.getNumSamples() < 512)
        return;

    const float mix = juce::jlimit(0.0f, 1.0f, pitchMix);
    const float width = juce::jlimit(0.0f, 1.0f, pitchWidth);
    const bool usePitchShift = std::abs(pitchShift) >= 0.01f;
    const bool useChorus = width >= 0.001f;
    if (mix <= 0.0005f || (! usePitchShift && ! useChorus))
        return;

    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0)
        return;

    const int bufferSize = pitchBuffer.getNumSamples();
    const float baseRatio = std::pow(2.0f, pitchShift / 12.0f);
    const int windowSize = juce::jlimit(384,
                                        bufferSize - 4,
                                        juce::roundToInt(static_cast<float>(currentSampleRate) * 0.035f));
    const float halfWindow = 0.5f * static_cast<float>(windowSize);
    const float advance = usePitchShift ? juce::jlimit(-0.95f, 3.5f, baseRatio - 1.0f)
                                        : 0.0f;
    const float widthCurve = std::pow(width, 0.92f);
    // chorusBlend: how much the modulated delays replace the dry signal in each voice.
    // Raised from 0.08+0.40 (max 0.48) to 0.12+0.63 (max 0.75) for a fuller, more audible chorus.
    const float chorusBlend = juce::jlimit(0.0f, 1.0f, 0.12f + widthCurve * 0.63f);
    const float chorusWetness = juce::jlimit(0.0f, 1.0f, std::pow(widthCurve, 1.35f));
    // Primary LFO rate; secondary LFO runs at golden-ratio multiple for non-periodic beating.
    const float chorusRateHz  = juce::jmap(widthCurve, 0.18f, 0.72f);
    const float chorusRate2Hz = chorusRateHz * (1.0f + widthCurve * 0.618f);
    // Depth raised from max 5.2ms to max 10ms for a lush, audible sweep.
    const float chorusDepthSamples = static_cast<float>(currentSampleRate) * juce::jmap(widthCurve, 0.0012f, 0.0100f);
    const float chorusBaseDelaySamples = static_cast<float>(currentSampleRate) * juce::jmap(widthCurve, 0.011f, 0.020f);
    const float chorusPhaseAdvance  = static_cast<float>(2.0 * juce::MathConstants<double>::pi * chorusRateHz  / currentSampleRate);
    const float chorusPhaseAdvance2 = static_cast<float>(2.0 * juce::MathConstants<double>::pi * chorusRate2Hz / currentSampleRate);

    auto wrapWindowPosition = [windowSize](float value)
    {
        const float window = static_cast<float>(windowSize);
        while (value < 0.0f)
            value += window;
        while (value >= window)
            value -= window;
        return value;
    };

    auto readInterpolatedSample = [this, bufferSize](float delaySamples)
    {
        float readPosition = static_cast<float>(pitchWritePosition) - delaySamples;
        while (readPosition < 0.0f)
            readPosition += static_cast<float>(bufferSize);
        while (readPosition >= static_cast<float>(bufferSize))
            readPosition -= static_cast<float>(bufferSize);

        const int index0 = static_cast<int>(std::floor(readPosition));
        const int index1 = (index0 + 1) % bufferSize;
        const float frac = readPosition - static_cast<float>(index0);
        const float s0 = pitchBuffer.getSample(0, index0);
        const float s1 = pitchBuffer.getSample(0, index1);
        return s0 + (s1 - s0) * frac;
    };

    auto renderGranularVoice = [&](float grainA, float grainB, float envA, float envB, float modulationSamples)
    {
        const float sampleA = readInterpolatedSample(static_cast<float>(windowSize) - grainA + modulationSamples);
        const float sampleB = readInterpolatedSample(static_cast<float>(windowSize) - grainB - modulationSamples * 0.35f);
        const float envelopeSum = juce::jmax(1.0e-4f, envA + envB);
        return (sampleA * envA + sampleB * envB) / envelopeSum;
    };

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float monoDry = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            monoDry += buffer.getReadPointer(channel)[sample];

        monoDry /= static_cast<float>(numChannels);
        pitchBuffer.setSample(0, pitchWritePosition, monoDry);

        const float grainA = wrapWindowPosition(pitchReadOffset);
        const float grainB = wrapWindowPosition(grainA + halfWindow);

        const float envA = 1.0f - std::abs((grainA / halfWindow) - 1.0f);
        const float envB = 1.0f - std::abs((grainB / halfWindow) - 1.0f);

        const float centeredPitched = renderGranularVoice(grainA, grainB, envA, envB, 0.0f);
        // Dual-LFO chorus: primary LFO (0°/90°) + secondary LFO at golden-ratio rate (0°/90°).
        // Blending both (70/30) creates non-periodic beating that sounds like two detune voices.
        const float lfo1L = std::sin(pitchChorusPhase);
        const float lfo1R = std::sin(pitchChorusPhase + juce::MathConstants<float>::halfPi);
        const float lfo2L = std::sin(pitchChorusPhase2);
        const float lfo2R = std::sin(pitchChorusPhase2 + juce::MathConstants<float>::halfPi);
        const float chorusLfoLeft  = lfo1L * 0.70f + lfo2L * 0.30f;
        const float chorusLfoRight = lfo1R * 0.70f + lfo2R * 0.30f;
        const float chorusDelayLeft  = chorusBaseDelaySamples + chorusDepthSamples * chorusLfoLeft;
        const float chorusDelayRight = chorusBaseDelaySamples + chorusDepthSamples * chorusLfoRight;
        const float chorusVoiceLeft  = readInterpolatedSample(chorusDelayLeft);
        const float chorusVoiceRight = readInterpolatedSample(chorusDelayRight);
        const float chorusStereoLeft  = monoDry * (1.0f - chorusBlend) + chorusVoiceLeft  * chorusBlend;
        const float chorusStereoRight = monoDry * (1.0f - chorusBlend) + chorusVoiceRight * chorusBlend;

        // When pitch shift is active, blend chorus more aggressively into the wet path so
        // the whammy has stereo width and life instead of sounding like a single dry voice.
        // Old multiplier was 0.28 (max 13% chorus); new 0.52 gives max ~39% chorus contribution.
        const float chorMixFraction = usePitchShift ? 0.52f * chorusBlend : 1.0f;
        const float wetLeft = usePitchShift
            ? juce::jmap(chorusWetness,
                         centeredPitched,
                         centeredPitched * (1.0f - chorMixFraction) + chorusStereoLeft  * chorMixFraction)
            : chorusStereoLeft;
        const float wetRight = usePitchShift
            ? juce::jmap(chorusWetness,
                         centeredPitched,
                         centeredPitched * (1.0f - chorMixFraction) + chorusStereoRight * chorMixFraction)
            : chorusStereoRight;

        if (numChannels == 1)
        {
            const float monoWet = juce::jmap(mix,
                                             monoDry,
                                             0.5f * (wetLeft + wetRight));
            buffer.getWritePointer(0)[sample] = monoWet;
        }
        else
        {
            buffer.getWritePointer(0)[sample] = juce::jmap(mix, monoDry, wetLeft);
            buffer.getWritePointer(1)[sample] = juce::jmap(mix, monoDry, wetRight);

            for (int channel = 2; channel < numChannels; ++channel)
                buffer.getWritePointer(channel)[sample] = juce::jmap(mix, monoDry, 0.5f * (wetLeft + wetRight));
        }

        pitchReadOffset = wrapWindowPosition(grainA + advance);
        pitchChorusPhase += chorusPhaseAdvance;
        if (pitchChorusPhase >= juce::MathConstants<float>::twoPi)
            pitchChorusPhase -= juce::MathConstants<float>::twoPi;
        pitchChorusPhase2 += chorusPhaseAdvance2;
        if (pitchChorusPhase2 >= juce::MathConstants<float>::twoPi)
            pitchChorusPhase2 -= juce::MathConstants<float>::twoPi;

        pitchWritePosition = (pitchWritePosition + 1) % bufferSize;
    }
}

bool HexstackAudioProcessor::isFxPedalEnabled(size_t index) const
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

    if (const auto* value = parameters.getRawParameterValue(ids[index]))
        return value->load() >= 0.5f;

    return true;
}

float HexstackAudioProcessor::getParameterValue(const char* paramId, float fallback) const
{
    if (const auto* value = parameters.getRawParameterValue(paramId))
        return value->load();

    return fallback;
}

float HexstackAudioProcessor::getHostSyncedDelayTimeMs(float manualDelayTimeMs)
{
    const float clampedManualMs = juce::jlimit(60.0f, 900.0f, manualDelayTimeMs);

    auto* transportPlayHead = getPlayHead();
    if (transportPlayHead == nullptr)
        return clampedManualMs;

    const auto position = transportPlayHead->getPosition();
    if (! position.hasValue())
        return clampedManualMs;

    const auto bpmValue = position->getBpm();
    if (! bpmValue.hasValue() || *bpmValue <= 1.0)
        return clampedManualMs;

    const auto timeSignature = position->getTimeSignature();
    const float bpm = static_cast<float>(*bpmValue);
    const int numerator = timeSignature.hasValue() ? juce::jmax(1, timeSignature->numerator) : 4;
    const int denominator = timeSignature.hasValue() ? juce::jmax(1, timeSignature->denominator) : 4;
    const float quarterNoteMs = 60000.0f / bpm;
    const float hostBeatInQuarterNotes = 4.0f / static_cast<float>(denominator);
    const float barLengthInQuarterNotes = static_cast<float>(numerator) * hostBeatInQuarterNotes;
    const float normalizedTime = juce::jlimit(0.0f,
                                              1.0f,
                                              (clampedManualMs - 60.0f) / (900.0f - 60.0f));

    const std::array<float, 8> syncDivisionsInQuarterNotes {
        hostBeatInQuarterNotes * 0.25f,
        hostBeatInQuarterNotes / 3.0f,
        hostBeatInQuarterNotes * 0.5f,
        hostBeatInQuarterNotes * 0.75f,
        hostBeatInQuarterNotes,
        hostBeatInQuarterNotes * 1.5f,
        hostBeatInQuarterNotes * 2.0f,
        barLengthInQuarterNotes
    };

    const int divisionIndex = juce::jlimit(0,
                                           static_cast<int>(syncDivisionsInQuarterNotes.size()) - 1,
                                           juce::roundToInt(normalizedTime * static_cast<float>(syncDivisionsInQuarterNotes.size() - 1)));

    return juce::jlimit(20.0f,
                        2000.0f,
                        quarterNoteMs * syncDivisionsInQuarterNotes[static_cast<size_t>(divisionIndex)]);
}

HexstackAudioProcessor::TunerData HexstackAudioProcessor::getTunerData() const
{
    TunerData data;
    data.frequencyHz = tunerFrequencyHz.load();
    data.centsOffset = tunerCentsOffset.load();
    data.levelDb = tunerLevelDb.load();
    data.midiNote = tunerMidiNote.load();
    data.hasSignal = tunerHasSignal.load();
    return data;
}

void HexstackAudioProcessor::setTunerAnalysisEnabled(bool enabled)
{
    const bool wasEnabled = tunerAnalysisEnabled.exchange(enabled);

    if (enabled || ! wasEnabled)
        return;

    tunerCaptureWritePos = 0;
    tunerCaptureValidSamples = 0;
    tunerSamplesSinceLastAnalysis = 0;
    tunerSmoothedFrequencyHz = 0.0f;
    tunerLastMidiNote = -1;
    tunerStableFrames = 0;
    tunerSilenceFrames = 0;
    tunerCommittedMidiNote = -1;
    std::fill(std::begin(tunerRawFreqHistory), std::end(tunerRawFreqHistory), 0.0f);
    tunerRawFreqHistoryPos   = 0;
    tunerRawFreqHistoryCount = 0;

    tunerHasSignal.store(false);
    tunerFrequencyHz.store(0.0f);
    tunerCentsOffset.store(0.0f);
    tunerMidiNote.store(-1);
    tunerLevelDb.store(-100.0f);
}

bool HexstackAudioProcessor::isTunerAnalysisEnabled() const
{
    return tunerAnalysisEnabled.load(std::memory_order_relaxed);
}

void HexstackAudioProcessor::setTunerOutputMuted(bool shouldMute)
{
    tunerOutputMuted.store(shouldMute, std::memory_order_relaxed);
}

bool HexstackAudioProcessor::isTunerOutputMuted() const
{
    return tunerOutputMuted.load(std::memory_order_relaxed);
}

void HexstackAudioProcessor::setTunerReferenceHz(float hz)
{
    tunerReferenceHz.store(juce::jlimit(400.0f, 500.0f, hz));
}

float HexstackAudioProcessor::getTunerReferenceHz() const
{
    return tunerReferenceHz.load();
}

void HexstackAudioProcessor::setTunerRangeMode(TunerRangeMode mode)
{
    tunerRangeMode.store(static_cast<int>(mode));
}

HexstackAudioProcessor::TunerRangeMode HexstackAudioProcessor::getTunerRangeMode() const
{
    const int value = tunerRangeMode.load();
    if (value == static_cast<int>(TunerRangeMode::bass))
        return TunerRangeMode::bass;
    if (value == static_cast<int>(TunerRangeMode::guitar))
        return TunerRangeMode::guitar;
    return TunerRangeMode::wide;
}

void HexstackAudioProcessor::analyzeTunerInput(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0 || currentSampleRate <= 0.0
        || tunerCaptureBuffer.empty() || tunerWindowBuffer.empty())
    {
        tunerHasSignal.store(false);
        tunerFrequencyHz.store(0.0f);
        tunerCentsOffset.store(0.0f);
        tunerMidiNote.store(-1);
        tunerLevelDb.store(-100.0f);
        return;
    }

    const int numChannels = juce::jmax(1, juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels()));
    const int numSamples = buffer.getNumSamples();

    // Apply the Input knob gain so the tuner sees the same signal level as the amp.
    // In standalone, the raw interface level is often 10-20 dB quieter than in a DAW
    // (where the DAW bus pre-conditions the signal).  Without this, low-output guitars
    // fall below the YIN noise floor even if the amp sounds fine.
    const float inputGainLinear = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(ParamIDs::input)->load());

    double sumSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            mono += buffer.getReadPointer(ch)[i];

        mono = (mono / static_cast<float>(numChannels)) * inputGainLinear;

        tunerCaptureBuffer[static_cast<size_t>(tunerCaptureWritePos)] = mono;
        tunerCaptureWritePos = (tunerCaptureWritePos + 1) % static_cast<int>(tunerCaptureBuffer.size());
        tunerCaptureValidSamples = juce::jmin(tunerCaptureValidSamples + 1, static_cast<int>(tunerCaptureBuffer.size()));

        const double s = static_cast<double>(mono);
        sumSquares += s * s;
    }

    const float rms = std::sqrt(static_cast<float>(sumSquares / juce::jmax(1, numSamples)));
    const float levelDb = juce::Decibels::gainToDecibels(rms, -100.0f);
    tunerLevelDb.store(levelDb);

    tunerSamplesSinceLastAnalysis += numSamples;

    // Only bail out on genuine silence (no signal at all for several frames).
    // We rely on the peak-normalisation step inside the window analysis to
    // guard against very low-level noise producing a bogus pitch reading.
    if (levelDb < -96.0f)
    {
        ++tunerSilenceFrames;

        if (tunerSilenceFrames > 6)
        {
            tunerHasSignal.store(false);
            tunerFrequencyHz.store(0.0f);
            tunerCentsOffset.store(0.0f);
            tunerMidiNote.store(-1);
            tunerSmoothedFrequencyHz = 0.0f;
            tunerLastMidiNote = -1;
            tunerStableFrames = 0;
            tunerCommittedMidiNote = -1;
            tunerRawFreqHistoryCount = 0;
        }

        return;
    }

    tunerSilenceFrames = 0;

    if (tunerCaptureValidSamples < static_cast<int>(tunerWindowBuffer.size()))
        return;

    constexpr int tunerAnalysisHopSize = 2048;  // ~46ms at 44.1kHz — responsive
    if (tunerSamplesSinceLastAnalysis < tunerAnalysisHopSize)
        return;

    tunerSamplesSinceLastAnalysis = 0;

    const int captureSize = static_cast<int>(tunerCaptureBuffer.size());
    const int windowSize = static_cast<int>(tunerWindowBuffer.size());
    int readIndex = tunerCaptureWritePos - windowSize;
    if (readIndex < 0)
        readIndex += captureSize;

    for (int i = 0; i < windowSize; ++i)
    {
        tunerWindowBuffer[static_cast<size_t>(i)] = tunerCaptureBuffer[static_cast<size_t>(readIndex)];
        readIndex = (readIndex + 1) % captureSize;
    }

    // DC removal only — YIN does not benefit from centre-clipping and the
    // clipper's artificial harmonics actively hurt autocorrelation accuracy.
    float mean = 0.0f;
    for (float s : tunerWindowBuffer)
        mean += s;
    mean /= static_cast<float>(windowSize);

    for (int i = 0; i < windowSize; ++i)
        tunerWindowBuffer[static_cast<size_t>(i)] -= mean;

    // Normalize amplitude so YIN works reliably on low-output guitars
    // (passive pickups, direct into interface at low gain — common in standalone).
    float peak = 0.0f;
    for (float s : tunerWindowBuffer)
        peak = juce::jmax(peak, std::abs(s));
    if (peak < 1e-6f)
        return;   // truly silent — bail out
    const float normGain = 1.0f / peak;
    for (int i = 0; i < windowSize; ++i)
        tunerWindowBuffer[static_cast<size_t>(i)] *= normGain;

    const float frequency = estimateFrequencyYIN(tunerWindowBuffer.data(), windowSize);
    if (frequency <= 0.0f)
    {
        if (++tunerSilenceFrames > 3)
        {
            tunerHasSignal.store(false);
            tunerFrequencyHz.store(0.0f);
            tunerCentsOffset.store(0.0f);
            tunerMidiNote.store(-1);
        }
        return;
    }

    // ── 3-slot median on raw YIN output ────────────────────────────────────────
    tunerRawFreqHistory[tunerRawFreqHistoryPos] = frequency;
    tunerRawFreqHistoryPos = (tunerRawFreqHistoryPos + 1) % 3;
    if (tunerRawFreqHistoryCount < 3) ++tunerRawFreqHistoryCount;

    float sorted[3];
    std::copy(tunerRawFreqHistory, tunerRawFreqHistory + tunerRawFreqHistoryCount, sorted);
    std::sort(sorted, sorted + tunerRawFreqHistoryCount);
    // Use lower median: for count=2 this gives sorted[0] (lower value) which
    // is more likely the fundamental than the harmonic.
    const float medianFreq = sorted[(tunerRawFreqHistoryCount - 1) / 2];

    // ── Note from median, with stale-value flush on note change ───────────────
    const float referenceHz = getTunerReferenceHz();
    const int rawMidiNote = juce::roundToInt(69.0f + 12.0f * std::log2(medianFreq / referenceHz));

    // When the note changes, flush stale median values from the previous note
    // so they can no longer drag the median back, and snap the display frequency.
    if (tunerLastMidiNote >= 0 && std::abs(rawMidiNote - tunerLastMidiNote) > 1)
    {
        std::fill(std::begin(tunerRawFreqHistory), std::end(tunerRawFreqHistory), frequency);
        tunerRawFreqHistoryPos = 0;
        tunerRawFreqHistoryCount = 3;
        tunerSmoothedFrequencyHz = frequency;
    }

    if (rawMidiNote == tunerLastMidiNote)
        ++tunerStableFrames;
    else
    {
        tunerLastMidiNote = rawMidiNote;
        tunerStableFrames = 1;
    }

    // Commit note after first stable frame — YIN is accurate, no need for 2+
    if (tunerStableFrames >= 1)
        tunerCommittedMidiNote = rawMidiNote;
    else if (tunerCommittedMidiNote < 0)
        tunerCommittedMidiNote = juce::jmax(rawMidiNote, 0);

    const int midiNote = tunerCommittedMidiNote;

    // Cents computed directly from medianFreq (the same value used for midiNote).
    // Using tunerSmoothedFrequencyHz here caused the needle to swing ±50c after
    // octave-error frames because the smoothed value lagged behind the committed note.
    // The UI needle already does its own 0.34-coefficient smoothing for display.
    const float semitone = 69.0f + 12.0f * std::log2(medianFreq / referenceHz);
    const float cents = (semitone - static_cast<float>(midiNote)) * 100.0f;

    tunerHasSignal.store(true);
    tunerFrequencyHz.store(medianFreq);
    tunerCentsOffset.store(cents);
    tunerMidiNote.store(midiNote);
}

// ── YIN pitch estimator (de Cheveigné & Kawahara 2002) ──────────────────────
// YIN uses a Cumulative Mean Normalized Difference Function (CMNDF) which makes
// octave errors structurally impossible: the CMNDF minimum at lag=T (the true
// period) is always deeper than the minimum at lag=T/2 (octave up).
// This replaces the NSDF+MPM approach which failed when harmonics had higher
// correlation than the fundamental (common on guitar with heavy picking).
float HexstackAudioProcessor::estimateFrequencyYIN(const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples < 256 || currentSampleRate <= 0.0)
        return 0.0f;

    float minFrequency, maxFrequency;
    switch (getTunerRangeMode())
    {
        case TunerRangeMode::bass:
            minFrequency = 25.0f;
            maxFrequency = 420.0f;
            break;
        case TunerRangeMode::guitar:
            minFrequency = 30.0f;
            maxFrequency = 1400.0f;
            break;
        case TunerRangeMode::wide:
        default:
            minFrequency = 22.0f;
            maxFrequency = 2000.0f;
            break;
    }

    const int W      = numSamples / 2;
    const int minTau = juce::jlimit(2, W - 1, static_cast<int>(currentSampleRate / maxFrequency));
    const int maxTau = juce::jlimit(minTau + 1, W - 1, static_cast<int>(currentSampleRate / minFrequency));

    // Use pre-allocated work buffers (avoid heap alloc in audio thread).
    // They are sized to W+1 in prepareToPlay.
    if (static_cast<int>(yinDiffBuf.size()) < maxTau + 1
        || static_cast<int>(yinNormBuf.size()) < maxTau + 1)
        return 0.0f;  // not yet prepared

    // ── Step 2: difference function ─────────────────────────────────────────
    yinDiffBuf[0] = 0.0f;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        float sum = 0.0f;
        for (int n = 0; n < W; ++n)
        {
            const float diff = samples[n] - samples[n + tau];
            sum += diff * diff;
        }
        yinDiffBuf[static_cast<size_t>(tau)] = sum;
    }

    // ── Step 3: cumulative mean normalized difference (CMNDF) ────────────────
    yinNormBuf[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        runningSum += yinDiffBuf[static_cast<size_t>(tau)];
        yinNormBuf[static_cast<size_t>(tau)] = (runningSum > 0.0f)
            ? yinDiffBuf[static_cast<size_t>(tau)] * static_cast<float>(tau) / runningSum
            : 1.0f;
    }

    // ── Step 4: absolute threshold ───────────────────────────────────────────
    // 0.20 is more lenient than the original 0.12 and handles real guitar signals
    // whose CMNDF dip often sits in the 0.12-0.20 range due to inharmonicity.
    constexpr float yinThreshold = 0.20f;
    int bestTau = -1;

    for (int tau = minTau; tau <= maxTau - 1; ++tau)
    {
        if (yinNormBuf[static_cast<size_t>(tau)] < yinThreshold)
        {
            // Slide to local minimum of this dip
            while (tau + 1 <= maxTau
                   && yinNormBuf[static_cast<size_t>(tau + 1)] < yinNormBuf[static_cast<size_t>(tau)])
                ++tau;
            bestTau = tau;
            break;
        }
    }

    // Fallback: use global minimum — no hard gate so marginal signals still display.
    if (bestTau < 0)
    {
        int gMin = minTau;
        for (int tau = minTau + 1; tau <= maxTau; ++tau)
            if (yinNormBuf[static_cast<size_t>(tau)] < yinNormBuf[static_cast<size_t>(gMin)])
                gMin = tau;

        // Only reject if the signal is truly unpitched (> 0.45 = very high aperiodicity).
        if (yinNormBuf[static_cast<size_t>(gMin)] > 0.45f)
            return 0.0f;

        bestTau = gMin;
    }

    if (bestTau <= 0)
        return 0.0f;

    // ── Step 5: parabolic interpolation (Aubio-corrected formula) ────────────
    float refinedTau = static_cast<float>(bestTau);
    if (bestTau > minTau && bestTau < maxTau)
    {
        const float s0 = yinNormBuf[static_cast<size_t>(bestTau - 1)];
        const float s1 = yinNormBuf[static_cast<size_t>(bestTau)];
        const float s2 = yinNormBuf[static_cast<size_t>(bestTau + 1)];
        const float denom = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs(denom) > 1.0e-8f)
            refinedTau += juce::jlimit(-0.5f, 0.5f, (s2 - s0) / denom);
    }

    if (refinedTau <= 0.0f)
        return 0.0f;

    return static_cast<float>(currentSampleRate / static_cast<double>(refinedTau));
}

bool HexstackAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HexstackAudioProcessor::createEditor()
{
    return new HexstackAudioProcessorEditor(*this);
}

void HexstackAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState(); state.isValid())
    {
        auto mutableState = state;
        mutableState.setProperty(StateKeys::irSource, currentIRFile.existsAsFile() ? "file" : "builtin", nullptr);
        mutableState.setProperty(StateKeys::irPath, currentIRFile.getFullPathName(), nullptr);
        mutableState.setProperty(StateKeys::programIndex, currentProgramIndex, nullptr);
        mutableState.setProperty("hexFilePath", activeHexFilePath, nullptr);
        std::unique_ptr<juce::XmlElement> xml(mutableState.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void HexstackAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
    {
        const auto tree = juce::ValueTree::fromXml(*xmlState);
        if (tree.isValid())
            applyLoadedStateTree(tree);
    }
}

bool HexstackAudioProcessor::saveHexPresetToFile(const juce::File& file, const juce::String& presetName)
{
    if (file == juce::File())
        return false;

    auto state = parameters.copyState();
    if (! state.isValid())
        return false;

    auto mutableState = state;

    // Determine IR source: if a user cab is loaded, embed the raw WAV bytes as base64
    // so the .hex file is fully self-contained.  Recipients can load the preset without
    // having to obtain the same IR file separately.
    juce::MemoryBlock embeddedCabRaw;
    const bool hasUserCab = currentIRFile.existsAsFile();
    if (hasUserCab && currentIRFile.loadFileAsData(embeddedCabRaw))
    {
        mutableState.setProperty(StateKeys::irSource, "embedded", nullptr);
        mutableState.setProperty(StateKeys::irPath, currentIRFile.getFileName(), nullptr);  // display name only
    }
    else
    {
        mutableState.setProperty(StateKeys::irSource, hasUserCab ? "file" : "builtin", nullptr);
        mutableState.setProperty(StateKeys::irPath, currentIRFile.getFullPathName(), nullptr);
    }

    mutableState.setProperty(StateKeys::programIndex, currentProgramIndex, nullptr);
    auto stateXml = std::unique_ptr<juce::XmlElement>(mutableState.createXml());
    if (stateXml == nullptr)
        return false;

    juce::XmlElement root(HexPresetKeys::rootTag);
    root.setAttribute(HexPresetKeys::schemaVersion, 1);
    root.setAttribute(HexPresetKeys::presetName, presetName);

    auto pluginState = std::make_unique<juce::XmlElement>(HexPresetKeys::pluginStateTag);
    pluginState->addChildElement(stateXml.release());
    root.addChildElement(pluginState.release());

    // Append the embedded cab data (base64) as a sibling of PluginState.
    if (embeddedCabRaw.getSize() > 0)
    {
        juce::MemoryOutputStream base64Stream;
        juce::Base64::convertToBase64(base64Stream, embeddedCabRaw.getData(), embeddedCabRaw.getSize());

        auto* cabElement = new juce::XmlElement("CabIR");
        cabElement->setAttribute("filename", currentIRFile.getFileName());
        cabElement->setAttribute("encoding", "base64");
        cabElement->addTextElement(base64Stream.toString());
        root.addChildElement(cabElement);
    }

    if (! file.getParentDirectory().createDirectory())
        return false;

    return root.writeTo(file);
}

bool HexstackAudioProcessor::loadHexPresetFromFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return false;

    if (! xml->hasTagName(HexPresetKeys::rootTag))
        return false;

    const auto* pluginState = xml->getChildByName(HexPresetKeys::pluginStateTag);
    if (pluginState == nullptr || pluginState->getNumChildElements() <= 0)
        return false;

    const auto* stateXml = pluginState->getChildElement(0);
    if (stateXml == nullptr)
        return false;

    auto stateTree = juce::ValueTree::fromXml(*stateXml);

    // ── Embedded cab IR ──────────────────────────────────────────────────────────
    // Presets saved with a user cab embed the raw WAV bytes as base64 in a <CabIR>
    // element.  Extract to HEXSTACK/UserCabs/ so currentIRFile points to a real
    // file; re-saving the preset will then re-embed the same data.
    const auto irSource = stateTree.getProperty(StateKeys::irSource).toString();
    juce::MemoryBlock embeddedCabData;
    juce::File embeddedCabFile;

    if (irSource == "embedded")
    {
        const auto* cabElement = xml->getChildByName("CabIR");
        if (cabElement != nullptr)
        {
            juce::MemoryOutputStream mos(embeddedCabData, false);
            juce::Base64::convertFromBase64(mos, cabElement->getAllSubText().trim());

            if (embeddedCabData.getSize() > 0)
            {
                const juce::String cabFilename = cabElement->getStringAttribute("filename", "embedded_cab.wav");
                const juce::File userCabsDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                                   .getChildFile("HEXSTACK/UserCabs");
                userCabsDir.createDirectory();
                embeddedCabFile = userCabsDir.getChildFile(cabFilename);
                embeddedCabFile.replaceWithData(embeddedCabData.getData(), embeddedCabData.getSize());
            }
        }
        // Tell applyLoadedStateTree the source is builtin so it won't try to reload
        // a user file path.  We'll load the embedded IR ourselves right after.
        stateTree.setProperty(StateKeys::irSource, "builtin", nullptr);
    }
    // ── External cab path resolution ─────────────────────────────────────────────
    else if (irSource == "file")
    {
        const juce::File savedIR(stateTree.getProperty(StateKeys::irPath).toString());
        if (! savedIR.existsAsFile())
        {
            // Try relative: cab with the same filename in the same folder as the .hex
            const juce::File relativeIR = file.getParentDirectory()
                                              .getChildFile(savedIR.getFileName());
            if (relativeIR.existsAsFile())
            {
                stateTree.setProperty(StateKeys::irPath, relativeIR.getFullPathName(), nullptr);
            }
            else
            {
                juce::MessageManager::callAsync([cabName = savedIR.getFileName()]
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Missing Cabinet IR",
                        "The preset requires the cab file:\n\n  " + cabName
                            + "\n\nPlace the cab file in the same folder as the .hex preset and reload it.",
                        "OK");
                });
                return false;
            }
        }
    }

    const bool ok = applyLoadedStateTree(stateTree);

    // After applyLoadedStateTree, load the embedded IR if one was found.
    // applyLoadedStateTree saw "builtin" and (with currentIRFile empty) skipped any
    // stock cab reload, so loading the embedded IR here takes effect cleanly.
    if (ok && embeddedCabFile.existsAsFile())
    {
        primaryCabConvolution.loadImpulseResponse(embeddedCabFile,
                                                  juce::dsp::Convolution::Stereo::yes,
                                                  juce::dsp::Convolution::Trim::yes,
                                                  0,
                                                  juce::dsp::Convolution::Normalise::yes);
        currentIRFile = embeddedCabFile;
        currentIRName = embeddedCabFile.getFileNameWithoutExtension();
        requestCabTransitionReset();
    }

    // Override the hex file path with the actual file being loaded (old .hex files
    // pre-date this field so applyLoadedStateTree would leave it empty).
    if (ok)
        activeHexFilePath = file.getFullPathName();

    return ok;
}

bool HexstackAudioProcessor::applyLoadedStateTree(const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return false;

    // Signal the audio thread to clear all buffers and mute BEFORE the parameter
    // state changes.  This eliminates the one-block "toilet bowl" artifact caused
    // by the audio thread reading new parameter values against old dirty buffers.
    presetChangeResetPending.store(true, std::memory_order_release);

    parameters.replaceState(tree);

    if (tree.hasProperty(StateKeys::programIndex))
        currentProgramIndex = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty(StateKeys::programIndex)));
    else
        currentProgramIndex = inferProgramIndexFromState(tree);

    // If the saved state was on a built-in preset (no .hex file), re-apply the
    // canonical preset values so that version updates to built-in presets are
    // always reflected instead of showing stale saved parameter values.
    if (tree.getProperty("hexFilePath", "").toString().isEmpty())
        setCurrentProgram(currentProgramIndex);

    const auto source = tree.getProperty(StateKeys::irSource).toString();
    const auto path = tree.getProperty(StateKeys::irPath).toString();
    if (source == "file")
    {
        const juce::File savedIR(path);
        if (! loadUserIRFromFile(savedIR))
            loadInfernoCabIR();
    }
    else
    {
        // Only trigger a full IR reload (which causes a JUCE convolution crossfade)
        // when transitioning FROM a user file IR to the built-in cab.  If the
        // built-in IR is already active there is nothing to reload and the
        // unnecessary crossfade is the primary cause of the "toilet bowl" artifact.
        if (currentIRFile.existsAsFile())
            loadInfernoCabIR();
    }

    applyAutomatableControlState(true);
    activeHexFilePath = tree.getProperty("hexFilePath", "").toString();
    presetChangeResetPending.store(true, std::memory_order_release);
    return true;
}

bool HexstackAudioProcessor::loadUserIRFromFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    primaryCabConvolution.loadImpulseResponse(file,
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::yes,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
    // Do NOT call reset() here — it is not audio-thread-safe when called from the
    // message thread while processBlock() is running.  resetRuntimeVoicingState()
    // (triggered via presetChangeResetPending) handles this from the audio thread.

    currentIRFile = file;
    currentIRName = file.getFileNameWithoutExtension();
    requestCabTransitionReset();
    return true;
}

void HexstackAudioProcessor::loadInfernoCabIR()
{
    currentIRFile = juce::File();
    reloadStockCabConvolutions();
    currentIRName = BuiltInCab::name;
    requestCabTransitionReset();
}

void HexstackAudioProcessor::reloadStockCabConvolutions()
{
    constexpr float builtInMicDistance = 0.18f;
    const auto primarySamples = createCabIRFromProfile(getFixedCabProfile(), builtInMicDistance);

    const auto primaryWav = createWavFromSamples(primarySamples, 48000, 2);

    primaryCabConvolution.loadImpulseResponse(primaryWav.getData(),
                                              primaryWav.getSize(),
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::yes,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
    // reset() omitted intentionally for live-session calls — see loadUserIRFromFile
    // for the explanation.  prepareToPlay() is the only caller that actually needs
    // a synchronous reset, and it goes through prepareCabinetConvolution() which
    // calls prepare() (which resets internally).
}

juce::String HexstackAudioProcessor::getCurrentIRName() const
{
    return currentIRName;
}

void HexstackAudioProcessor::prepareCabinetConvolution()
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = currentSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(currentMaxBlockSize);
    spec.numChannels = static_cast<juce::uint32>(currentNumChannels);

    primaryCabConvolution.prepare(spec);
}

std::vector<float> HexstackAudioProcessor::phaseAlignIRToReference(const std::vector<float>& reference,
                                                                   std::vector<float> candidate)
{
    if (reference.empty() || candidate.empty())
        return candidate;

    const int analysisLength = juce::jmin(384, static_cast<int>(juce::jmin(reference.size(), candidate.size())));
    const int maxShift = juce::jmin(96, analysisLength / 3);
    int bestShiftInt = 0;
    float bestScore = -1.0e9f;
    bool shouldInvert = false;

    auto weightedCorrelationAtShift = [&](float shift)
    {
        float dot = 0.0f;
        float refEnergy = 0.0f;
        float candidateEnergy = 0.0f;

        for (int i = 0; i < analysisLength; ++i)
        {
            const float candidateIndex = static_cast<float>(i) - shift;
            if (candidateIndex < 0.0f || candidateIndex >= static_cast<float>(candidate.size() - 1))
                continue;

            const int index0 = static_cast<int>(std::floor(candidateIndex));
            const int index1 = juce::jmin(index0 + 1, static_cast<int>(candidate.size()) - 1);
            const float frac = candidateIndex - static_cast<float>(index0);
            const float c = juce::jmap(frac,
                                       candidate[static_cast<size_t>(index0)],
                                       candidate[static_cast<size_t>(index1)]);
            const float r = reference[static_cast<size_t>(i)];
            const float window = 1.15f - (static_cast<float>(i) / static_cast<float>(analysisLength));

            dot += r * c * window;
            refEnergy += r * r * window;
            candidateEnergy += c * c * window;
        }

        if (refEnergy <= 1.0e-9f || candidateEnergy <= 1.0e-9f)
            return 0.0f;

        return dot / std::sqrt(refEnergy * candidateEnergy);
    };

    for (int shift = -maxShift; shift <= maxShift; ++shift)
    {
        const float norm = weightedCorrelationAtShift(static_cast<float>(shift));
        const float score = std::abs(norm);
        if (score > bestScore)
        {
            bestScore = score;
            bestShiftInt = shift;
            shouldInvert = norm < 0.0f;
        }
    }

    float bestShift = static_cast<float>(bestShiftInt);
    if (bestShiftInt > -maxShift && bestShiftInt < maxShift)
    {
        const float y1 = std::abs(weightedCorrelationAtShift(static_cast<float>(bestShiftInt - 1)));
        const float y2 = std::abs(weightedCorrelationAtShift(static_cast<float>(bestShiftInt)));
        const float y3 = std::abs(weightedCorrelationAtShift(static_cast<float>(bestShiftInt + 1)));
        const float denom = y1 - 2.0f * y2 + y3;
        if (std::abs(denom) > 1.0e-8f)
        {
            const float delta = 0.5f * (y1 - y3) / denom;
            bestShift += juce::jlimit(-0.5f, 0.5f, delta);
        }
    }

    candidate = shiftImpulseFractional(candidate, bestShift);

    if (shouldInvert)
    {
        for (auto& sample : candidate)
            sample = -sample;
    }

    normalizeImpulsePeak(candidate, 0.90f);

    return candidate;
}

juce::MemoryBlock HexstackAudioProcessor::createWavFromSamples(const std::vector<float>& samples, int sampleRate, int numChannels)
{
    constexpr int bitsPerSample = 16;

    numChannels = juce::jlimit(1, 2, numChannels);

    std::vector<int16_t> pcm;
    pcm.reserve(samples.size() * static_cast<size_t>(numChannels));
    for (const auto sample : samples)
    {
        const auto pcmSample = static_cast<int16_t>(juce::jlimit(-1.0f, 1.0f, sample) * 32767.0f);
        for (int channel = 0; channel < numChannels; ++channel)
            pcm.push_back(pcmSample);
    }

    const uint32_t dataChunkSize = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    const uint32_t fmtChunkSize = 16;
    const uint32_t riffChunkSize = 4 + (8 + fmtChunkSize) + (8 + dataChunkSize);

    juce::MemoryOutputStream stream;

    stream.write("RIFF", 4);
    stream.writeInt(static_cast<int>(riffChunkSize));
    stream.write("WAVE", 4);

    stream.write("fmt ", 4);
    stream.writeInt(static_cast<int>(fmtChunkSize));
    stream.writeShort(1);
    stream.writeShort(static_cast<short>(numChannels));
    stream.writeInt(sampleRate);
    stream.writeInt(sampleRate * numChannels * (bitsPerSample / 8));
    stream.writeShort(static_cast<short>(numChannels * (bitsPerSample / 8)));
    stream.writeShort(bitsPerSample);

    stream.write("data", 4);
    stream.writeInt(static_cast<int>(dataChunkSize));
    stream.write(pcm.data(), dataChunkSize);

    return stream.getMemoryBlock();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HexstackAudioProcessor();
}
