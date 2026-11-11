#pragma once
#include <JuceHeader.h>
#include "DSP/XVBassDSP.h"
#include "DSP/XVBassTuner.h"

namespace XVCloudConfig
{
    inline constexpr const char* kPluginVersion     = "1.6.0";
    inline constexpr int         kBuiltInPresetsRev = 1;
    inline constexpr const char* kManifestUrlMain   = "https://raw.githubusercontent.com/Somospopups/Plugin-Bass/main/xv_updates.json";
    inline constexpr const char* kManifestUrlMaster = "https://raw.githubusercontent.com/Somospopups/Plugin-Bass/master/xv_updates.json";
    inline constexpr const char* kReleasesPageUrl   = "https://github.com/Somospopups/Plugin-Bass/releases/latest";
}

namespace ParamIDs
{
    inline constexpr const char* level      = "level";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* low        = "low";
    inline constexpr const char* high       = "high";
    inline constexpr const char* blend      = "blend";
    inline constexpr const char* mid        = "mid";
    inline constexpr const char* presence   = "presence";
    inline constexpr const char* cab        = "cab";
    inline constexpr const char* bypass     = "bypass";
    inline constexpr const char* pitchOn    = "pitch_on";
    inline constexpr const char* pitchSemi  = "pitch_semi";
    inline constexpr const char* pitchCents = "pitch_cents";
    inline constexpr const char* pitchMix   = "pitch_mix";
}

struct XVFactoryPreset
{
    juce::String name;
    float level      = 5.60f;
    float drive      = 4.08f;
    float lowDb      = 3.70f;
    float midDb      = -5.80f;
    float highDb     = 0.85f;
    float presenceDb = 7.30f;
    float blendPct   = 57.6f;
    int   cabIndex   = 1; // 0 = BASS, 1 = PUNK, 2 = CLEAN
    juce::String tuningHint;
};

struct XVUserPreset
{
    juce::String name;
    float level          = 5.60f;
    float drive          = 4.08f;
    float lowDb          = 3.70f;
    float midDb          = -5.80f;
    float highDb         = 0.85f;
    float presenceDb     = 7.30f;
    float blendPct       = 57.6f;
    int   cabIndex       = 1;
    bool  hasSavedPitch  = false;
    bool  pitchEnabled   = false;
    int   pitchSemitones = 0;
    float pitchCents     = 0.0f;
    float pitchMixPct    = 100.0f;
};

class XVBassPedalAudioProcessor final : public juce::AudioProcessor
{
public:
    XVBassPedalAudioProcessor();
    ~XVBassPedalAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgramIndex; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // Snapshot actual para motor DSP y exportador de pedaleras
    xvdsp::PedalControlSnapshot buildCurrentSnapshot() const noexcept;

    // Chromatic Bass Tuner accessors (lock-free)
    xvdsp::XVBassTuner& getTuner() noexcept { return bassTuner; }
    void setTunerActive (bool shouldBeActive) noexcept { tunerActive.store (shouldBeActive, std::memory_order_relaxed); }
    bool isTunerActive() const noexcept { return tunerActive.load (std::memory_order_relaxed); }
    void setTunerMuted (bool shouldMute) noexcept { tunerMuted.store (shouldMute, std::memory_order_relaxed); }
    bool isTunerMuted() const noexcept { return tunerMuted.load (std::memory_order_relaxed); }

    // Helpers directos para el módulo Digital Pitch Shifter
    bool  isPitchEnabled() const noexcept;
    int   getPitchSemitones() const noexcept;
    float getPitchCents() const noexcept;
    float getPitchMixPct() const noexcept;
    void  setPitchEnabled (bool enabled);
    void  setPitchSemitones (int semitones);
    void  setPitchCents (float cents);
    void  setPitchMixPct (float mixPct);

    // Presets (Fábrica + Nube OTA + Usuario)
    const std::vector<XVFactoryPreset>& getFactoryPresets() const noexcept { return activeFactoryPresets; }
    static std::vector<XVFactoryPreset> getBuiltInFactoryPresets();
    juce::String getProgramTuningHint (int index) const;
    juce::String getProgramSelectionHud (int index) const;

    void reloadUserPresets();
    const std::vector<XVUserPreset>& getUserPresets() const noexcept { return userPresets; }
    juce::String getDefaultStartupPresetName() const { return defaultPresetName; }
    bool isActiveUserPreset() const noexcept;
    int  getActiveUserPresetIndex() const noexcept;
    juce::String getActivePresetDisplayName() const;
    juce::String getActivePresetCleanName() const;

    int  saveCurrentAsUserPreset (const juce::String& presetName, bool makeDefaultStartup);
    bool deleteUserPreset (int userIndex);
    void setPresetAsDefaultStartup (const juce::String& presetName);
    void saveCurrentSettingsAsDefaultStartup();
    void clearDefaultStartupPreset();

    // Sincronización OTA en vivo con GitHub (Somospopups/Plugin-Bass) + Auto-Actualizador
    void triggerCloudSyncAsync (bool forceNow, std::function<void (const juce::String&)> onUiMessage = nullptr);
    bool hasPluginUpdateAvailable() const noexcept { return updateAvailable.load (std::memory_order_relaxed); }
    bool isDownloadingUpdate() const noexcept      { return downloadingUpdate.load (std::memory_order_relaxed); }
    juce::String getRemotePluginVersion() const    { return remotePluginVersion; }
    juce::String getRemoteUpdateNotes() const      { return remoteUpdateNotes; }
    void launchPluginUpdateAsync (std::function<void (const juce::String&)> onUiMessage = nullptr);

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static int compareVersions (const juce::String& a, const juce::String& b);

private:
    static juce::File getAppDataDir();
    static juce::File getUserPresetsFile();
    static juce::File getCloudCacheFile();

    void loadCachedCloudManifestFromDisk();
    bool applyManifestJson (const juce::String& jsonText, bool saveToCache, int& outAddedOrUpdated);

    void saveUserPresetsToDisk() const;
    void applyPresetValues (float lvl, float drv, float lo, float mid, float hi, float pres, float blnd, int cab);
    void setParamNormalizedValue (const char* paramId, float plainValue);

    xvdsp::XVBassDSP   dspEngine;
    xvdsp::XVBassTuner bassTuner;
    std::atomic<bool>  tunerActive { false };
    std::atomic<bool>  tunerMuted  { false };

    std::vector<XVFactoryPreset> activeFactoryPresets;
    int activePresetsRevision = XVCloudConfig::kBuiltInPresetsRev;

    std::atomic<bool> updateAvailable   { false };
    std::atomic<bool> downloadingUpdate { false };
    juce::String remotePluginVersion;
    juce::String remoteUpdateNotes;
    juce::String remoteWebDownloadUrlMac { XVCloudConfig::kReleasesPageUrl };
    juce::String remoteWebDownloadUrlWin { XVCloudConfig::kReleasesPageUrl };
    juce::String remoteDirectPkgUrlMac;
    juce::String remoteDirectExeUrlWin;

    std::shared_ptr<std::atomic<bool>> aliveFlag;
    std::unique_ptr<juce::Thread>      cloudSyncThread;
    std::unique_ptr<juce::Thread>      updateDownloadThread;

    std::vector<XVUserPreset> userPresets;
    juce::String defaultPresetName;
    int currentProgramIndex = 0;

    std::atomic<float>* levelParam      = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* lowParam        = nullptr;
    std::atomic<float>* highParam       = nullptr;
    std::atomic<float>* blendParam      = nullptr;
    std::atomic<float>* midParam        = nullptr;
    std::atomic<float>* presenceParam   = nullptr;
    std::atomic<float>* cabParam        = nullptr;
    std::atomic<float>* bypassParam     = nullptr;
    std::atomic<float>* pitchOnParam    = nullptr;
    std::atomic<float>* pitchSemiParam  = nullptr;
    std::atomic<float>* pitchCentsParam = nullptr;
    std::atomic<float>* pitchMixParam   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XVBassPedalAudioProcessor)
};
