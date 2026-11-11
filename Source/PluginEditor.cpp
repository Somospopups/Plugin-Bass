#include "PluginProcessor.h"
#include "PluginEditor.h"

XVBassPedalAudioProcessorEditor::XVBassPedalAudioProcessorEditor (XVBassPedalAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    plateActive = juce::ImageCache::getFromMemory (BinaryData::pedal_base_active_png,
                                                   BinaryData::pedal_base_active_pngSize);
    plateBypass = juce::ImageCache::getFromMemory (BinaryData::pedal_base_bypass_png,
                                                   BinaryData::pedal_base_bypass_pngSize);
    knobPointerStrip = juce::ImageCache::getFromMemory (BinaryData::knob_pointer_filmstrip_128_png,
                                                        BinaryData::knob_pointer_filmstrip_128_pngSize);

    auto cab0 = juce::ImageCache::getFromMemory (BinaryData::cab_switch_0_bass_png,
                                                 BinaryData::cab_switch_0_bass_pngSize);
    auto cab1 = juce::ImageCache::getFromMemory (BinaryData::cab_switch_1_punk_png,
                                                 BinaryData::cab_switch_1_punk_pngSize);
    auto cab2 = juce::ImageCache::getFromMemory (BinaryData::cab_switch_2_clean_png,
                                                 BinaryData::cab_switch_2_clean_pngSize);

    auto fsUp   = juce::ImageCache::getFromMemory (BinaryData::footswitch_0_up_png,
                                                   BinaryData::footswitch_0_up_pngSize);
    auto fsDown = juce::ImageCache::getFromMemory (BinaryData::footswitch_1_down_png,
                                                   BinaryData::footswitch_1_down_pngSize);

    auto hudCb = [this] (const juce::String& msg) { showHudMessage (msg); };

    struct KnobSpec
    {
        const char* id;
        const char* label;
        float x, y, w, h;
        float defVal;
    };

    const std::array<KnobSpec, 7> specs {{
        { ParamIDs::level,    "LEVEL",     171.0f, 114.0f, 200.0f, 200.0f,  5.0f  },
        { ParamIDs::drive,    "DRIVE",     390.0f, 114.0f, 200.0f, 200.0f,  5.0f  },
        { ParamIDs::low,      "LOW",       605.0f, 114.0f, 200.0f, 200.0f,  0.0f  },
        { ParamIDs::high,     "HIGH",      820.0f, 114.0f, 200.0f, 200.0f,  0.0f  },
        { ParamIDs::blend,    "BLEND",    1036.0f, 114.0f, 200.0f, 200.0f, 50.0f  },
        { ParamIDs::mid,      "MID",       504.0f, 351.0f, 200.0f, 200.0f,  0.0f  },
        { ParamIDs::presence, "PRESENCE",  728.0f, 351.0f, 200.0f, 200.0f,  0.0f  },
    }};

    knobs.reserve (specs.size());
    for (const auto& s : specs)
    {
        KnobEntry entry;
        entry.paramId      = s.id;
        entry.refBounds    = { s.x, s.y, s.w, s.h };
        entry.defaultValue = s.defVal;
        entry.slider       = std::make_unique<XVFilmstripKnob> (knobPointerStrip, s.label, hudCb);
        addAndMakeVisible (*entry.slider);

        entry.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.apvts, s.id, *entry.slider);
        entry.slider->setDoubleClickReturnValue (true, static_cast<double> (s.defVal));

        knobs.push_back (std::move (entry));
    }

    if (auto* cabParam = audioProcessor.apvts.getParameter (ParamIDs::cab))
    {
        cabToggle = std::make_unique<XVCabToggle> (
            *cabParam,
            std::array<juce::Image, 3> { cab0, cab1, cab2 },
            juce::Rectangle<float> (0.0f, 0.0f, 180.0f / 275.0f, 1.0f),
            hudCb);
        addAndMakeVisible (*cabToggle);
    }

    if (auto* bypParam = audioProcessor.apvts.getParameter (ParamIDs::bypass))
    {
        footswitch = std::make_unique<XVFootswitchButton> (
            *bypParam,
            fsUp,
            fsDown,
            [this] (bool bypassedNow)
            {
                pedalBypassed = bypassedNow;
                repaint();
            },
            hudCb);
        addAndMakeVisible (*footswitch);
    }

    tunerCard = std::make_unique<XVAppleTunerCard> (audioProcessor, [this] { toggleTunerCard(); });
    addChildComponent (*tunerCard);
    if (audioProcessor.isTunerActive())
        tunerCard->setVisible (true);

    pitchCard = std::make_unique<XVApplePitchCard> (
        audioProcessor,
        [this] (const juce::String& msg) { showHudMessage (msg); repaint(); },
        [this] { pitchCard->setVisible (false); repaint(); });
    addChildComponent (*pitchCard);

    savePresetCard = std::make_unique<XVAppleSavePresetCard> (
        audioProcessor,
        [this] (const juce::String& msg) { showHudMessage (msg); repaint(); },
        [this] { savePresetCard->setVisible (false); repaint(); });
    addChildComponent (*savePresetCard);

    exportCard = std::make_unique<XVAppleExportCard> (
        audioProcessor,
        plateActive,
        knobPointerStrip,
        std::array<juce::Image, 3> { cab0, cab1, cab2 },
        fsUp,
        [this] (const juce::String& msg) { showHudMessage (msg); repaint(); },
        [this] { exportCard->setVisible (false); repaint(); });
    addChildComponent (*exportCard);

    const double aspect = static_cast<double> (kRefWidth) / static_cast<double> (kRefHeight);
#if JUCE_IOS
    setResizable (true, false);
    if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = disp->userArea;
        setSize (juce::jmax (320, area.getWidth()), juce::jmax (320, area.getHeight()));
    }
    else
    {
        setSize (1024, 740);
    }
#else
    setResizable (true, true);
    setResizeLimits (520, juce::roundToInt (520.0 / aspect), 1347, 1165);
    getConstrainer()->setFixedAspectRatio (aspect);

    // Tamaño inicial adaptativo según la pantalla (ideal tanto en notebooks 1366x768 como en monitores grandes)
    int initH = 660;
    if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initH = juce::jlimit (530, 720, juce::roundToInt (static_cast<float> (disp->userArea.getHeight()) * 0.75f));
    const int initW = juce::roundToInt (static_cast<double> (initH) * aspect);
    setSize (initW, initH);
#endif

    startTimerHz (45);

    // Sincronizar silenciosamente en segundo plano con GitHub (Somospopups/Plugin-Bass)
    audioProcessor.triggerCloudSyncAsync (false,
        [safeThis = juce::Component::SafePointer<XVBassPedalAudioProcessorEditor> (this)] (const juce::String& msg)
        {
            if (safeThis != nullptr)
            {
                if (msg.isNotEmpty())
                    safeThis->showHudMessage (msg, 95);
                safeThis->repaint();
            }
        });
}

XVBassPedalAudioProcessorEditor::~XVBassPedalAudioProcessorEditor()
{
    stopTimer();
}

void XVBassPedalAudioProcessorEditor::closeAllFloatingCardsExcept (juce::Component* keepOpen)
{
    if (tunerCard != nullptr && tunerCard.get() != keepOpen && tunerCard->isVisible())
    {
        tunerCard->setVisible (false);
        audioProcessor.setTunerActive (false);
        if (audioProcessor.isTunerMuted())
            audioProcessor.setTunerMuted (false);
    }
    if (pitchCard != nullptr && pitchCard.get() != keepOpen && pitchCard->isVisible())
        pitchCard->setVisible (false);
    if (savePresetCard != nullptr && savePresetCard.get() != keepOpen && savePresetCard->isVisible())
        savePresetCard->setVisible (false);
    if (exportCard != nullptr && exportCard.get() != keepOpen && exportCard->isVisible())
        exportCard->setVisible (false);
}

void XVBassPedalAudioProcessorEditor::toggleTunerCard()
{
    closeAllFloatingCardsExcept (tunerCard.get());
    const bool nextOpen = ! tunerCard->isVisible();
    tunerCard->setVisible (nextOpen);
    audioProcessor.setTunerActive (nextOpen);
    if (! nextOpen && audioProcessor.isTunerMuted())
        audioProcessor.setTunerMuted (false);
    repaint();
}

void XVBassPedalAudioProcessorEditor::togglePitchCard()
{
    closeAllFloatingCardsExcept (pitchCard.get());
    pitchCard->setVisible (! pitchCard->isVisible());
    repaint();
}

void XVBassPedalAudioProcessorEditor::toggleSavePresetCard()
{
    closeAllFloatingCardsExcept (savePresetCard.get());
    if (savePresetCard->isVisible())
        savePresetCard->setVisible (false);
    else
        savePresetCard->prepareToOpen();
    repaint();
}

void XVBassPedalAudioProcessorEditor::toggleExportCard()
{
    closeAllFloatingCardsExcept (exportCard.get());
    if (exportCard->isVisible())
        exportCard->setVisible (false);
    else
        exportCard->prepareToOpen();
    repaint();
}

void XVBassPedalAudioProcessorEditor::openApplePresetMenu()
{
    closeAllFloatingCardsExcept (nullptr);
    audioProcessor.reloadUserPresets();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&appleMenuLnF);

    const auto& factory  = audioProcessor.getFactoryPresets();
    const int numFactory = static_cast<int> (factory.size());
    const auto& userList = audioProcessor.getUserPresets();
    const int currentIdx = audioProcessor.getCurrentProgram();
    const auto defName   = audioProcessor.getDefaultStartupPresetName();

    constexpr int kActionOpenSaveCard   = 9001;
    constexpr int kActionMakeCurrentDef = 9002;
    constexpr int kActionClearDef       = 9003;
    constexpr int kActionOpenExportCard = 9004;
    constexpr int kActionSyncCloudNow   = 9005;
    constexpr int kActionRunAutoUpdate  = 9006;

    if (audioProcessor.hasPluginUpdateAvailable())
    {
        menu.addItem (kActionRunAutoUpdate,
                      "[STAR] Actualizar XV Bass Pedal a v" + audioProcessor.getRemotePluginVersion() + "...");
        menu.addSeparator();
    }

    if (! userList.empty())
    {
        menu.addSectionHeader ("Mis Presets Guardados");
        for (int u = 0; u < static_cast<int> (userList.size()); ++u)
        {
            const int progIdx = numFactory + u;
            juce::String label = userList[static_cast<size_t> (u)].name;
            if (defName.isNotEmpty() && label == defName)
                label = "[STAR] " + label;
            menu.addItem (progIdx + 1, label, true, progIdx == currentIdx);
        }
        menu.addSeparator();
    }

    menu.addSectionHeader ("XV & Canciones");
    for (int i = 0; i < numFactory; ++i)
    {
        juce::String label = factory[static_cast<size_t> (i)].name;
        if (defName.isNotEmpty() && label == defName)
            label = "[STAR] " + label;
        menu.addItem (i + 1, label, true, i == currentIdx);
        if (i == 0)
            menu.addSeparator();
    }

    menu.addSeparator();
    menu.addItem (kActionOpenSaveCard,   "[PLUS] Guardar ajustes actuales como preset...");
    menu.addItem (kActionMakeCurrentDef, "[STAR] Fijar sonido actual como Predeterminado");
    menu.addItem (kActionOpenExportCard, "[PLUS] Exportar sonido a M-VAVE Blackbox...");
    menu.addItem (kActionSyncCloudNow,   "[PLUS] Sincronizar presets con la nube ahora");
    if (defName.isNotEmpty())
        menu.addItem (kActionClearDef, "Restaurar XV predeterminado de fabrica");

    const auto fullCapsule = prevArrowRect.getUnion (presetPillRect).getUnion (nextArrowRect);
    auto options = juce::PopupMenu::Options()
                       .withTargetScreenArea (localAreaToGlobal (fullCapsule.toNearestInt()))
                       .withMinimumWidth (juce::jmax (268, juce::roundToInt (fullCapsule.getWidth())));

    menu.showMenuAsync (options, [safeThis = juce::Component::SafePointer<XVBassPedalAudioProcessorEditor> (this)] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        if (result == kActionOpenSaveCard)   { safeThis->toggleSavePresetCard(); return; }
        if (result == kActionOpenExportCard) { safeThis->toggleExportCard(); return; }
        if (result == kActionSyncCloudNow)
        {
            safeThis->showHudMessage ("Buscando actualizaciones en la nube...", 65);
            safeThis->audioProcessor.triggerCloudSyncAsync (true,
                [safeThis] (const juce::String& msg)
                {
                    if (safeThis != nullptr)
                    {
                        safeThis->showHudMessage (msg, 95);
                        safeThis->repaint();
                    }
                });
            return;
        }
        if (result == kActionRunAutoUpdate)
        {
            safeThis->audioProcessor.launchPluginUpdateAsync (
                [safeThis] (const juce::String& msg)
                {
                    if (safeThis != nullptr)
                    {
                        safeThis->showHudMessage (msg, 110);
                        safeThis->repaint();
                    }
                });
            return;
        }
        if (result == kActionMakeCurrentDef)
        {
            safeThis->audioProcessor.saveCurrentSettingsAsDefaultStartup();
            safeThis->showHudMessage ("Sonido fijado como Predeterminado al abrir", 75);
            safeThis->repaint();
            return;
        }
        if (result == kActionClearDef)
        {
            safeThis->audioProcessor.clearDefaultStartupPreset();
            safeThis->showHudMessage ("Seteo XV de fabrica restaurado", 75);
            safeThis->repaint();
            return;
        }

        const int idx = result - 1;
        safeThis->audioProcessor.setCurrentProgram (idx);
        safeThis->audioProcessor.updateHostDisplay();
        safeThis->showHudMessage (safeThis->audioProcessor.getProgramSelectionHud (idx), 95);
        safeThis->repaint();
    });
}

void XVBassPedalAudioProcessorEditor::showHudMessage (const juce::String& text, int holdFrames)
{
    hudText       = text;
    hudAlpha      = 1.0f;
    hudHoldFrames = holdFrames;
    repaint();
}

void XVBassPedalAudioProcessorEditor::timerCallback()
{
    bool needsRepaint = false;

    if (tunerCard != nullptr && tunerCard->isVisible())
    {
        latestTunerReading = audioProcessor.getTuner().analyze();
        tunerCard->updateReading (latestTunerReading);
        needsRepaint = true;
    }

    if (hudHoldFrames > 0)
    {
        --hudHoldFrames;
    }
    else if (hudAlpha > 0.01f)
    {
        hudAlpha *= 0.85f;
        if (hudAlpha <= 0.02f)
            hudAlpha = 0.0f;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}

void XVBassPedalAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const bool hTuner  = tunerPillRect.contains (pt);
    const bool hPitch  = pitchPillRect.contains (pt);
    const bool hPrev   = prevArrowRect.contains (pt);
    const bool hPreset = presetPillRect.contains (pt);
    const bool hNext   = nextArrowRect.contains (pt);
    const bool hSave   = savePillRect.contains (pt);
    const bool hExport = exportPillRect.contains (pt);
    const bool hUpdate = audioProcessor.hasPluginUpdateAvailable() && updateBadgeRect.contains (pt);

    if (hTuner != hoverTunerPill || hPitch != hoverPitchPill || hPrev != hoverPrevArrow
        || hPreset != hoverPresetPill || hNext != hoverNextArrow
        || hSave != hoverSavePill || hExport != hoverExportPill || hUpdate != hoverUpdateBadge)
    {
        hoverTunerPill   = hTuner;
        hoverPitchPill   = hPitch;
        hoverPrevArrow   = hPrev;
        hoverPresetPill  = hPreset;
        hoverNextArrow   = hNext;
        hoverSavePill    = hSave;
        hoverExportPill  = hExport;
        hoverUpdateBadge = hUpdate;

        const bool anyPill = (hTuner || hPitch || hPrev || hPreset || hNext || hSave || hExport || hUpdate);
        setMouseCursor (anyPill ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void XVBassPedalAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.position;

    if (audioProcessor.hasPluginUpdateAvailable() && updateBadgeRect.contains (pt))
    {
        audioProcessor.launchPluginUpdateAsync (
            [safeThis = juce::Component::SafePointer<XVBassPedalAudioProcessorEditor> (this)] (const juce::String& msg)
            {
                if (safeThis != nullptr)
                {
                    safeThis->showHudMessage (msg, 110);
                    safeThis->repaint();
                }
            });
        return;
    }

    if (tunerPillRect.expanded (4.0f).contains (pt))  { toggleTunerCard();      return; }
    if (pitchPillRect.expanded (4.0f).contains (pt))  { togglePitchCard();      return; }
    if (savePillRect.expanded (4.0f).contains (pt))   { toggleSavePresetCard(); return; }
    if (exportPillRect.expanded (4.0f).contains (pt)) { toggleExportCard();     return; }

    if (prevArrowRect.expanded (4.0f).contains (pt))
    {
        const int n   = juce::jmax (1, audioProcessor.getNumPrograms());
        const int idx = (audioProcessor.getCurrentProgram() - 1 + n) % n;
        audioProcessor.setCurrentProgram (idx);
        audioProcessor.updateHostDisplay();
        showHudMessage (audioProcessor.getProgramSelectionHud (idx), 95);
        repaint();
        return;
    }

    if (nextArrowRect.contains (pt))
    {
        const int n   = juce::jmax (1, audioProcessor.getNumPrograms());
        const int idx = (audioProcessor.getCurrentProgram() + 1) % n;
        audioProcessor.setCurrentProgram (idx);
        audioProcessor.updateHostDisplay();
        showHudMessage (audioProcessor.getProgramSelectionHud (idx), 95);
        repaint();
        return;
    }

    if (presetPillRect.contains (pt))
    {
        openApplePresetMenu();
        return;
    }

    // Cerrar cualquier tarjeta abierta al hacer clic fuera
    if (tunerCard != nullptr && tunerCard->isVisible() && ! tunerCard->getBounds().toFloat().contains (pt))
        toggleTunerCard();
    if (pitchCard != nullptr && pitchCard->isVisible() && ! pitchCard->getBounds().toFloat().contains (pt))
    {
        pitchCard->setVisible (false);
        repaint();
    }
    if (savePresetCard != nullptr && savePresetCard->isVisible() && ! savePresetCard->getBounds().toFloat().contains (pt))
    {
        savePresetCard->setVisible (false);
        repaint();
    }
    if (exportCard != nullptr && exportCard->isVisible() && ! exportCard->getBounds().toFloat().contains (pt))
    {
        exportCard->setVisible (false);
        repaint();
    }
}

void XVBassPedalAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float s  = juce::jmin (bounds.getWidth() / kRefWidth, bounds.getHeight() / kRefHeight);
    const float ox = (bounds.getWidth()  - kRefWidth  * s) * 0.5f;
    const float oy = (bounds.getHeight() - kRefHeight * s) * 0.5f;
    const float sx = s;
    const float sy = s;

    // Fondo estudio sutil radial
    juce::ColourGradient bgGrad (juce::Colour (0xff191a20), bounds.getCentreX(), bounds.getHeight() * 0.44f,
                                 juce::Colour (0xff09090c), 0.0f, bounds.getBottom(), true);
    g.setGradientFill (bgGrad);
    g.fillRect (bounds);

    // Sombra realista del chasis desplazada con kPedalOffset (centrada sin deformar en iPad horizontal/vertical/AUv3)
    juce::DropShadow chassisShadow (juce::Colours::black.withAlpha (0.80f),
                                    juce::jmax (12, juce::roundToInt (28.0f * s)),
                                    { 0, juce::roundToInt (12.0f * s) });
    chassisShadow.drawForRectangle (g, juce::Rectangle<int> (
        juce::roundToInt (ox + (108.0f + kPedalOffsetX) * s),
        juce::roundToInt (oy + (56.0f  + kPedalOffsetY) * s),
        juce::roundToInt (1172.0f * s),
        juce::roundToInt (1022.0f * s)));

    // Placa fotorrealista del pedal con proporción exacta 1:1 en cualquier pantalla
    const auto& currentPlate = pedalBypassed ? plateBypass : plateActive;
    if (currentPlate.isValid())
    {
        const juce::Rectangle<float> plateDest (
            ox + kPedalOffsetX * s,
            oy + kPedalOffsetY * s,
            kPlateWidth        * s,
            kPlateHeight       * s);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (currentPlate, plateDest, juce::RectanglePlacement::stretchToFit);
    }

    // --- Barra Superior Simétrica de 5 Píldoras estilo macOS Sonoma / Dynamic Island ---
    const bool tunerOpen  = (tunerCard != nullptr && tunerCard->isVisible());
    const bool pitchOpen  = (pitchCard != nullptr && pitchCard->isVisible());
    const bool saveOpen   = (savePresetCard != nullptr && savePresetCard->isVisible());
    const bool exportOpen = (exportCard != nullptr && exportCard->isVisible());
    const bool mutedNow   = audioProcessor.isTunerMuted();
    const bool pitchOn    = audioProcessor.isPitchEnabled();
    const int  pitchSemi  = audioProcessor.getPitchSemitones();

    const float pillRadius = tunerPillRect.getHeight() * 0.5f;
    const float pillFontH  = juce::jlimit (10.0f, 15.2f, tunerPillRect.getHeight() * 0.50f);
    const auto  boldFont   = makeXvFont (pillFontH, juce::Font::bold);

    // 1. Píldora 1: "• Afinador"
    g.setColour (tunerOpen ? juce::Colour (0xf225252b)
                           : juce::Colour (hoverTunerPill ? 0xe825252b : 0xdc18181d));
    g.fillRoundedRectangle (tunerPillRect, pillRadius);
    g.setColour (tunerOpen ? juce::Colour (0xff30d158).withAlpha (0.72f)
                           : juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (tunerPillRect, pillRadius, 1.0f);

    juce::String tunerBtnText = "Afinador";
    if (tunerOpen && latestTunerReading.hasSignal)
        tunerBtnText = juce::String (latestTunerReading.noteName) + juce::String (latestTunerReading.octave);

    g.setFont (boldFont);
    const float dotR   = juce::jmax (2.2f, 4.4f * sy);
    const float dotGap = 9.5f * sx;
    const float tTxtW  = measureXvStringWidth (boldFont, tunerBtnText);
    const float tUnitW = dotR * 2.0f + dotGap + tTxtW;
    const float tStart = tunerPillRect.getCentreX() - tUnitW * 0.5f;

    const juce::Colour dotCol = mutedNow ? juce::Colour (0xffff9f0a)
                                         : (tunerOpen ? juce::Colour (0xff30d158) : juce::Colour (0xffa1a1aa));
    g.setColour (dotCol);
    g.fillEllipse (tStart, tunerPillRect.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);
    g.setColour (juce::Colour (0xfff5f5f7));
    g.drawText (tunerBtnText,
                juce::Rectangle<float> (tStart + dotR * 2.0f + dotGap - 2.0f, tunerPillRect.getY(),
                                        tTxtW + 8.0f, tunerPillRect.getHeight()),
                juce::Justification::centredLeft, false);

    // 2. Píldora 2: "Pitch" (muestra p.ej. "Pitch -2" en índigo cuando está activo)
    g.setColour ((pitchOpen || pitchOn) ? juce::Colour (0xee24233b)
                                        : juce::Colour (hoverPitchPill ? 0xe8252530 : 0xdc18181d));
    g.fillRoundedRectangle (pitchPillRect, pillRadius);
    g.setColour ((pitchOpen || pitchOn) ? juce::Colour (0xff818cf8).withAlpha (0.78f)
                                        : juce::Colour (0xff818cf8).withAlpha (0.36f));
    g.drawRoundedRectangle (pitchPillRect, pillRadius, 1.0f);

    juce::String pitchLbl = "Pitch";
    if (pitchOn)
    {
        if (pitchSemi != 0)
            pitchLbl = "Pitch " + juce::String (pitchSemi > 0 ? "+" : "") + juce::String (pitchSemi);
        else
            pitchLbl = "Pitch On";
    }
    const juce::Colour pitchFg = (pitchOpen || pitchOn) ? juce::Colour (0xffc7d2fe) : juce::Colour (0xffa5b4fc);
    const float pIconH = juce::jmax (3.0f, 5.6f * sy);
    const float pGap   = 8.5f * sx;
    const float pTxtW  = measureXvStringWidth (boldFont, pitchLbl);
    const float pUnitW = pIconH * 1.4f + pGap + pTxtW;
    const float pStart = pitchPillRect.getCentreX() - pUnitW * 0.5f;

    XVVectorIcons::drawPitchArrows (g, { pStart + pIconH * 0.7f, pitchPillRect.getCentreY() },
                                    pIconH, juce::jmax (1.3f, 2.0f * sy), pitchFg);
    g.setColour (pitchFg);
    g.drawText (pitchLbl,
                juce::Rectangle<float> (pStart + pIconH * 1.4f + pGap - 1.0f, pitchPillRect.getY(),
                                        pTxtW + 8.0f, pitchPillRect.getHeight()),
                juce::Justification::centredLeft, false);

    // 3. Cápsula central de Presets: "< | ★ Nombre Preset ▾ | >"
    const auto fullPresetCapsule = prevArrowRect.getUnion (presetPillRect).getUnion (nextArrowRect);
    const bool anyPresetHover = (hoverPrevArrow || hoverPresetPill || hoverNextArrow);
    g.setColour (juce::Colour (anyPresetHover ? 0xe825252b : 0xdc18181d));
    g.fillRoundedRectangle (fullPresetCapsule, pillRadius);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (fullPresetCapsule, pillRadius, 1.0f);

    const float sepMargin = fullPresetCapsule.getHeight() * 0.24f;
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawVerticalLine (juce::roundToInt (prevArrowRect.getRight()), fullPresetCapsule.getY() + sepMargin, fullPresetCapsule.getBottom() - sepMargin);
    g.drawVerticalLine (juce::roundToInt (nextArrowRect.getX()),     fullPresetCapsule.getY() + sepMargin, fullPresetCapsule.getBottom() - sepMargin);

    const float chevSize   = juce::jmax (3.1f, 5.9f * sy);
    const float chevStroke = juce::jmax (1.3f, 2.1f * sy);
    XVVectorIcons::drawChevronLeft  (g, prevArrowRect.getCentre(), chevSize, chevStroke,
                                     hoverPrevArrow ? juce::Colours::white : juce::Colour (0xffa8a8b3));
    XVVectorIcons::drawChevronRight (g, nextArrowRect.getCentre(), chevSize, chevStroke,
                                     hoverNextArrow ? juce::Colours::white : juce::Colour (0xffa8a8b3));

    juce::String rawPresetName = audioProcessor.getProgramName (audioProcessor.getCurrentProgram());
    if (rawPresetName.isEmpty())
        rawPresetName = "XV";
    const auto defName  = audioProcessor.getDefaultStartupPresetName();
    const bool isDefNow = (defName.isNotEmpty() && rawPresetName == defName);

    const auto presetFont = makeXvFont (pillFontH, juce::Font::plain);
    g.setFont (presetFont);

    const float starR     = juce::jmax (3.4f, 6.5f * sy);
    const float starSpace = isDefNow ? (starR * 2.0f + 8.5f * sx) : 0.0f;
    const float downW     = juce::jmax (2.5f, 4.8f * sy);
    const float downSpace = downW * 2.0f + 9.0f * sx;
    const float maxTextW  = presetPillRect.getWidth() - starSpace - downSpace - 16.0f * sx;
    const float measTextW = juce::jmin (maxTextW, measureXvStringWidth (presetFont, rawPresetName) + 4.0f);

    const float unitW  = starSpace + measTextW + downSpace;
    float cursorX      = presetPillRect.getCentreX() - unitW * 0.5f;
    const float midY   = presetPillRect.getCentreY();

    if (isDefNow)
    {
        XVVectorIcons::drawStar (g, { cursorX + starR, midY - 0.3f * sy }, starR, juce::Colour (0xffffd60a));
        cursorX += starSpace;
    }

    g.setColour (juce::Colour (0xfff5f5f7));
    g.drawText (rawPresetName,
                juce::Rectangle<float> (cursorX, presetPillRect.getY(), measTextW, presetPillRect.getHeight()),
                juce::Justification::centred, true);
    cursorX += measTextW + 8.0f * sx;

    XVVectorIcons::drawChevronDown (g, { cursorX + downW, midY + 0.5f * sy }, downW,
                                    juce::jmax (1.2f, 1.9f * sy), juce::Colour (0xff98989f));

    // 4. Píldora 4: "+ Guardar"
    g.setColour (saveOpen ? juce::Colour (0xff0a84ff)
                          : juce::Colour (hoverSavePill ? 0xe8242834 : 0xdc18181d));
    g.fillRoundedRectangle (savePillRect, pillRadius);
    g.setColour (saveOpen ? juce::Colours::white.withAlpha (0.42f)
                          : juce::Colour (0xff0a84ff).withAlpha (0.58f));
    g.drawRoundedRectangle (savePillRect, pillRadius, 1.0f);

    const juce::Colour saveFg = saveOpen ? juce::Colours::white : juce::Colour (0xff64b5ff);
    g.setFont (boldFont);
    const juce::String saveLbl = "Guardar";
    const float plusHalf = juce::jmax (2.9f, 5.4f * sy);
    const float plusGap  = 8.5f * sx;
    const float sTxtW    = measureXvStringWidth (boldFont, saveLbl);
    const float sUnitW   = plusHalf * 2.0f + plusGap + sTxtW;
    const float sStart   = savePillRect.getCentreX() - sUnitW * 0.5f;

    XVVectorIcons::drawPlus (g, { sStart + plusHalf, savePillRect.getCentreY() },
                             plusHalf, juce::jmax (1.3f, 2.2f * sy), saveFg);
    g.setColour (saveFg);
    g.drawText (saveLbl,
                juce::Rectangle<float> (sStart + plusHalf * 2.0f + plusGap - 1.0f, savePillRect.getY(),
                                        sTxtW + 8.0f, savePillRect.getHeight()),
                juce::Justification::centredLeft, false);

    // 5. Píldora 5: "Exportar" (M-VAVE Blackbox / NAM / IRs + Mini-Imagen 240x240)
    g.setColour (exportOpen ? juce::Colour (0xff248a3d)
                            : juce::Colour (hoverExportPill ? 0xe81f2c24 : 0xdc18181d));
    g.fillRoundedRectangle (exportPillRect, pillRadius);
    g.setColour (exportOpen ? juce::Colours::white.withAlpha (0.42f)
                            : juce::Colour (0xff30d158).withAlpha (0.55f));
    g.drawRoundedRectangle (exportPillRect, pillRadius, 1.0f);

    const juce::Colour expFg = exportOpen ? juce::Colours::white : juce::Colour (0xff6ee7b7);
    const juce::String expLbl = "Exportar";
    const float expSz    = juce::jmax (3.0f, 5.6f * sy);
    const float expGap   = 8.5f * sx;
    const float eTxtW    = measureXvStringWidth (boldFont, expLbl);
    const float eUnitW   = expSz * 1.8f + expGap + eTxtW;
    const float eStart   = exportPillRect.getCentreX() - eUnitW * 0.5f;

    XVVectorIcons::drawExportShare (g, { eStart + expSz * 0.9f, exportPillRect.getCentreY() },
                                    expSz, juce::jmax (1.2f, 1.9f * sy), expFg);
    g.setColour (expFg);
    g.drawText (expLbl,
                juce::Rectangle<float> (eStart + expSz * 1.8f + expGap - 1.0f, exportPillRect.getY(),
                                        eTxtW + 8.0f, exportPillRect.getHeight()),
                juce::Justification::centredLeft, false);

    // --- Pastilla flotante discreta de Auto-Actualizacion (solo visible si hay nueva version en GitHub) ---
    if (audioProcessor.hasPluginUpdateAvailable())
    {
        const bool busy = audioProcessor.isDownloadingUpdate();
        const float uRad = updateBadgeRect.getHeight() * 0.5f;
        g.setColour (busy ? juce::Colour (0xee1e3a2f)
                          : juce::Colour (hoverUpdateBadge ? 0xf5289e47 : 0xec1f7a37));
        g.fillRoundedRectangle (updateBadgeRect, uRad);
        g.setColour (juce::Colours::white.withAlpha (hoverUpdateBadge ? 0.55f : 0.30f));
        g.drawRoundedRectangle (updateBadgeRect, uRad, 1.1f);

        g.setColour (juce::Colours::white);
        g.setFont (makeXvFont (juce::jlimit (9.5f, 13.0f, updateBadgeRect.getHeight() * 0.46f), juce::Font::bold));
        const juce::String uTxt = busy ? "Actualizando..."
                                       : ("Actualizar v" + audioProcessor.getRemotePluginVersion());
        g.drawText (uTxt, updateBadgeRect, juce::Justification::centred, true);
    }

    // --- Cápsula HUD efímera estilo iOS abajo al mover perillas ---
    if (hudAlpha > 0.01f && hudText.isNotEmpty())
    {
        juce::Font hudFont = makeXvFont (juce::jlimit (10.5f, 14.5f, pillFontH), juce::Font::bold);
        g.setFont (hudFont);
        const float textW = juce::jmin (bounds.getWidth() - 28.0f * sx, measureXvStringWidth (hudFont, hudText) + 32.0f * sx);
        const float hudH  = juce::jmax (21.0f, 40.0f * sy);
        const juce::Rectangle<float> pillRect (
            (bounds.getWidth() - textW) * 0.5f,
            bounds.getHeight() - hudH - 14.0f * sy,
            textW, hudH);

        g.setColour (juce::Colour (0xee18181c).withMultipliedAlpha (hudAlpha));
        g.fillRoundedRectangle (pillRect, hudH * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.18f * hudAlpha));
        g.drawRoundedRectangle (pillRect, hudH * 0.5f, 1.0f);
        g.setColour (juce::Colour (0xfff5f5f7).withAlpha (0.96f * hudAlpha));
        g.drawText (hudText, pillRect, juce::Justification::centred, false);
    }
}

void XVBassPedalAudioProcessorEditor::resized()
{
    const float w  = static_cast<float> (getWidth());
    const float h  = static_cast<float> (getHeight());
    const float s  = juce::jmin (w / kRefWidth, h / kRefHeight);
    const float ox = (w - kRefWidth  * s) * 0.5f;
    const float oy = (h - kRefHeight * s) * 0.5f;

    auto mapPedalRect = [s, ox, oy] (float x, float y, float rw, float rh) -> juce::Rectangle<int>
    {
        return {
            juce::roundToInt (ox + (x + kPedalOffsetX) * s),
            juce::roundToInt (oy + (y + kPedalOffsetY) * s),
            juce::roundToInt (rw * s),
            juce::roundToInt (rh * s)
        };
    };

    for (auto& k : knobs)
        if (k.slider)
            k.slider->setBounds (mapPedalRect (k.refBounds.getX(), k.refBounds.getY(),
                                               k.refBounds.getWidth(), k.refBounds.getHeight()));

    if (cabToggle)
        cabToggle->setBounds (mapPedalRect (990.0f, 371.0f, 275.0f, 180.0f));

    if (footswitch)
        footswitch->setBounds (mapPedalRect (166.0f, 734.0f, 220.0f, 220.0f));

    // 5 Píldoras flotantes simétricas centradas sobre el eje del chasis (x_plate = 693)
    const float cxPedal = ox + (693.0f + kPedalOffsetX) * s;
    const float pillY   = oy + 20.0f * s;
    const float pillH   = 45.0f * s;

    const float wTuner  = 152.0f * s;
    const float wPitch  = 140.0f * s;
    const float wMid    = 394.0f * s;
    const float arrowW  =  39.0f * s;
    const float wSave   = 146.0f * s;
    const float wExport = 152.0f * s;
    const float gapIn   =  11.0f * s;
    const float gapMid  =  14.0f * s;

    const float totalW  = wTuner + gapIn + wPitch + gapMid + wMid + gapMid + wSave + gapIn + wExport;
    const float startX  = cxPedal - totalW * 0.5f;

    tunerPillRect  = { startX, pillY, wTuner, pillH };
    pitchPillRect  = { tunerPillRect.getRight() + gapIn, pillY, wPitch, pillH };

    const float capX = pitchPillRect.getRight() + gapMid;
    prevArrowRect  = { capX, pillY, arrowW, pillH };
    presetPillRect = { capX + arrowW, pillY, wMid - 2.0f * arrowW, pillH };
    nextArrowRect  = { capX + wMid - arrowW, pillY, arrowW, pillH };

    savePillRect   = { nextArrowRect.getRight() + gapMid, pillY, wSave, pillH };
    exportPillRect = { savePillRect.getRight() + gapIn, pillY, wExport, pillH };
    updateBadgeRect = { w - 224.0f * s - 12.0f,
                        h -  52.0f * s - 10.0f,
                        204.0f * s, 38.0f * s };

    // Tarjetas flotantes desplegadas justo debajo de la barra superior con margen pequeño (12 ref)
    const int cardTopY = juce::roundToInt (oy + 77.0f * s);
    if (tunerCard != nullptr)
    {
        const int tw = juce::roundToInt (510.0f * s);
        const int th = juce::roundToInt (300.0f * s);
        tunerCard->setBounds (juce::roundToInt (cxPedal) - tw / 2, cardTopY, tw, th);
    }

    if (pitchCard != nullptr)
    {
        const int pw = juce::roundToInt (540.0f * s);
        const int ph = juce::roundToInt (332.0f * s);
        pitchCard->setBounds (juce::roundToInt (cxPedal) - pw / 2, cardTopY, pw, ph);
    }

    if (savePresetCard != nullptr)
    {
        const int sw = juce::roundToInt (490.0f * s);
        const int sh = juce::roundToInt (240.0f * s);
        savePresetCard->setBounds (juce::roundToInt (cxPedal) - sw / 2, cardTopY, sw, sh);
    }

    if (exportCard != nullptr)
    {
        const int ew = juce::roundToInt (640.0f * s);
        const int eh = juce::roundToInt (294.0f * s);
        exportCard->setBounds (juce::roundToInt (cxPedal) - ew / 2, cardTopY, ew, eh);
    }
}
