#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    std::atomic<juce::int64> gLastAutoCloudCheckMs { 0 };

    class LambdaWorkerThread final : public juce::Thread
    {
    public:
        LambdaWorkerThread (const juce::String& threadName, std::function<void (juce::Thread&)> fn)
            : juce::Thread (threadName), workFn (std::move (fn))
        {
        }

        void run() override
        {
            if (workFn)
                workFn (*this);
        }

    private:
        std::function<void (juce::Thread&)> workFn;
    };

    juce::String fetchUrlTextWithTimeout (const juce::String& urlStr, int timeoutMs)
    {
        juce::URL url (urlStr);
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (timeoutMs)
                        .withNumRedirectsToFollow (5);
        if (auto stream = url.createInputStream (opts))
            return stream->readEntireStreamAsString();
        return {};
    }
}

std::vector<XVFactoryPreset> XVBassPedalAudioProcessor::getBuiltInFactoryPresets()
{
    return {
        //  Nombre                                   LVL    DRV    LOW     MID    HIGH   PRES   BLND   CAB  Aviso de Afinacion
        { "XV",                                     5.60f, 4.08f,  3.70f, -5.80f, 0.85f, 7.30f, 57.6f, 1,  "Seteo Original XV  |  Afinacion estandar (E A D G)" },
        { "Jimmy Eat World - The Middle",           5.90f, 3.10f,  2.40f, -0.60f, 1.60f, 4.20f, 48.0f, 1,  "Afinacion del tema: Drop D (4ta cuerda en D)" },
        { "The Ataris - In This Diary",             5.60f, 4.60f,  2.20f,  1.40f, 2.00f, 5.80f, 64.0f, 1,  "Afinacion del tema: Mi bemol (Eb)  -  Tip: Pitch -1 st" },
        { "blink-182 - Dance With Me",              5.40f, 6.60f,  3.20f, -3.40f, 2.00f, 8.20f, 78.0f, 1,  "Afinacion del tema: Estandar (E A D G)" },
        { "NFG - Failure's Not Flattering",         5.50f, 6.20f,  3.40f, -4.20f, 2.80f, 7.80f, 74.0f, 1,  "Afinacion del tema: Drop D / Estandar" },
        { "Neck Deep - Heartbreak Century",         5.40f, 7.20f,  3.50f, -3.80f, 2.60f, 8.80f, 76.0f, 1,  "Afinacion del tema: Mi bemol (Eb)  -  Tip: Pitch -1 st" },
        { "ADTR - All I Want",                      5.40f, 7.80f,  3.00f, -3.20f, 3.20f, 9.00f, 82.0f, 1,  "Afinacion del tema: Drop C (C G C F)" },
        { "FYS - Learn To Love The Lie",            4.90f, 8.10f,  3.80f,  1.60f, 1.80f, 6.80f, 72.0f, 0,  "Afinacion del tema: D Standard  -  Tip: Pitch -2 st" },
        { "NOFX - Stickin' In My Eye",              6.10f, 3.60f,  1.80f, -2.60f, 3.80f, 8.40f, 54.0f, 1,  "Afinacion del tema: Estandar (E A D G)" },
        { "Yellowcard - Ocean Avenue",              5.40f, 4.50f,  3.20f, -2.20f, 1.20f, 5.20f, 56.0f, 0,  "Afinacion del tema: Drop D (4ta cuerda en D)" },
        { "Millencolin - Man Or Mouse",             5.30f, 6.80f,  2.60f,  2.20f, 2.40f, 7.60f, 70.0f, 1,  "Afinacion del tema: Estandar (E A D G)" }
    };
}

int XVBassPedalAudioProcessor::compareVersions (const juce::String& a, const juce::String& b)
{
    auto cleanA = a.trim().trimCharactersAtStart ("vV");
    auto cleanB = b.trim().trimCharactersAtStart ("vV");
    juce::StringArray tokA, tokB;
    tokA.addTokens (cleanA, ".", "");
    tokB.addTokens (cleanB, ".", "");
    const int n = juce::jmax (tokA.size(), tokB.size());
    for (int i = 0; i < n; ++i)
    {
        const int va = (i < tokA.size()) ? tokA[i].getIntValue() : 0;
        const int vb = (i < tokB.size()) ? tokB[i].getIntValue() : 0;
        if (va != vb)
            return (va > vb) ? 1 : -1;
    }
    return 0;
}

juce::File XVBassPedalAudioProcessor::getAppDataDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("XV Audio")
                   .getChildFile ("XV Bass Pedal");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::File XVBassPedalAudioProcessor::getUserPresetsFile()
{
    return getAppDataDir().getChildFile ("user_presets.xml");
}

juce::File XVBassPedalAudioProcessor::getCloudCacheFile()
{
    return getAppDataDir().getChildFile ("cloud_manifest_cache.json");
}

void XVBassPedalAudioProcessor::loadCachedCloudManifestFromDisk()
{
    const auto cacheFile = getCloudCacheFile();
    if (! cacheFile.existsAsFile())
        return;

    const auto text = cacheFile.loadFileAsString();
    if (text.isNotEmpty())
    {
        int dummy = 0;
        applyManifestJson (text, false, dummy);
    }
}

bool XVBassPedalAudioProcessor::applyManifestJson (const juce::String& jsonText, bool saveToCache, int& outAddedOrUpdated)
{
    outAddedOrUpdated = 0;
    const juce::var parsed = juce::JSON::parse (jsonText);
    const auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return false;

    const juce::String ver = obj->getProperty ("pluginVersion").toString().trim();
    if (ver.isNotEmpty())
    {
        remotePluginVersion = ver;
        remoteUpdateNotes   = obj->getProperty ("updateNotes").toString().trim();

        const auto dlMac = obj->getProperty ("downloadUrlMac").toString().trim();
        const auto dlWin = obj->getProperty ("downloadUrlWin").toString().trim();
        if (dlMac.isNotEmpty()) remoteWebDownloadUrlMac = dlMac;
        if (dlWin.isNotEmpty()) remoteWebDownloadUrlWin = dlWin;

        remoteDirectPkgUrlMac = obj->getProperty ("pkgDirectUrlMac").toString().trim();
        remoteDirectExeUrlWin = obj->getProperty ("exeDirectUrlWin").toString().trim();

        updateAvailable.store (compareVersions (ver, XVCloudConfig::kPluginVersion) > 0, std::memory_order_relaxed);
    }

    const int remoteRev = static_cast<int> (obj->getProperty ("presetsRevision"));
    const juce::var presetsVar = obj->getProperty ("presets");

    if (presetsVar.isArray() && remoteRev >= activePresetsRevision)
    {
        const auto* arr = presetsVar.getArray();
        if (arr != nullptr && ! arr->isEmpty())
        {
            std::vector<XVFactoryPreset> loaded;
            loaded.reserve (static_cast<size_t> (arr->size()));

            for (const auto& item : *arr)
            {
                if (const auto* po = item.getDynamicObject())
                {
                    XVFactoryPreset fp;
                    fp.name       = po->getProperty ("name").toString().trim();
                    if (fp.name.isEmpty())
                        continue;
                    fp.level      = juce::jlimit (0.0f, 10.0f,   static_cast<float> (po->getProperty ("level")));
                    fp.drive      = juce::jlimit (0.0f, 10.0f,   static_cast<float> (po->getProperty ("drive")));
                    fp.lowDb      = juce::jlimit (-12.0f, 12.0f, static_cast<float> (po->getProperty ("lowDb")));
                    fp.midDb      = juce::jlimit (-12.0f, 12.0f, static_cast<float> (po->getProperty ("midDb")));
                    fp.highDb     = juce::jlimit (-12.0f, 12.0f, static_cast<float> (po->getProperty ("highDb")));
                    fp.presenceDb = juce::jlimit (-12.0f, 12.0f, static_cast<float> (po->getProperty ("presenceDb")));
                    fp.blendPct   = juce::jlimit (0.0f, 100.0f,  static_cast<float> (po->getProperty ("blendPct")));
                    fp.cabIndex   = juce::jlimit (0, 2,          static_cast<int>   (po->getProperty ("cabIndex")));
                    fp.tuningHint = po->getProperty ("tuningHint").toString().trim();
                    loaded.push_back (std::move (fp));
                }
            }

            if (! loaded.empty())
            {
                const bool revIncreased = (remoteRev > activePresetsRevision)
                                       || (loaded.size() != activeFactoryPresets.size());
                activeFactoryPresets  = std::move (loaded);
                activePresetsRevision = remoteRev;
                currentProgramIndex   = juce::jlimit (0, juce::jmax (0, getNumPrograms() - 1), currentProgramIndex);
                if (revIncreased)
                    outAddedOrUpdated = static_cast<int> (activeFactoryPresets.size());
            }
        }
    }

    if (saveToCache)
        getCloudCacheFile().replaceWithText (jsonText);

    return true;
}

void XVBassPedalAudioProcessor::triggerCloudSyncAsync (bool forceNow,
                                                       std::function<void (const juce::String&)> onUiMessage)
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    if (! forceNow)
    {
        const auto prevMs = gLastAutoCloudCheckMs.load (std::memory_order_relaxed);
        if (prevMs > 0 && (static_cast<juce::int64> (nowMs) - prevMs) < 120000)
            return;
    }
    gLastAutoCloudCheckMs.store (static_cast<juce::int64> (nowMs), std::memory_order_relaxed);

    if (cloudSyncThread != nullptr && cloudSyncThread->isThreadRunning())
    {
        if (! forceNow)
            return;
        cloudSyncThread->stopThread (1500);
    }

    auto alive = aliveFlag;
    cloudSyncThread = std::make_unique<LambdaWorkerThread> ("XVCloudSync",
        [this, alive, forceNow, onUiMessage = std::move (onUiMessage)] (juce::Thread& th)
        {
            if (th.threadShouldExit() || ! alive->load (std::memory_order_relaxed))
                return;

            const juce::String bust = "?t=" + juce::String (juce::Time::currentTimeMillis() / 60000);
            juce::String body = fetchUrlTextWithTimeout (juce::String (XVCloudConfig::kManifestUrlMain) + bust, 3500);

            if (body.isEmpty() && ! th.threadShouldExit())
                body = fetchUrlTextWithTimeout (juce::String (XVCloudConfig::kManifestUrlMaster) + bust, 3500);

            if (th.threadShouldExit() || ! alive->load (std::memory_order_relaxed))
                return;

            juce::MessageManager::callAsync ([this, alive, forceNow, body, onUiMessage]()
            {
                if (! alive->load (std::memory_order_relaxed))
                    return;

                if (body.isEmpty())
                {
                    if (forceNow && onUiMessage)
                        onUiMessage ("Canal en la nube aun sin publicar o sin conexion");
                    return;
                }

                int updatedTotal = 0;
                const bool ok = applyManifestJson (body, true, updatedTotal);
                updateHostDisplay();

                if (! ok)
                {
                    if (forceNow && onUiMessage)
                        onUiMessage ("No se pudo leer xv_updates.json");
                    return;
                }

                if (onUiMessage)
                {
                    if (hasPluginUpdateAvailable())
                        onUiMessage ("Nueva version v" + remotePluginVersion + " disponible para descargar");
                    else if (updatedTotal > 0)
                        onUiMessage ("Nube sincronizada: " + juce::String (updatedTotal) + " presets al dia");
                    else if (forceNow)
                        onUiMessage ("Todo al dia (" + juce::String (activeFactoryPresets.size()) + " presets sincronizados)");
                }
            });
        });

    cloudSyncThread->startThread (juce::Thread::Priority::low);
}

void XVBassPedalAudioProcessor::launchPluginUpdateAsync (std::function<void (const juce::String&)> onUiMessage)
{
    if (downloadingUpdate.load (std::memory_order_relaxed))
        return;

#if JUCE_MAC && ! JUCE_IOS
    const juce::String directUrl   = remoteDirectPkgUrlMac;
    const juce::String fallbackWeb = remoteWebDownloadUrlMac.isNotEmpty()
                                         ? remoteWebDownloadUrlMac
                                         : juce::String (XVCloudConfig::kReleasesPageUrl);
    const juce::String fileName    = "Instalar_XV_Bass_Pedal_Mac_Universal.pkg";
#elif JUCE_WINDOWS
    const juce::String directUrl   = remoteDirectExeUrlWin;
    const juce::String fallbackWeb = remoteWebDownloadUrlWin.isNotEmpty()
                                         ? remoteWebDownloadUrlWin
                                         : juce::String (XVCloudConfig::kReleasesPageUrl);
    const juce::String fileName    = "Instalar_XV_Bass_Pedal_Windows_x64.exe";
#else
    const juce::String directUrl;
    const juce::String fallbackWeb = XVCloudConfig::kReleasesPageUrl;
    const juce::String fileName    = "XV_Bass_Pedal_Update";
#endif

    if (directUrl.isEmpty())
    {
        juce::URL (fallbackWeb).launchInDefaultBrowser();
        if (onUiMessage)
            onUiMessage ("Abriendo pagina de actualizacion oficial...");
        return;
    }

    downloadingUpdate.store (true, std::memory_order_relaxed);
    if (onUiMessage)
        onUiMessage ("Descargando actualizacion v" + remotePluginVersion + "...");

    if (updateDownloadThread != nullptr && updateDownloadThread->isThreadRunning())
        updateDownloadThread->stopThread (1500);

    auto alive = aliveFlag;
    updateDownloadThread = std::make_unique<LambdaWorkerThread> ("XVAutoUpdater",
        [this, alive, directUrl, fallbackWeb, fileName, onUiMessage = std::move (onUiMessage)] (juce::Thread& th)
        {
            bool launchedDirect = false;
            const auto destFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile (fileName);

            juce::URL url (directUrl);
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (8000)
                            .withNumRedirectsToFollow (8);

            if (auto in = url.createInputStream (opts))
            {
                destFile.deleteFile();
                juce::FileOutputStream out (destFile);
                if (out.openedOk())
                {
                    juce::MemoryBlock chunk (65536);
                    while (! in->isExhausted() && ! th.threadShouldExit())
                    {
                        const int r = in->read (chunk.getData(), static_cast<int> (chunk.getSize()));
                        if (r <= 0)
                            break;
                        out.write (chunk.getData(), static_cast<size_t> (r));
                    }
                    out.flush();
                }

                if (! th.threadShouldExit() && destFile.existsAsFile() && destFile.getSize() > 50000)
                {
#if JUCE_MAC && ! JUCE_IOS
                    {
                        juce::ChildProcess unquarantine;
                        unquarantine.start (juce::StringArray { "/usr/bin/xattr", "-cr", destFile.getFullPathName() });
                        unquarantine.waitForProcessToFinish (2500);

                        juce::ChildProcess openPkg;
                        launchedDirect = openPkg.start (juce::StringArray { "/usr/bin/open", destFile.getFullPathName() });
                    }
#elif ! JUCE_IOS
                    launchedDirect = destFile.startAsProcess();
#endif
                }
            }

            if (! alive->load (std::memory_order_relaxed))
                return;

            juce::MessageManager::callAsync ([this, alive, launchedDirect, fallbackWeb, onUiMessage]()
            {
                if (! alive->load (std::memory_order_relaxed))
                    return;

                downloadingUpdate.store (false, std::memory_order_relaxed);
                if (launchedDirect)
                {
                    if (onUiMessage)
                        onUiMessage ("Instalador abierto - cierra tu DAW para completar la actualizacion");
                }
                else
                {
                    juce::URL (fallbackWeb).launchInDefaultBrowser();
                    if (onUiMessage)
                        onUiMessage ("Abriendo descarga oficial en tu navegador...");
                }
            });
        });

    updateDownloadThread->startThread (juce::Thread::Priority::normal);
}

void XVBassPedalAudioProcessor::reloadUserPresets()
{
    userPresets.clear();
    defaultPresetName.clear();

    const auto file = getUserPresetsFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (xml->hasTagName ("XVBassPresets"))
        {
            defaultPresetName = xml->getStringAttribute ("defaultPresetName", {});
            for (auto* child : xml->getChildWithTagNameIterator ("Preset"))
            {
                XVUserPreset up;
                up.name           = child->getStringAttribute ("name", "Mi Preset");
                up.level          = static_cast<float> (child->getDoubleAttribute ("level", 5.60));
                up.drive          = static_cast<float> (child->getDoubleAttribute ("drive", 4.08));
                up.lowDb          = static_cast<float> (child->getDoubleAttribute ("lowDb", 3.70));
                up.midDb          = static_cast<float> (child->getDoubleAttribute ("midDb", -5.80));
                up.highDb         = static_cast<float> (child->getDoubleAttribute ("highDb", 0.85));
                up.presenceDb     = static_cast<float> (child->getDoubleAttribute ("presenceDb", 7.30));
                up.blendPct       = static_cast<float> (child->getDoubleAttribute ("blendPct", 57.6));
                up.cabIndex       = child->getIntAttribute ("cabIndex", 1);
                up.hasSavedPitch  = child->hasAttribute ("pitchOn");
                up.pitchEnabled   = child->getBoolAttribute ("pitchOn", false);
                up.pitchSemitones = child->getIntAttribute ("pitchSemi", 0);
                up.pitchCents     = static_cast<float> (child->getDoubleAttribute ("pitchCents", 0.0));
                up.pitchMixPct    = static_cast<float> (child->getDoubleAttribute ("pitchMix", 100.0));
                userPresets.push_back (up);
            }
        }
    }
}

void XVBassPedalAudioProcessor::saveUserPresetsToDisk() const
{
    juce::XmlElement root ("XVBassPresets");
    root.setAttribute ("defaultPresetName", defaultPresetName);

    for (const auto& up : userPresets)
    {
        auto* item = root.createNewChildElement ("Preset");
        item->setAttribute ("name",       up.name);
        item->setAttribute ("level",      static_cast<double> (up.level));
        item->setAttribute ("drive",      static_cast<double> (up.drive));
        item->setAttribute ("lowDb",      static_cast<double> (up.lowDb));
        item->setAttribute ("midDb",      static_cast<double> (up.midDb));
        item->setAttribute ("highDb",     static_cast<double> (up.highDb));
        item->setAttribute ("presenceDb", static_cast<double> (up.presenceDb));
        item->setAttribute ("blendPct",   static_cast<double> (up.blendPct));
        item->setAttribute ("cabIndex",   up.cabIndex);
        if (up.hasSavedPitch)
        {
            item->setAttribute ("pitchOn",    up.pitchEnabled);
            item->setAttribute ("pitchSemi",  up.pitchSemitones);
            item->setAttribute ("pitchCents", static_cast<double> (up.pitchCents));
            item->setAttribute ("pitchMix",   static_cast<double> (up.pitchMixPct));
        }
    }

    root.writeTo (getUserPresetsFile());
}

XVBassPedalAudioProcessor::XVBassPedalAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XV_PARAMETERS", createParameterLayout()),
      activeFactoryPresets (getBuiltInFactoryPresets()),
      aliveFlag (std::make_shared<std::atomic<bool>> (true))
{
    levelParam      = apvts.getRawParameterValue (ParamIDs::level);
    driveParam      = apvts.getRawParameterValue (ParamIDs::drive);
    lowParam        = apvts.getRawParameterValue (ParamIDs::low);
    highParam       = apvts.getRawParameterValue (ParamIDs::high);
    blendParam      = apvts.getRawParameterValue (ParamIDs::blend);
    midParam        = apvts.getRawParameterValue (ParamIDs::mid);
    presenceParam   = apvts.getRawParameterValue (ParamIDs::presence);
    cabParam        = apvts.getRawParameterValue (ParamIDs::cab);
    bypassParam     = apvts.getRawParameterValue (ParamIDs::bypass);
    pitchOnParam    = apvts.getRawParameterValue (ParamIDs::pitchOn);
    pitchSemiParam  = apvts.getRawParameterValue (ParamIDs::pitchSemi);
    pitchCentsParam = apvts.getRawParameterValue (ParamIDs::pitchCents);
    pitchMixParam   = apvts.getRawParameterValue (ParamIDs::pitchMix);

    loadCachedCloudManifestFromDisk();
    reloadUserPresets();

    if (defaultPresetName.isNotEmpty())
    {
        const int total = getNumPrograms();
        for (int i = 0; i < total; ++i)
        {
            if (getProgramName (i) == defaultPresetName)
            {
                setCurrentProgram (i);
                break;
            }
        }
    }
    else
    {
        // Preset #1 por defecto al abrir: "XV" (tu seteo exacto)
        setCurrentProgram (0);
    }
}

XVBassPedalAudioProcessor::~XVBassPedalAudioProcessor()
{
    if (aliveFlag != nullptr)
        aliveFlag->store (false, std::memory_order_relaxed);
    if (cloudSyncThread != nullptr)
        cloudSyncThread->stopThread (2500);
    if (updateDownloadThread != nullptr)
        updateDownloadThread->stopThread (2500);
}

juce::AudioProcessorValueTreeState::ParameterLayout XVBassPedalAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto dbFormat   = [] (float v, int) { return (v >= 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };
    auto dialFormat = [] (float v, int) { return juce::String (v, 1); };
    auto pctFormat  = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; };
    auto ctFormat   = [] (float v, int) { const int c = juce::roundToInt (v); return (c > 0 ? "+" : "") + juce::String (c) + " ct"; };

    // Valores iniciales exactos del Preset #1 ("XV")
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "LEVEL",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.60f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dialFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drive, 1 }, "DRIVE",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 4.08f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dialFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::low, 1 }, "LOW",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 3.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::high, 1 }, "HIGH",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.85f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::blend, 1 }, "BLEND",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 57.6f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mid, 1 }, "MID",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), -5.8f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::presence, 1 }, "PRESENCE",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 7.3f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbFormat)));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::cab, 1 }, "CAB",
        juce::StringArray { "BASS", "PUNK", "CLEAN" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::bypass, 1 }, "BYPASS", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::pitchOn, 1 }, "PITCH ON", false));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::pitchSemi, 1 }, "PITCH SEMITONES", -12, 12, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchCents, 1 }, "PITCH CENTS",
        juce::NormalisableRange<float> (-50.0f, 50.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (ctFormat)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::pitchMix, 1 }, "PITCH MIX",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctFormat)));

    return { params.begin(), params.end() };
}

bool XVBassPedalAudioProcessor::isPitchEnabled() const noexcept
{
    return pitchOnParam != nullptr && pitchOnParam->load() >= 0.5f;
}

int XVBassPedalAudioProcessor::getPitchSemitones() const noexcept
{
    return pitchSemiParam != nullptr ? juce::jlimit (-12, 12, juce::roundToInt (pitchSemiParam->load())) : 0;
}

float XVBassPedalAudioProcessor::getPitchCents() const noexcept
{
    return pitchCentsParam != nullptr ? juce::jlimit (-50.0f, 50.0f, pitchCentsParam->load()) : 0.0f;
}

float XVBassPedalAudioProcessor::getPitchMixPct() const noexcept
{
    return pitchMixParam != nullptr ? juce::jlimit (0.0f, 100.0f, pitchMixParam->load()) : 100.0f;
}

void XVBassPedalAudioProcessor::setPitchEnabled (bool enabled)
{
    setParamNormalizedValue (ParamIDs::pitchOn, enabled ? 1.0f : 0.0f);
}

void XVBassPedalAudioProcessor::setPitchSemitones (int semitones)
{
    setParamNormalizedValue (ParamIDs::pitchSemi, static_cast<float> (juce::jlimit (-12, 12, semitones)));
}

void XVBassPedalAudioProcessor::setPitchCents (float cents)
{
    setParamNormalizedValue (ParamIDs::pitchCents, juce::jlimit (-50.0f, 50.0f, cents));
}

void XVBassPedalAudioProcessor::setPitchMixPct (float mixPct)
{
    setParamNormalizedValue (ParamIDs::pitchMix, juce::jlimit (0.0f, 100.0f, mixPct));
}

xvdsp::PedalControlSnapshot XVBassPedalAudioProcessor::buildCurrentSnapshot() const noexcept
{
    xvdsp::PedalControlSnapshot snapshot;
    snapshot.level          = levelParam->load();
    snapshot.drive          = driveParam->load();
    snapshot.lowDb          = lowParam->load();
    snapshot.highDb         = highParam->load();
    snapshot.blend          = blendParam->load() * 0.01f;
    snapshot.midDb          = midParam->load();
    snapshot.presenceDb     = presenceParam->load();
    snapshot.cab            = static_cast<xvdsp::CabMode> (juce::jlimit (0, 2, juce::roundToInt (cabParam->load())));
    snapshot.bypassed       = (bypassParam->load() >= 0.5f);
    snapshot.tunerMuted     = tunerMuted.load (std::memory_order_relaxed);
    snapshot.pitchEnabled   = isPitchEnabled();
    snapshot.pitchSemitones = getPitchSemitones();
    snapshot.pitchCents     = getPitchCents();
    snapshot.pitchMix       = getPitchMixPct() * 0.01f;
    return snapshot;
}

int XVBassPedalAudioProcessor::getNumPrograms()
{
    return static_cast<int> (activeFactoryPresets.size() + userPresets.size());
}

const juce::String XVBassPedalAudioProcessor::getProgramName (int index)
{
    const int numFactory = static_cast<int> (activeFactoryPresets.size());
    if (index >= 0 && index < numFactory)
        return activeFactoryPresets[static_cast<size_t> (index)].name;

    const int uIdx = index - numFactory;
    if (uIdx >= 0 && uIdx < static_cast<int> (userPresets.size()))
        return userPresets[static_cast<size_t> (uIdx)].name;

    return {};
}

juce::String XVBassPedalAudioProcessor::getProgramTuningHint (int index) const
{
    const int numFactory = static_cast<int> (activeFactoryPresets.size());
    if (index >= 0 && index < numFactory)
        return activeFactoryPresets[static_cast<size_t> (index)].tuningHint;
    return {};
}

juce::String XVBassPedalAudioProcessor::getProgramSelectionHud (int index) const
{
    const auto nm = const_cast<XVBassPedalAudioProcessor*> (this)->getProgramName (index);
    const auto hint = getProgramTuningHint (index);
    if (hint.isNotEmpty())
        return nm + "   |   " + hint;
    return nm;
}

bool XVBassPedalAudioProcessor::isActiveUserPreset() const noexcept
{
    const int numFactory = static_cast<int> (activeFactoryPresets.size());
    return currentProgramIndex >= numFactory
        && (currentProgramIndex - numFactory) < static_cast<int> (userPresets.size());
}

int XVBassPedalAudioProcessor::getActiveUserPresetIndex() const noexcept
{
    if (! isActiveUserPreset())
        return -1;
    return currentProgramIndex - static_cast<int> (activeFactoryPresets.size());
}

juce::String XVBassPedalAudioProcessor::getActivePresetCleanName() const
{
    const auto name = const_cast<XVBassPedalAudioProcessor*> (this)->getProgramName (currentProgramIndex);
    return name.isEmpty() ? juce::String ("XV") : name;
}

juce::String XVBassPedalAudioProcessor::getActivePresetDisplayName() const
{
    return getActivePresetCleanName();
}

void XVBassPedalAudioProcessor::setParamNormalizedValue (const char* paramId, float plainValue)
{
    if (auto* p = apvts.getParameter (paramId))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void XVBassPedalAudioProcessor::applyPresetValues (float lvl, float drv, float lo, float mid, float hi, float pres, float blnd, int cab)
{
    setParamNormalizedValue (ParamIDs::level,    lvl);
    setParamNormalizedValue (ParamIDs::drive,    drv);
    setParamNormalizedValue (ParamIDs::low,      lo);
    setParamNormalizedValue (ParamIDs::mid,      mid);
    setParamNormalizedValue (ParamIDs::high,     hi);
    setParamNormalizedValue (ParamIDs::presence, pres);
    setParamNormalizedValue (ParamIDs::blend,    blnd);
    setParamNormalizedValue (ParamIDs::cab,      static_cast<float> (cab));
    setParamNormalizedValue (ParamIDs::bypass,   0.0f);
}

void XVBassPedalAudioProcessor::setCurrentProgram (int index)
{
    const int numFactory = static_cast<int> (activeFactoryPresets.size());

    if (index >= 0 && index < numFactory)
    {
        currentProgramIndex = index;
        const auto& pr = activeFactoryPresets[static_cast<size_t> (index)];
        // Cambia solo el sonido/perillas sin alterar la afinacion de PITCH (modo solo_aviso)
        applyPresetValues (pr.level, pr.drive, pr.lowDb, pr.midDb, pr.highDb, pr.presenceDb, pr.blendPct, pr.cabIndex);
        return;
    }

    const int uIdx = index - numFactory;
    if (uIdx >= 0 && uIdx < static_cast<int> (userPresets.size()))
    {
        currentProgramIndex = index;
        const auto& up = userPresets[static_cast<size_t> (uIdx)];
        applyPresetValues (up.level, up.drive, up.lowDb, up.midDb, up.highDb, up.presenceDb, up.blendPct, up.cabIndex);
        if (up.hasSavedPitch)
        {
            setPitchEnabled (up.pitchEnabled);
            setPitchSemitones (up.pitchSemitones);
            setPitchCents (up.pitchCents);
            setPitchMixPct (up.pitchMixPct);
        }
    }
}

int XVBassPedalAudioProcessor::saveCurrentAsUserPreset (const juce::String& presetName, bool makeDefaultStartup)
{
    juce::String cleanName = presetName.trim();
    if (cleanName.isEmpty())
        cleanName = "Mi Bajo XV " + juce::String (userPresets.size() + 1);

    XVUserPreset up;
    up.name           = cleanName;
    up.level          = levelParam->load();
    up.drive          = driveParam->load();
    up.lowDb          = lowParam->load();
    up.midDb          = midParam->load();
    up.highDb         = highParam->load();
    up.presenceDb     = presenceParam->load();
    up.blendPct       = blendParam->load();
    up.cabIndex       = juce::jlimit (0, 2, juce::roundToInt (cabParam->load()));
    up.hasSavedPitch  = true;
    up.pitchEnabled   = isPitchEnabled();
    up.pitchSemitones = getPitchSemitones();
    up.pitchCents     = getPitchCents();
    up.pitchMixPct    = getPitchMixPct();

    int foundIdx = -1;
    for (int i = 0; i < static_cast<int> (userPresets.size()); ++i)
    {
        if (userPresets[static_cast<size_t> (i)].name.equalsIgnoreCase (cleanName))
        {
            userPresets[static_cast<size_t> (i)] = up;
            foundIdx = i;
            break;
        }
    }

    if (foundIdx < 0)
    {
        userPresets.push_back (up);
        foundIdx = static_cast<int> (userPresets.size()) - 1;
    }

    if (makeDefaultStartup)
        defaultPresetName = up.name;

    currentProgramIndex = static_cast<int> (activeFactoryPresets.size()) + foundIdx;
    saveUserPresetsToDisk();
    updateHostDisplay();
    return currentProgramIndex;
}

bool XVBassPedalAudioProcessor::deleteUserPreset (int userIndex)
{
    if (userIndex < 0 || userIndex >= static_cast<int> (userPresets.size()))
        return false;

    const auto removedName = userPresets[static_cast<size_t> (userIndex)].name;
    userPresets.erase (userPresets.begin() + userIndex);

    if (defaultPresetName == removedName)
        defaultPresetName.clear();

    currentProgramIndex = 0;
    setCurrentProgram (0);
    saveUserPresetsToDisk();
    updateHostDisplay();
    return true;
}

void XVBassPedalAudioProcessor::setPresetAsDefaultStartup (const juce::String& presetName)
{
    defaultPresetName = presetName.trim();
    saveUserPresetsToDisk();
}

void XVBassPedalAudioProcessor::saveCurrentSettingsAsDefaultStartup()
{
    saveCurrentAsUserPreset ("Mi Sonido Predeterminado", true);
}

void XVBassPedalAudioProcessor::clearDefaultStartupPreset()
{
    defaultPresetName.clear();
    saveUserPresetsToDisk();
}

juce::AudioProcessorParameter* XVBassPedalAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamIDs::bypass);
}

void XVBassPedalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dspEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    bassTuner.prepare (sampleRate);
}

void XVBassPedalAudioProcessor::releaseResources()
{
    dspEngine.reset();
    bassTuner.reset();
}

bool XVBassPedalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();

    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;

    return inSet == outSet || (inSet == juce::AudioChannelSet::mono() && outSet == juce::AudioChannelSet::stereo());
}

void XVBassPedalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    if (totalNumInputChannels == 1 && totalNumOutputChannels == 2)
    {
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }
    else if (totalNumInputChannels >= 2 && totalNumOutputChannels >= 2 && numSamples > 0)
    {
        // Auto-detección inteligente para placas USB de 2 entradas (ej. bajo en Input 1 o Input 2 en iPad / Mac / PC)
        const float pk0 = buffer.getMagnitude (0, 0, numSamples);
        const float pk1 = buffer.getMagnitude (1, 0, numSamples);
        if (pk0 < 0.00025f && pk1 > 0.0012f)
            buffer.copyFrom (0, 0, buffer, 1, 0, numSamples);
        else if (pk1 < 0.00025f && pk0 > 0.0012f)
            buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    }
    else
    {
        for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, numSamples);
    }

    if (totalNumInputChannels > 0 && numSamples > 0)
        bassTuner.pushSamples (buffer.getReadPointer (0), numSamples);

    dspEngine.process (buffer, buildCurrentSnapshot());
}

juce::AudioProcessorEditor* XVBassPedalAudioProcessor::createEditor()
{
    return new XVBassPedalAudioProcessorEditor (*this);
}

void XVBassPedalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentProgramIndex, nullptr);
    state.setProperty ("tunerA4", bassTuner.getReferenceA4(), nullptr);
    if (std::unique_ptr<juce::XmlElement> xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void XVBassPedalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (std::unique_ptr<juce::XmlElement> xmlState = getXmlFromBinary (data, sizeInBytes))
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            reloadUserPresets();
            auto vt = juce::ValueTree::fromXml (*xmlState);
            currentProgramIndex = juce::jlimit (0, juce::jmax (0, getNumPrograms() - 1),
                                                static_cast<int> (vt.getProperty ("presetIndex", 0)));
            bassTuner.setReferenceA4 (static_cast<float> (vt.getProperty ("tunerA4", 440.0f)));
            apvts.replaceState (vt);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XVBassPedalAudioProcessor();
}
