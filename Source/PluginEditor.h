#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "DSP/XVProfileExporter.h"

inline juce::Font makeXvFont (float height, int styleFlags = juce::Font::plain)
{
#if JUCE_MAJOR_VERSION >= 8
    return juce::Font (juce::FontOptions (height, styleFlags));
#else
    return juce::Font (height, styleFlags);
#endif
}

inline float measureXvStringWidth (const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

/**
 * Iconos vectoriales nítidos estilo SF Symbols (100% independientes de fuentes GDI/macOS)
 */
namespace XVVectorIcons
{
    inline void drawStar (juce::Graphics& g, juce::Point<float> c, float outerR, juce::Colour col)
    {
        juce::Path p;
        const float innerR = outerR * 0.42f;
        for (int i = 0; i < 10; ++i)
        {
            const float r = (i % 2 == 0) ? outerR : innerR;
            const float a = static_cast<float> (i) * juce::MathConstants<float>::pi * 0.2f
                          - juce::MathConstants<float>::halfPi;
            const juce::Point<float> pt (c.x + std::cos (a) * r, c.y + std::sin (a) * r);
            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }
        p.closeSubPath();
        g.setColour (col);
        g.fillPath (p);
    }

    inline void drawChevronLeft (juce::Graphics& g, juce::Point<float> c, float halfH, float strokeW, juce::Colour col)
    {
        juce::Path p;
        const float hw = halfH * 0.58f;
        p.startNewSubPath (c.x + hw * 0.5f, c.y - halfH);
        p.lineTo          (c.x - hw * 0.5f, c.y);
        p.lineTo          (c.x + hw * 0.5f, c.y + halfH);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawChevronRight (juce::Graphics& g, juce::Point<float> c, float halfH, float strokeW, juce::Colour col)
    {
        juce::Path p;
        const float hw = halfH * 0.58f;
        p.startNewSubPath (c.x - hw * 0.5f, c.y - halfH);
        p.lineTo          (c.x + hw * 0.5f, c.y);
        p.lineTo          (c.x - hw * 0.5f, c.y + halfH);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawChevronDown (juce::Graphics& g, juce::Point<float> c, float halfW, float strokeW, juce::Colour col)
    {
        juce::Path p;
        const float hh = halfW * 0.58f;
        p.startNewSubPath (c.x - halfW, c.y - hh * 0.5f);
        p.lineTo          (c.x,         c.y + hh * 0.5f);
        p.lineTo          (c.x + halfW, c.y - hh * 0.5f);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawPlus (juce::Graphics& g, juce::Point<float> c, float halfLen, float strokeW, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath (c.x - halfLen, c.y);
        p.lineTo          (c.x + halfLen, c.y);
        p.startNewSubPath (c.x, c.y - halfLen);
        p.lineTo          (c.x, c.y + halfLen);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawCloseX (juce::Graphics& g, juce::Point<float> c, float halfLen, float strokeW, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath (c.x - halfLen, c.y - halfLen);
        p.lineTo          (c.x + halfLen, c.y + halfLen);
        p.startNewSubPath (c.x + halfLen, c.y - halfLen);
        p.lineTo          (c.x - halfLen, c.y + halfLen);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawCheckmark (juce::Graphics& g, juce::Point<float> c, float sz, float strokeW, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath (c.x - sz * 0.55f, c.y + sz * 0.02f);
        p.lineTo          (c.x - sz * 0.15f, c.y + sz * 0.44f);
        p.lineTo          (c.x + sz * 0.60f, c.y - sz * 0.42f);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawPitchArrows (juce::Graphics& g, juce::Point<float> c, float halfSz, float strokeW, juce::Colour col)
    {
        juce::Path p;
        // Flecha arriba-abajo de transposición musical
        const float dx = halfSz * 0.52f;
        p.startNewSubPath (c.x,      c.y - halfSz);
        p.lineTo          (c.x,      c.y + halfSz);
        p.startNewSubPath (c.x - dx, c.y - halfSz * 0.42f);
        p.lineTo          (c.x,      c.y - halfSz);
        p.lineTo          (c.x + dx, c.y - halfSz * 0.42f);
        p.startNewSubPath (c.x - dx, c.y + halfSz * 0.42f);
        p.lineTo          (c.x,      c.y + halfSz);
        p.lineTo          (c.x + dx, c.y + halfSz * 0.42f);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline void drawExportShare (juce::Graphics& g, juce::Point<float> c, float halfSz, float strokeW, juce::Colour col)
    {
        juce::Path p;
        // Icono SF Symbol square.and.arrow.up
        const float bw = halfSz * 0.88f;
        const float topBox = c.y - halfSz * 0.12f;
        const float botBox = c.y + halfSz * 0.92f;
        p.startNewSubPath (c.x - bw * 0.55f, topBox);
        p.lineTo          (c.x - bw,         topBox);
        p.lineTo          (c.x - bw,         botBox);
        p.lineTo          (c.x + bw,         botBox);
        p.lineTo          (c.x + bw,         topBox);
        p.lineTo          (c.x + bw * 0.55f, topBox);

        p.startNewSubPath (c.x, c.y + halfSz * 0.42f);
        p.lineTo          (c.x, c.y - halfSz * 0.95f);
        p.startNewSubPath (c.x - halfSz * 0.50f, c.y - halfSz * 0.45f);
        p.lineTo          (c.x,                  c.y - halfSz * 0.95f);
        p.lineTo          (c.x + halfSz * 0.50f, c.y - halfSz * 0.45f);

        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

/**
 * Custom macOS / iOS Dark Translucent Rounded PopupMenu LookAndFeel (con iconos vectoriales)
 */
class XVApplePopupLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    XVApplePopupLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xf41c1c21));
        setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xfff5f5f7));
        setColour (juce::PopupMenu::headerTextColourId,            juce::Colour (0xff8e8e93));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff0a84ff));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
        g.setColour (juce::Colour (0xf51b1b20));
        g.fillRoundedRectangle (bounds.reduced (1.0f), 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 12.0f, 1.0f);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) override
    {
        g.setColour (juce::Colour (0xff8e8e93));
        g.setFont (makeXvFont (10.8f, juce::Font::bold));
        g.drawText (sectionName.toUpperCase(), area.reduced (14, 0), juce::Justification::centredLeft, true);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                            const juce::String& /*shortcutKeyText*/, const juce::Drawable* /*icon*/,
                            const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.fillRect (area.reduced (12, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        auto r = area.reduced (5, 2).toFloat();
        if (isHighlighted && isActive)
        {
            g.setColour (juce::Colour (0xff0a84ff));
            g.fillRoundedRectangle (r, 7.0f);
        }

        juce::String cleanText = text;
        const bool hasStarTag = cleanText.startsWith ("[STAR] ");
        const bool hasPlusTag = cleanText.startsWith ("[PLUS] ");
        if (hasStarTag) cleanText = cleanText.substring (7);
        if (hasPlusTag) cleanText = cleanText.substring (7);

        auto checkZone = r.removeFromLeft (24.0f);
        if (isTicked)
        {
            const auto checkCol = (isHighlighted && isActive) ? juce::Colours::white : juce::Colour (0xff0a84ff);
            XVVectorIcons::drawCheckmark (g, checkZone.getCentre(), 7.5f, 1.8f, checkCol);
        }

        if (hasStarTag)
        {
            auto starZone = r.removeFromLeft (16.0f);
            const auto starCol = (isHighlighted && isActive) ? juce::Colour (0xffffe66d) : juce::Colour (0xffffd60a);
            XVVectorIcons::drawStar (g, starZone.getCentre(), 5.4f, starCol);
        }
        else if (hasPlusTag)
        {
            auto plusZone = r.removeFromLeft (16.0f);
            const auto plusCol = (isHighlighted && isActive) ? juce::Colours::white : juce::Colour (0xff64b5ff);
            XVVectorIcons::drawPlus (g, plusZone.getCentre(), 4.4f, 1.7f, plusCol);
        }

        const bool isActionBlue = (hasPlusTag || (hasStarTag && cleanText.startsWith ("Fijar")));
        juce::Colour txtCol = ! isActive ? juce::Colours::grey
                            : ((isHighlighted) ? juce::Colours::white
                                               : (isActionBlue ? juce::Colour (0xff64b5ff) : juce::Colour (0xfff5f5f7)));

        g.setColour (txtCol);
        g.setFont (makeXvFont (13.2f, (isTicked || isActionBlue) ? juce::Font::bold : juce::Font::plain));
        g.drawText (cleanText, r.reduced (2.0f, 0.0f), juce::Justification::centredLeft, true);
    }
};

/**
 * Perilla rotatoria fotorrealista basada en el filmstrip PBR de 128 cuadros.
 */
class XVFilmstripKnob final : public juce::Slider
{
public:
    XVFilmstripKnob (const juce::Image& filmstrip128, const juce::String& displayLabel,
                     std::function<void (const juce::String&)> onInteractCallback)
        : strip (filmstrip128), labelName (displayLabel), onInteract (std::move (onInteractCallback))
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setPopupDisplayEnabled (false, false, nullptr);
        setDoubleClickReturnValue (true, 0.0);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;

        constexpr int numFrames = 128;
        const int frameW = strip.getWidth();
        const int frameH = strip.getHeight() / numFrames;

        const double norm = valueToProportionOfLength (getValue());
        const int frameIdx = juce::jlimit (0, numFrames - 1,
                                           juce::roundToInt (norm * static_cast<double> (numFrames - 1)));

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip, 0, 0, getWidth(), getHeight(),
                     0, frameIdx * frameH, frameW, frameH, false);
    }

    void valueChanged() override
    {
        juce::Slider::valueChanged();
        if (isMouseButtonDown())
            notifyHUD();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        setVelocityBasedMode (e.mods.isShiftDown() || e.mods.isCommandDown());
        juce::Slider::mouseDown (e);
        notifyHUD();
    }

private:
    void notifyHUD()
    {
        if (onInteract)
            onInteract (labelName + "  " + getTextFromValue (getValue()));
    }

    const juce::Image& strip;
    juce::String labelName;
    std::function<void (const juce::String&)> onInteract;
};

/**
 * Switch metálico CAB de 3 posiciones: BASS (0), PUNK (1), CLEAN (2).
 */
class XVCabToggle final : public juce::Component
{
public:
    XVCabToggle (juce::RangedAudioParameter& param,
                 std::array<juce::Image, 3> stateImages,
                 juce::Rectangle<float> spriteBoundsInHitBox,
                 std::function<void (const juce::String&)> onInteractCallback)
        : images (std::move (stateImages)),
          normSpriteRect (spriteBoundsInHitBox),
          onInteract (std::move (onInteractCallback)),
          attachment (param, [this] (float v)
          {
              currentIndex = juce::jlimit (0, 2, juce::roundToInt (v));
              repaint();
          })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attachment.sendInitialUpdate();
    }

    int getCurrentIndex() const noexcept { return currentIndex; }

    void paint (juce::Graphics& g) override
    {
        const auto& img = images[static_cast<size_t> (currentIndex)];
        if (! img.isValid())
            return;

        const auto dest = juce::Rectangle<float> (
            normSpriteRect.getX() * static_cast<float> (getWidth()),
            normSpriteRect.getY() * static_cast<float> (getHeight()),
            normSpriteRect.getWidth() * static_cast<float> (getWidth()),
            normSpriteRect.getHeight() * static_cast<float> (getHeight()));

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, dest, juce::RectanglePlacement::stretchToFit);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float relX = static_cast<float> (e.x) / static_cast<float> (juce::jmax (1, getWidth()));
        const float relY = static_cast<float> (e.y) / static_cast<float> (juce::jmax (1, getHeight()));

        int nextIdx = (currentIndex + 1) % 3;
        if (relX > 0.58f)
            nextIdx = (relY < 0.38f) ? 0 : ((relY < 0.63f) ? 1 : 2);

        attachment.setValueAsCompleteGesture (static_cast<float> (nextIdx));
        announceState (nextIdx);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float relY = static_cast<float> (e.y) / static_cast<float> (juce::jmax (1, getHeight()));
        const int dragIdx = (relY < 0.38f) ? 0 : ((relY < 0.63f) ? 1 : 2);
        if (dragIdx != currentIndex)
        {
            attachment.setValueAsPartOfGesture (static_cast<float> (dragIdx));
            announceState (dragIdx);
        }
    }

private:
    void announceState (int idx)
    {
        static const char* names[3] = { "Gabinete SVT 8x10 Deep", "Gabinete Mark Hoppus SVT-810AV", "API Studio Direct DI" };
        if (onInteract)
            onInteract (names[juce::jlimit (0, 2, idx)]);
    }

    std::array<juce::Image, 3> images;
    juce::Rectangle<float> normSpriteRect;
    std::function<void (const juce::String&)> onInteract;
    int currentIndex = 1;
    juce::ParameterAttachment attachment;
};

/**
 * Footswitch metálico 3PDT con hundimiento mecánico al presionar.
 */
class XVFootswitchButton final : public juce::Component
{
public:
    XVFootswitchButton (juce::RangedAudioParameter& bypassParam,
                        juce::Image upImg,
                        juce::Image downImg,
                        std::function<void (bool)> onBypassChangedCallback,
                        std::function<void (const juce::String&)> onInteractCallback)
        : imgUp (std::move (upImg)),
          imgDown (std::move (downImg)),
          onBypassChanged (std::move (onBypassChangedCallback)),
          onInteract (std::move (onInteractCallback)),
          attachment (bypassParam, [this] (float v)
          {
              isBypassed = (v >= 0.5f);
              if (onBypassChanged)
                  onBypassChanged (isBypassed);
              repaint();
          })
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& img = isMomentaryPressed ? imgDown : imgUp;
        if (! img.isValid())
            return;

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        isMomentaryPressed = true;
        const bool nextBypass = ! isBypassed;
        attachment.setValueAsCompleteGesture (nextBypass ? 1.0f : 0.0f);
        if (onInteract)
            onInteract (nextBypass ? "Bypass (DI Limpio)" : "Pedal Activo");
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isMomentaryPressed = false;
        repaint();
    }

private:
    juce::Image imgUp, imgDown;
    std::function<void (bool)> onBypassChanged;
    std::function<void (const juce::String&)> onInteract;
    bool isBypassed = false;
    bool isMomentaryPressed = false;
    juce::ParameterAttachment attachment;
};

/**
 * Tarjeta flotante estilo macOS / iPhone para Guardar Presets y fijar Preset Predeterminado
 */
class XVAppleSavePresetCard final : public juce::Component
{
public:
    XVAppleSavePresetCard (XVBassPedalAudioProcessor& proc,
                           std::function<void (const juce::String&)> onSavedCallback,
                           std::function<void()> onCloseCallback)
        : processor (proc), onSaved (std::move (onSavedCallback)), onClose (std::move (onCloseCallback))
    {
        nameEditor.setMultiLine (false);
        nameEditor.setReturnKeyStartsNewLine (false);
        nameEditor.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0x24ffffff));
        nameEditor.setColour (juce::TextEditor::outlineColourId,        juce::Colour (0x34ffffff));
        nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff0a84ff));
        nameEditor.setColour (juce::TextEditor::textColourId,           juce::Colour (0xfff5f5f7));
        nameEditor.setColour (juce::TextEditor::highlightColourId,      juce::Colour (0x660a84ff));
        nameEditor.setTextToShowWhenEmpty ("Nombre (ej: Mi Bajo Hoppus)", juce::Colour (0xff8e8e93));
        nameEditor.onReturnKey = [this] { commitSave(); };
        addAndMakeVisible (nameEditor);
    }

    void prepareToOpen()
    {
        if (processor.isActiveUserPreset())
            nameEditor.setText (processor.getProgramName (processor.getCurrentProgram()), false);
        else
            nameEditor.setText ("Mi Bajo Hoppus " + juce::String (processor.getUserPresets().size() + 1), false);

        setVisible (true);
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
        repaint();
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = b.getWidth() / 320.0f;
        const auto card = b.reduced (4.0f * sc);
        nameEditor.setFont (makeXvFont (13.0f * sc, juce::Font::plain));
        nameEditor.setBounds (juce::Rectangle<float> (
            card.getX() + 14.0f * sc,
            card.getY() + 40.0f * sc,
            card.getWidth() - 28.0f * sc,
            30.0f * sc).toNearestInt());
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = b.getWidth() / 320.0f;

        juce::DropShadow cardShadow (juce::Colours::black.withAlpha (0.70f), juce::roundToInt (22.0f * sc), { 0, juce::roundToInt (8.0f * sc) });
        cardShadow.drawForRectangle (g, getLocalBounds().reduced (juce::roundToInt (4.0f * sc)));

        const auto card = b.reduced (4.0f * sc);
        g.setColour (juce::Colour (0xf519191e));
        g.fillRoundedRectangle (card, 18.0f * sc);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (card, 18.0f * sc, 1.0f);

        const float topY = card.getY() + 10.0f * sc;
        g.setColour (juce::Colour (0xfff5f5f7));
        g.setFont (makeXvFont (13.0f * sc, juce::Font::bold));
        g.drawText ("Guardar Mi Preset",
                    juce::Rectangle<float> (card.getX() + 14.0f * sc, topY, card.getWidth() - 52.0f * sc, 21.0f * sc),
                    juce::Justification::centredLeft, false);

        closeRect = { card.getRight() - 32.0f * sc, topY, 20.0f * sc, 20.0f * sc };
        g.setColour (juce::Colours::white.withAlpha (0.11f));
        g.fillEllipse (closeRect);
        XVVectorIcons::drawCloseX (g, closeRect.getCentre(), 3.6f * sc, 1.5f * sc, juce::Colour (0xffd4d4d8));

        const float swRowY = card.getY() + 80.0f * sc;
        defaultRowRect = { card.getX() + 14.0f * sc, swRowY, card.getWidth() - 28.0f * sc, 24.0f * sc };

        XVVectorIcons::drawStar (g, { defaultRowRect.getX() + 6.0f * sc, defaultRowRect.getCentreY() },
                                 5.2f * sc, makeDefaultOnSave ? juce::Colour (0xffffd60a) : juce::Colour (0xff8e8e93));

        g.setColour (juce::Colour (0xffebebf0));
        g.setFont (makeXvFont (11.5f * sc, juce::Font::plain));
        g.drawText ("Predeterminado al abrir",
                    defaultRowRect.withTrimmedLeft (16.0f * sc).withTrimmedRight (42.0f * sc),
                    juce::Justification::centredLeft, false);

        const juce::Rectangle<float> pillSw (defaultRowRect.getRight() - 36.0f * sc, swRowY + 2.5f * sc, 36.0f * sc, 19.0f * sc);
        g.setColour (makeDefaultOnSave ? juce::Colour (0xff30d158) : juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (pillSw, 9.5f * sc);
        const float kx = makeDefaultOnSave ? (pillSw.getRight() - 17.0f * sc) : (pillSw.getX() + 2.0f * sc);
        g.setColour (juce::Colours::white);
        g.fillEllipse (kx, swRowY + 4.5f * sc, 15.0f * sc, 15.0f * sc);

        const float btnY = card.getY() + 113.0f * sc;
        const bool showDelete = processor.isActiveUserPreset();

        if (showDelete)
        {
            deleteBtnRect = { card.getX() + 14.0f * sc, btnY, 88.0f * sc, 30.0f * sc };
            saveBtnRect   = { deleteBtnRect.getRight() + 8.0f * sc, btnY, card.getRight() - 14.0f * sc - (deleteBtnRect.getRight() + 8.0f * sc), 30.0f * sc };

            g.setColour (juce::Colour (0x2eff453a));
            g.fillRoundedRectangle (deleteBtnRect, 9.0f * sc);
            g.setColour (juce::Colour (0xffff453a));
            g.setFont (makeXvFont (11.8f * sc, juce::Font::bold));
            g.drawText ("Eliminar", deleteBtnRect, juce::Justification::centred, false);
        }
        else
        {
            deleteBtnRect = {};
            saveBtnRect   = { card.getX() + 14.0f * sc, btnY, card.getWidth() - 28.0f * sc, 30.0f * sc };
        }

        g.setColour (juce::Colour (0xff0a84ff));
        g.fillRoundedRectangle (saveBtnRect, 9.0f * sc);
        g.setColour (juce::Colours::white);
        g.setFont (makeXvFont (12.4f * sc, juce::Font::bold));
        g.drawText ("Guardar Preset", saveBtnRect, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (closeRect.expanded (4.0f).contains (pt))
        {
            if (onClose) onClose();
            return;
        }
        if (defaultRowRect.expanded (3.0f).contains (pt))
        {
            makeDefaultOnSave = ! makeDefaultOnSave;
            repaint();
            return;
        }
        if (saveBtnRect.contains (pt))
        {
            commitSave();
            return;
        }
        if (! deleteBtnRect.isEmpty() && deleteBtnRect.contains (pt))
        {
            const int uIdx = processor.getActiveUserPresetIndex();
            if (uIdx >= 0 && processor.deleteUserPreset (uIdx))
            {
                if (onSaved) onSaved ("Preset eliminado");
                if (onClose) onClose();
            }
        }
    }

private:
    void commitSave()
    {
        processor.saveCurrentAsUserPreset (nameEditor.getText(), makeDefaultOnSave);
        const auto savedMsg = makeDefaultOnSave
            ? ("Guardado como Predeterminado: " + processor.getProgramName (processor.getCurrentProgram()))
            : ("Preset guardado: " + processor.getProgramName (processor.getCurrentProgram()));
        if (onSaved) onSaved (savedMsg);
        if (onClose) onClose();
    }

    XVBassPedalAudioProcessor& processor;
    std::function<void (const juce::String&)> onSaved;
    std::function<void()> onClose;

    juce::TextEditor nameEditor;
    bool makeDefaultOnSave = true;
    juce::Rectangle<float> closeRect, defaultRowRect, saveBtnRect, deleteBtnRect;
};

/**
 * Tarjeta flotante estilo macOS / Logic Pro para Pitch Shifter Digital y Drop-Tuner (-12 a +12 st).
 */
class XVApplePitchCard final : public juce::Component
{
public:
    XVApplePitchCard (XVBassPedalAudioProcessor& proc,
                      std::function<void (const juce::String&)> onChangeCallback,
                      std::function<void()> onCloseCallback)
        : processor (proc), onChange (std::move (onChangeCallback)), onClose (std::move (onCloseCallback))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    static juce::String describeSemitones (int st)
    {
        switch (st)
        {
            case -12: return "Sub-Octava (-1 Octava)";
            case -7:  return "La Grave (Quinta Abajo)";
            case -5:  return "Si Estandar (Bajo 5 Cuerdas)";
            case -4:  return "Do Grave (C Standard)";
            case -3:  return "Reb / Do# (C# Standard)";
            case -2:  return "Re Completo (D Standard)";
            case -1:  return "Mib / Eb (1/2 Tono Abajo)";
            case  0:  return "Mi Estandar (Afinacion Original)";
            case  1:  return "Capo +1 (Fa / F)";
            case  2:  return "Capo +2 (Fa# / F#)";
            case  5:  return "Capo +5 (La / Cuarta Arriba)";
            case  7:  return "Quinta Arriba (+7 st)";
            case 12:  return "Octava Arriba (+1 Octava / Guitar Wall)";
            default:  return (st > 0 ? "Capo Digital (+" : "Drop Digital (") + juce::String (st) + " Semitonos)";
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = b.getWidth() / 360.0f;

        juce::DropShadow cardShadow (juce::Colours::black.withAlpha (0.72f), juce::roundToInt (22.0f * sc), { 0, juce::roundToInt (9.0f * sc) });
        cardShadow.drawForRectangle (g, getLocalBounds().reduced (juce::roundToInt (4.0f * sc)));

        const auto card = b.reduced (4.0f * sc);
        g.setColour (juce::Colour (0xf517171d));
        g.fillRoundedRectangle (card, 18.0f * sc);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (card, 18.0f * sc, 1.0f);

        const bool pitchOn = processor.isPitchEnabled();
        const int  semi    = processor.getPitchSemitones();
        const float cents  = processor.getPitchCents();
        const float mixPct = processor.getPitchMixPct();

        // Encabezado + Switch Activo + Cerrar
        const float topY = card.getY() + 11.0f * sc;
        g.setColour (juce::Colour (0xfff5f5f7));
        g.setFont (makeXvFont (13.0f * sc, juce::Font::bold));
        g.drawText ("Pitch Digital / Afinacion",
                    juce::Rectangle<float> (card.getX() + 14.0f * sc, topY, 195.0f * sc, 21.0f * sc),
                    juce::Justification::centredLeft, false);

        closeRect = { card.getRight() - 33.0f * sc, topY, 21.0f * sc, 21.0f * sc };
        g.setColour (juce::Colours::white.withAlpha (0.11f));
        g.fillEllipse (closeRect);
        XVVectorIcons::drawCloseX (g, closeRect.getCentre(), 3.6f * sc, 1.5f * sc, juce::Colour (0xffd4d4d8));

        powerToggleRect = { closeRect.getX() - 96.0f * sc, topY, 88.0f * sc, 21.0f * sc };
        const juce::Rectangle<float> pillSw (powerToggleRect.getRight() - 35.0f * sc, topY + 1.0f * sc, 35.0f * sc, 19.0f * sc);
        g.setColour (pitchOn ? juce::Colour (0xff6366f1) : juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (pillSw, 9.5f * sc);
        const float kx = pitchOn ? (pillSw.getRight() - 17.0f * sc) : (pillSw.getX() + 2.0f * sc);
        g.setColour (juce::Colours::white);
        g.fillEllipse (kx, topY + 3.0f * sc, 15.0f * sc, 15.0f * sc);
        g.setColour (pitchOn ? juce::Colour (0xffa5b4fc) : juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (11.2f * sc, juce::Font::bold));
        g.drawText (pitchOn ? "Activo" : "Off", powerToggleRect.withRight (pillSw.getX() - 5.0f * sc),
                    juce::Justification::centredRight, false);

        // Selector central grande de Semitonos: [-]   -2 st   [+]
        const float dispY = card.getY() + 40.0f * sc;
        const juce::Rectangle<float> dispBox (card.getX() + 14.0f * sc, dispY, card.getWidth() - 28.0f * sc, 54.0f * sc);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (dispBox, 12.0f * sc);
        g.setColour (pitchOn ? juce::Colour (0x55818cf8) : juce::Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (dispBox, 12.0f * sc, 1.0f);

        minusSemiRect = { dispBox.getX() + 10.0f * sc, dispBox.getCentreY() - 16.0f * sc, 32.0f * sc, 32.0f * sc };
        plusSemiRect  = { dispBox.getRight() - 42.0f * sc, dispBox.getCentreY() - 16.0f * sc, 32.0f * sc, 32.0f * sc };

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (minusSemiRect, 9.0f * sc);
        g.fillRoundedRectangle (plusSemiRect,  9.0f * sc);
        g.setColour (juce::Colours::white);
        g.setFont (makeXvFont (18.0f * sc, juce::Font::bold));
        g.drawText ("-", minusSemiRect, juce::Justification::centred, false);
        XVVectorIcons::drawPlus (g, plusSemiRect.getCentre(), 5.2f * sc, 1.9f * sc, juce::Colours::white);

        const juce::String semiTxt = (semi > 0 ? "+" : "") + juce::String (semi) + " st";
        g.setColour (pitchOn ? juce::Colour (0xfff5f5f7) : juce::Colour (0xff8e8e93));
        g.setFont (makeXvFont (24.0f * sc, juce::Font::bold));
        g.drawText (semiTxt, juce::Rectangle<float> (minusSemiRect.getRight(), dispBox.getY() + 4.0f * sc,
                                                     plusSemiRect.getX() - minusSemiRect.getRight(), 28.0f * sc),
                    juce::Justification::centred, false);

        g.setColour (pitchOn ? juce::Colour (0xffa5b4fc) : juce::Colour (0xff71717a));
        g.setFont (makeXvFont (10.8f * sc, juce::Font::bold));
        g.drawText (describeSemitones (semi),
                    juce::Rectangle<float> (minusSemiRect.getRight(), dispBox.getBottom() - 21.0f * sc,
                                            plusSemiRect.getX() - minusSemiRect.getRight(), 17.0f * sc),
                    juce::Justification::centred, false);

        // Botones rápidos de afinaciones clásicas de bajo
        static constexpr std::array<int, 6> kQuickVals {{ -12, -5, -2, -1, 0, 12 }};
        static const char* kQuickLbls[6] = { "-12 Oct", "-5 Si", "-2 Re", "-1 Eb", "0 Orig", "+12 Oct" };
        const float qY = card.getY() + 102.0f * sc;
        const float qGap = 5.0f * sc;
        const float qW = (card.getWidth() - 28.0f * sc - 5.0f * qGap) / 6.0f;
        const float qH = 24.0f * sc;

        for (size_t i = 0; i < 6; ++i)
        {
            const juce::Rectangle<float> qr (card.getX() + 14.0f * sc + static_cast<float> (i) * (qW + qGap), qY, qW, qH);
            quickChipRects[i] = qr;
            const bool sel = (pitchOn && semi == kQuickVals[i]) || (! pitchOn && kQuickVals[i] == 0);
            g.setColour (sel ? juce::Colour (0xff6366f1) : juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (qr, 7.0f * sc);
            g.setColour (sel ? juce::Colours::white : juce::Colour (0xffd4d4d8));
            g.setFont (makeXvFont (10.2f * sc, juce::Font::bold));
            g.drawText (kQuickLbls[i], qr, juce::Justification::centred, false);
        }

        // Sliders horizontales: Fino (Cents) y Mezcla (Dry/Wet)
        drawPillSlider (g, sc, card.getX() + 14.0f * sc, card.getY() + 136.0f * sc, card.getWidth() - 28.0f * sc,
                        "Afinacion Fina", (cents > 0.0f ? "+" : "") + juce::String (juce::roundToInt (cents)) + " ct",
                        (cents + 50.0f) / 100.0f, true, centsTrackRect);

        drawPillSlider (g, sc, card.getX() + 14.0f * sc, card.getY() + 174.0f * sc, card.getWidth() - 28.0f * sc,
                        "Mezcla (Dry/Wet)", juce::String (juce::roundToInt (mixPct)) + " %",
                        mixPct / 100.0f, false, mixTrackRect);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        dragMode = 0;

        if (closeRect.expanded (4.0f).contains (pt))
        {
            if (onClose) onClose();
            return;
        }
        if (powerToggleRect.expanded (4.0f).contains (pt))
        {
            const bool next = ! processor.isPitchEnabled();
            processor.setPitchEnabled (next);
            notifyChange();
            return;
        }
        if (minusSemiRect.expanded (3.0f).contains (pt))
        {
            processor.setPitchEnabled (true);
            processor.setPitchSemitones (processor.getPitchSemitones() - 1);
            notifyChange();
            return;
        }
        if (plusSemiRect.expanded (3.0f).contains (pt))
        {
            processor.setPitchEnabled (true);
            processor.setPitchSemitones (processor.getPitchSemitones() + 1);
            notifyChange();
            return;
        }

        static constexpr std::array<int, 6> kQuickVals {{ -12, -5, -2, -1, 0, 12 }};
        for (size_t i = 0; i < 6; ++i)
        {
            if (quickChipRects[i].expanded (2.0f).contains (pt))
            {
                processor.setPitchSemitones (kQuickVals[i]);
                processor.setPitchEnabled (kQuickVals[i] != 0 || std::fabs (processor.getPitchCents()) > 0.5f);
                notifyChange();
                return;
            }
        }

        if (centsTrackRect.expanded (4.0f, 8.0f).contains (pt))
        {
            if (e.getNumberOfClicks() >= 2)
                processor.setPitchCents (0.0f);
            else
            {
                dragMode = 1;
                updateSliderFromX (pt.x);
            }
            notifyChange();
            return;
        }
        if (mixTrackRect.expanded (4.0f, 8.0f).contains (pt))
        {
            if (e.getNumberOfClicks() >= 2)
                processor.setPitchMixPct (100.0f);
            else
            {
                dragMode = 2;
                updateSliderFromX (pt.x);
            }
            notifyChange();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode != 0)
        {
            updateSliderFromX (e.position.x);
            notifyChange();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragMode = 0; }

private:
    static void drawPillSlider (juce::Graphics& g, float sc, float x, float y, float w,
                                const juce::String& label, const juce::String& valStr,
                                float norm01, bool bipolar, juce::Rectangle<float>& outTrack)
    {
        g.setColour (juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (10.6f * sc, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> (x, y, w * 0.62f, 14.0f * sc), juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (0xfff5f5f7));
        g.drawText (valStr, juce::Rectangle<float> (x + w * 0.62f, y, w * 0.38f, 14.0f * sc), juce::Justification::centredRight, false);

        outTrack = { x, y + 18.0f * sc, w, 8.0f * sc };
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (outTrack, 4.0f * sc);

        const float clamped = juce::jlimit (0.0f, 1.0f, norm01);
        const float thumbX = outTrack.getX() + clamped * outTrack.getWidth();
        if (bipolar)
        {
            const float midX = outTrack.getCentreX();
            const float x0 = std::min (midX, thumbX);
            const float fw = std::max (2.0f, std::fabs (thumbX - midX));
            g.setColour (juce::Colour (0xff818cf8));
            g.fillRoundedRectangle (x0, outTrack.getY(), fw, outTrack.getHeight(), 4.0f * sc);
        }
        else
        {
            g.setColour (juce::Colour (0xff6366f1));
            g.fillRoundedRectangle (outTrack.getX(), outTrack.getY(), std::max (6.0f * sc, thumbX - outTrack.getX()), outTrack.getHeight(), 4.0f * sc);
        }

        g.setColour (juce::Colours::white);
        g.fillEllipse (thumbX - 6.5f * sc, outTrack.getCentreY() - 6.5f * sc, 13.0f * sc, 13.0f * sc);
    }

    void updateSliderFromX (float px)
    {
        if (dragMode == 1 && centsTrackRect.getWidth() > 1.0f)
        {
            const float n = juce::jlimit (0.0f, 1.0f, (px - centsTrackRect.getX()) / centsTrackRect.getWidth());
            const float ct = std::round (n * 100.0f - 50.0f);
            processor.setPitchEnabled (true);
            processor.setPitchCents (ct);
        }
        else if (dragMode == 2 && mixTrackRect.getWidth() > 1.0f)
        {
            const float n = juce::jlimit (0.0f, 1.0f, (px - mixTrackRect.getX()) / mixTrackRect.getWidth());
            processor.setPitchEnabled (true);
            processor.setPitchMixPct (std::round (n * 100.0f));
        }
    }

    void notifyChange()
    {
        repaint();
        if (onChange)
        {
            if (! processor.isPitchEnabled())
                onChange ("Pitch Digital: Desactivado");
            else
            {
                const int st = processor.getPitchSemitones();
                onChange ("Pitch: " + juce::String (st > 0 ? "+" : "") + juce::String (st) + " st  (" + describeSemitones (st) + ")");
            }
        }
    }

    XVBassPedalAudioProcessor& processor;
    std::function<void (const juce::String&)> onChange;
    std::function<void()> onClose;

    juce::Rectangle<float> closeRect, powerToggleRect, minusSemiRect, plusSemiRect;
    std::array<juce::Rectangle<float>, 6> quickChipRects {};
    juce::Rectangle<float> centsTrackRect, mixTrackRect;
    int dragMode = 0;
};

/**
 * Tarjeta flotante para Exportar el Sonido + Mini-Imagen 240x240 a M-VAVE Blackbox, NAM y Pedaleras Digitales.
 */
class XVAppleExportCard final : public juce::Component
{
public:
    XVAppleExportCard (XVBassPedalAudioProcessor& proc,
                       const juce::Image& plateImg,
                       const juce::Image& knobStripImg,
                       std::array<juce::Image, 3> cabImgs,
                       const juce::Image& fsUpImg,
                       std::function<void (const juce::String&)> onStatusCallback,
                       std::function<void()> onCloseCallback)
        : processor (proc),
          plateActive (plateImg),
          knobStrip128 (knobStripImg),
          cabImages (std::move (cabImgs)),
          fsUpImage (fsUpImg),
          onStatus (std::move (onStatusCallback)),
          onClose (std::move (onCloseCallback))
    {
        nameEditor.setMultiLine (false);
        nameEditor.setReturnKeyStartsNewLine (false);
        nameEditor.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0x24ffffff));
        nameEditor.setColour (juce::TextEditor::outlineColourId,        juce::Colour (0x34ffffff));
        nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff30d158));
        nameEditor.setColour (juce::TextEditor::textColourId,           juce::Colour (0xfff5f5f7));
        nameEditor.setColour (juce::TextEditor::highlightColourId,      juce::Colour (0x5530d158));
        nameEditor.setTextToShowWhenEmpty ("Nombre perfil (ej: Ger Hoppus)", juce::Colour (0xff8e8e93));
        nameEditor.onTextChange = [this] { refreshMiniPreview(); repaint(); };
        addAndMakeVisible (nameEditor);
    }

    void prepareToOpen()
    {
        nameEditor.setText (processor.getActivePresetCleanName(), false);
        refreshMiniPreview();
        setVisible (true);
        repaint();
    }

    void refreshMiniPreview()
    {
        const auto snap = processor.buildCurrentSnapshot();
        const int cabIdx = juce::jlimit (0, 2, static_cast<int> (snap.cab));
        const auto tight = xvdsp::XVProfileExporter::buildCompositePedalImage (
            plateActive, knobStrip128, cabImages[static_cast<size_t> (cabIdx)], fsUpImage, snap);
        juce::String pName = nameEditor.getText().trim();
        if (pName.isEmpty()) pName = processor.getActivePresetCleanName();
        preview240 = xvdsp::XVProfileExporter::renderBlackboxScreen240 (tight, pName, snap, true);
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = b.getWidth() / 420.0f;
        const auto card = b.reduced (4.0f * sc);
        const float rightX = card.getX() + 146.0f * sc;
        const float rightW = card.getRight() - 14.0f * sc - rightX;
        nameEditor.setFont (makeXvFont (12.2f * sc, juce::Font::plain));
        nameEditor.setBounds (juce::Rectangle<float> (rightX, card.getY() + 38.0f * sc, rightW, 26.0f * sc).toNearestInt());
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = b.getWidth() / 420.0f;

        juce::DropShadow cardShadow (juce::Colours::black.withAlpha (0.74f), juce::roundToInt (24.0f * sc), { 0, juce::roundToInt (9.0f * sc) });
        cardShadow.drawForRectangle (g, getLocalBounds().reduced (juce::roundToInt (4.0f * sc)));

        const auto card = b.reduced (4.0f * sc);
        g.setColour (juce::Colour (0xf617181d));
        g.fillRoundedRectangle (card, 18.0f * sc);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (card, 18.0f * sc, 1.0f);

        // Encabezado
        const float topY = card.getY() + 10.0f * sc;
        g.setColour (juce::Colour (0xfff5f5f7));
        g.setFont (makeXvFont (13.0f * sc, juce::Font::bold));
        g.drawText ("Exportar Sonido a M-VAVE Blackbox y Pedaleras",
                    juce::Rectangle<float> (card.getX() + 14.0f * sc, topY, card.getWidth() - 50.0f * sc, 21.0f * sc),
                    juce::Justification::centredLeft, false);

        closeRect = { card.getRight() - 32.0f * sc, topY, 20.0f * sc, 20.0f * sc };
        g.setColour (juce::Colours::white.withAlpha (0.11f));
        g.fillEllipse (closeRect);
        XVVectorIcons::drawCloseX (g, closeRect.getCentre(), 3.6f * sc, 1.5f * sc, juce::Colour (0xffd4d4d8));

        // Columna izquierda: Vista Previa Real de la Pantallita 240x240 LCD de la Blackbox
        const juce::Rectangle<float> scrRect (card.getX() + 14.0f * sc, card.getY() + 38.0f * sc, 120.0f * sc, 120.0f * sc);
        if (preview240.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (preview240, scrRect);
        }
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawRoundedRectangle (scrRect, 9.0f * sc, 1.0f);

        g.setColour (juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (9.6f * sc, juce::Font::bold));
        g.drawText ("Pantalla 240x240 px",
                    juce::Rectangle<float> (scrRect.getX(), scrRect.getBottom() + 3.0f * sc, scrRect.getWidth(), 14.0f * sc),
                    juce::Justification::centred, false);

        // Columna derecha: Formatos incluidos con checkmarks vectoriales
        const float rightX = card.getX() + 146.0f * sc;
        const float rightW = card.getRight() - 14.0f * sc - rightX;
        static const char* kSpecs[4] = {
            "1_Cargar_en_AMP (.am3data Studio Bi-Amp)",
            "2_Cargar_en_CAB (.wav 44.1kHz 24-bit 1024)",
            "3_Foto_Perillas (.png 240x240 posicion real)",
            "Opcionales (.am4data, .am2data, .nam)"
        };

        g.setFont (makeXvFont (10.3f * sc, juce::Font::plain));
        for (int i = 0; i < 4; ++i)
        {
            const float ry = card.getY() + (71.0f + static_cast<float> (i) * 18.5f) * sc;
            XVVectorIcons::drawCheckmark (g, { rightX + 5.0f * sc, ry + 7.5f * sc }, 6.2f * sc, 1.6f * sc, juce::Colour (0xff30d158));
            g.setColour (juce::Colour (0xffd4d4d8));
            g.drawText (kSpecs[i], juce::Rectangle<float> (rightX + 15.0f * sc, ry, rightW - 15.0f * sc, 16.0f * sc),
                        juce::Justification::centredLeft, true);
        }

        // Botón principal de exportación
        exportBtnRect = { rightX, card.getY() + 148.0f * sc, rightW, 30.0f * sc };
        g.setColour (isExporting ? juce::Colour (0xff248a3d) : juce::Colour (0xff30d158));
        g.fillRoundedRectangle (exportBtnRect, 9.0f * sc);
        g.setColour (juce::Colour (0xff07130a));
        g.setFont (makeXvFont (11.8f * sc, juce::Font::bold));
#if JUCE_IOS
        const char* idleExportLabel = "Exportar y Compartir Pack (.ZIP)...";
#else
        const char* idleExportLabel = "Elegir Carpeta y Exportar Pack...";
#endif
        g.drawText (isExporting ? "Destilando modelo neuronal..." : idleExportLabel,
                    exportBtnRect, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (closeRect.expanded (6.0f).contains (pt))
        {
            if (onClose) onClose();
            return;
        }
        if (! isExporting && exportBtnRect.expanded (4.0f).contains (pt))
            triggerFolderExport();
    }

private:
    void triggerFolderExport()
    {
#if JUCE_IOS
        isExporting = true;
        repaint();

        juce::String profileName = nameEditor.getText().trim();
        if (profileName.isEmpty())
            profileName = processor.getActivePresetCleanName();

        const auto snap = processor.buildCurrentSnapshot();
        auto docsDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        docsDir.createDirectory();

        auto summary = xvdsp::XVProfileExporter::exportFullHardwarePack (
            docsDir.getFullPathName().toStdString(), profileName.toStdString(), snap);

        if (summary.ok)
        {
            const int cabIdx = juce::jlimit (0, 2, static_cast<int> (snap.cab));
            summary.filesWritten += xvdsp::XVProfileExporter::exportMiniPedalImages (
                summary.folderPath,
                plateActive,
                knobStrip128,
                cabImages[static_cast<size_t> (cabIdx)],
                fsUpImage,
                profileName,
                snap);

            const juce::File exportedFolder (juce::String (summary.folderPath));
            const juce::File zipOut = docsDir.getChildFile (exportedFolder.getFileName() + ".zip");
            zipOut.deleteFile();
            {
                juce::ZipFile::Builder zb;
                juce::Array<juce::File> allFiles;
                exportedFolder.findChildFiles (allFiles, juce::File::findFiles, true);
                for (const auto& f : allFiles)
                {
                    const auto rel = f.getRelativePathFrom (docsDir).replaceCharacter ('\\', '/');
                    zb.addFile (f, 6, rel);
                }
                juce::FileOutputStream zos (zipOut);
                if (zos.openedOk())
                    zb.writeToStream (zos, nullptr);
            }

            isExporting = false;
            if (onStatus)
                onStatus ("Pack guardado en Archivos: " + zipOut.getFileName());

            if (zipOut.existsAsFile())
            {
                activeShareBox = juce::ContentSharer::shareFilesScoped (
                    { juce::URL (zipOut) },
                    [safeThis = juce::Component::SafePointer<XVAppleExportCard> (this)] (bool, const juce::String&)
                    {
                        if (safeThis != nullptr && safeThis->onClose)
                            safeThis->onClose();
                    },
                    this);
            }
            else if (onClose)
            {
                onClose();
            }
        }
        else
        {
            isExporting = false;
            if (onStatus)
                onStatus ("Error al exportar en Documentos de iPad");
            repaint();
        }
#else
        auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
        if (! defaultDir.isDirectory())
            defaultDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        activeChooser = std::make_unique<juce::FileChooser> (
            "Selecciona donde guardar tu Pack para M-VAVE Blackbox y Pedaleras", defaultDir);

        const int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        activeChooser->launchAsync (flags, [safeThis = juce::Component::SafePointer<XVAppleExportCard> (this)] (const juce::FileChooser& fc)
        {
            if (safeThis == nullptr)
                return;

            const auto chosen = fc.getResult();
            if (chosen == juce::File())
                return;

            safeThis->isExporting = true;
            safeThis->repaint();

            juce::String profileName = safeThis->nameEditor.getText().trim();
            if (profileName.isEmpty())
                profileName = safeThis->processor.getActivePresetCleanName();

            const auto snap = safeThis->processor.buildCurrentSnapshot();
            const std::string baseDir = chosen.getFullPathName().toStdString();

            auto summary = xvdsp::XVProfileExporter::exportFullHardwarePack (
                baseDir, profileName.toStdString(), snap);

            if (summary.ok)
            {
                const int cabIdx = juce::jlimit (0, 2, static_cast<int> (snap.cab));
                summary.filesWritten += xvdsp::XVProfileExporter::exportMiniPedalImages (
                    summary.folderPath,
                    safeThis->plateActive,
                    safeThis->knobStrip128,
                    safeThis->cabImages[static_cast<size_t> (cabIdx)],
                    safeThis->fsUpImage,
                    profileName,
                    snap);

                juce::File (juce::String (summary.folderPath)).revealToUser();
                if (safeThis->onStatus)
                    safeThis->onStatus ("Listo para Blackbox: 1_AMP (.am3data) + 2_CAB (.wav)");
                safeThis->isExporting = false;
                if (safeThis->onClose)
                    safeThis->onClose();
            }
            else
            {
                safeThis->isExporting = false;
                if (safeThis->onStatus)
                    safeThis->onStatus ("No se pudo escribir en la carpeta elegida");
                safeThis->repaint();
            }
        });
#endif
    }

    XVBassPedalAudioProcessor& processor;
    const juce::Image& plateActive;
    const juce::Image& knobStrip128;
    std::array<juce::Image, 3> cabImages;
    juce::Image fsUpImage;
    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onClose;

    juce::TextEditor nameEditor;
    juce::Image preview240;
    std::unique_ptr<juce::FileChooser> activeChooser;
#if JUCE_IOS
    juce::ScopedMessageBox activeShareBox;
#endif
    bool isExporting = false;
    juce::Rectangle<float> closeRect, exportBtnRect;
};

/**
 * Tarjeta flotante estilo iPhone Dynamic Island / Logic Pro para el Afinador Cromático de Bajo.
 */
class XVAppleTunerCard final : public juce::Component
{
public:
    XVAppleTunerCard (XVBassPedalAudioProcessor& proc, std::function<void()> onCloseCallback)
        : processor (proc), onClose (std::move (onCloseCallback))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void updateReading (const xvdsp::TunerReading& r) noexcept
    {
        reading = r;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float w = b.getWidth();
        const float sc = w / 330.0f;

        juce::DropShadow cardShadow (juce::Colours::black.withAlpha (0.68f), juce::roundToInt (22.0f * sc), { 0, juce::roundToInt (9.0f * sc) });
        cardShadow.drawForRectangle (g, getLocalBounds().reduced (juce::roundToInt (4.0f * sc)));

        const auto card = b.reduced (4.0f * sc);
        g.setColour (juce::Colour (0xf316161a));
        g.fillRoundedRectangle (card, 20.0f * sc);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (card, 20.0f * sc, 1.0f);

        const float topY = card.getY() + 11.0f * sc;
        a4MinusRect = { card.getX() + 13.0f * sc, topY, 21.0f * sc, 21.0f * sc };
        a4PlusRect  = { card.getX() + 89.0f * sc, topY, 21.0f * sc, 21.0f * sc };
        const juce::Rectangle<float> a4LabelRect (a4MinusRect.getRight(), topY, a4PlusRect.getX() - a4MinusRect.getRight(), 21.0f * sc);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (a4MinusRect, 10.5f * sc);
        g.fillRoundedRectangle (a4PlusRect,  10.5f * sc);
        g.setColour (juce::Colour (0xffebebf0));
        g.setFont (makeXvFont (12.5f * sc, juce::Font::bold));
        g.drawText ("-", a4MinusRect, juce::Justification::centred, false);
        XVVectorIcons::drawPlus (g, a4PlusRect.getCentre(), 3.8f * sc, 1.5f * sc, juce::Colour (0xffebebf0));

        g.setColour (juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (11.2f * sc, juce::Font::plain));
        g.drawText (juce::String (juce::roundToInt (processor.getTuner().getReferenceA4())) + " Hz",
                    a4LabelRect, juce::Justification::centred, false);

        closeRect = { card.getRight() - 33.0f * sc, topY, 21.0f * sc, 21.0f * sc };
        g.setColour (juce::Colours::white.withAlpha (0.11f));
        g.fillEllipse (closeRect);
        XVVectorIcons::drawCloseX (g, closeRect.getCentre(), 3.6f * sc, 1.5f * sc, juce::Colour (0xffd4d4d8));

        muteToggleRect = { closeRect.getX() - 104.0f * sc, topY, 95.0f * sc, 21.0f * sc };
        const bool mutedNow = processor.isTunerMuted();
        const juce::Rectangle<float> pillSwitch (muteToggleRect.getRight() - 35.0f * sc, topY + 1.0f * sc, 35.0f * sc, 19.0f * sc);
        g.setColour (mutedNow ? juce::Colour (0xffff9f0a) : juce::Colours::white.withAlpha (0.16f));
        g.fillRoundedRectangle (pillSwitch, 9.5f * sc);
        const float knobX = mutedNow ? (pillSwitch.getRight() - 17.0f * sc) : (pillSwitch.getX() + 2.0f * sc);
        g.setColour (juce::Colours::white);
        g.fillEllipse (knobX, topY + 3.0f * sc, 15.0f * sc, 15.0f * sc);

        g.setColour (mutedNow ? juce::Colour (0xffffd60a) : juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (11.2f * sc, juce::Font::bold));
        g.drawText ("Silenciar", muteToggleRect.withRight (pillSwitch.getX() - 5.0f * sc),
                    juce::Justification::centredRight, false);

        const float cx = card.getCentreX();
        const float arcCy = card.getY() + 126.0f * sc;
        const float arcR  = 82.0f * sc;

        const bool inTune   = reading.hasSignal && std::fabs (reading.cents) <= 2.8f;
        const bool nearTune = reading.hasSignal && std::fabs (reading.cents) <= 12.0f;
        const juce::Colour stateCol = ! reading.hasSignal
            ? juce::Colour (0xff52525b)
            : (inTune ? juce::Colour (0xff30d158) : (nearTune ? juce::Colour (0xffffd60a) : juce::Colour (0xffff453a)));

        for (int i = -20; i <= 20; ++i)
        {
            const float norm = static_cast<float> (i) / 20.0f;
            const float ang  = norm * 1.12f - juce::MathConstants<float>::halfPi;
            const bool isCenter = (i == 0);
            const bool isMajor  = (i % 5 == 0);
            const float innerR  = arcR - (isCenter ? 12.5f : (isMajor ? 8.5f : 5.2f)) * sc;

            const float x1 = cx + std::cos (ang) * innerR;
            const float y1 = arcCy + std::sin (ang) * innerR;
            const float x2 = cx + std::cos (ang) * arcR;
            const float y2 = arcCy + std::sin (ang) * arcR;

            float tickAlpha = isCenter ? 0.85f : (isMajor ? 0.35f : 0.16f);
            juce::Colour tickCol = juce::Colours::white.withAlpha (tickAlpha);

            if (reading.hasSignal)
            {
                const float centNorm = juce::jlimit (-1.0f, 1.0f, reading.cents / 50.0f);
                if (std::fabs (norm - centNorm) < 0.08f || (isCenter && inTune))
                    tickCol = stateCol;
            }

            g.setColour (tickCol);
            g.drawLine (x1, y1, x2, y2, (isCenter ? 2.3f : (isMajor ? 1.5f : 1.0f)) * sc);
        }

        if (reading.hasSignal)
        {
            const float centNorm = juce::jlimit (-1.0f, 1.0f, reading.cents / 50.0f);
            const float needleAng = centNorm * 1.12f - juce::MathConstants<float>::halfPi;
            const float nx1 = cx + std::cos (needleAng) * (arcR - 28.0f * sc);
            const float ny1 = arcCy + std::sin (needleAng) * (arcR - 28.0f * sc);
            const float nx2 = cx + std::cos (needleAng) * (arcR + 3.0f * sc);
            const float ny2 = arcCy + std::sin (needleAng) * (arcR + 3.0f * sc);

            g.setColour (stateCol.withAlpha (0.32f));
            g.drawLine (nx1, ny1, nx2, ny2, 5.8f * sc);
            g.setColour (stateCol);
            g.drawLine (nx1, ny1, nx2, ny2, 2.5f * sc);
            g.fillEllipse (nx2 - 3.3f * sc, ny2 - 3.3f * sc, 6.6f * sc, 6.6f * sc);
        }

        g.setColour (reading.hasSignal ? (inTune ? juce::Colour (0xff30d158) : juce::Colours::white)
                                       : juce::Colour (0xff71717a));
        g.setFont (makeXvFont (42.0f * sc, juce::Font::bold));
        const juce::String noteStr = reading.hasSignal
            ? (juce::String (reading.noteName) + juce::String (reading.octave))
            : "--";
        g.drawText (noteStr, juce::Rectangle<float> (cx - 76.0f * sc, arcCy - 34.0f * sc, 152.0f * sc, 44.0f * sc),
                    juce::Justification::centred, false);

        juce::String guideText = "Toca una cuerda al aire";
        if (reading.hasSignal)
        {
            const int ctInt = juce::roundToInt (reading.cents);
            if (inTune)
                guideText = "AFINADO PERFECTO";
            else if (reading.cents < 0.0f)
                guideText = "Subir tension (" + juce::String (ctInt) + " ct)";
            else
                guideText = "Aflojar cuerda (+" + juce::String (ctInt) + " ct)";
        }

        const juce::Rectangle<float> badgeRect (cx - 102.0f * sc, arcCy + 13.0f * sc, 204.0f * sc, 21.0f * sc);
        g.setColour (stateCol.withAlpha (reading.hasSignal ? 0.16f : 0.08f));
        g.fillRoundedRectangle (badgeRect, 10.5f * sc);
        g.setColour (reading.hasSignal ? stateCol : juce::Colour (0xffa1a1aa));
        g.setFont (makeXvFont (11.5f * sc, juce::Font::bold));
        g.drawText (guideText, badgeRect, juce::Justification::centred, false);

        const juce::String subInfo = reading.hasSignal
            ? (juce::String (reading.stringHint) + "  |  " + juce::String (reading.freqHz, 1) + " Hz")
            : "B0 - E1 - A1 - D2 - G2 - Drop D";
        g.setColour (juce::Colour (0xff8e8e93));
        g.setFont (makeXvFont (10.6f * sc, juce::Font::plain));
        g.drawText (subInfo, juce::Rectangle<float> (card.getX(), card.getBottom() - 22.0f * sc, w - 8.0f * sc, 17.0f * sc),
                    juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (closeRect.expanded (4.0f).contains (pt))
        {
            if (onClose) onClose();
            return;
        }
        if (muteToggleRect.expanded (4.0f).contains (pt))
        {
            processor.setTunerMuted (! processor.isTunerMuted());
            repaint();
            return;
        }
        if (a4MinusRect.expanded (3.0f).contains (pt))
        {
            processor.getTuner().setReferenceA4 (processor.getTuner().getReferenceA4() - 1.0f);
            repaint();
            return;
        }
        if (a4PlusRect.expanded (3.0f).contains (pt))
        {
            processor.getTuner().setReferenceA4 (processor.getTuner().getReferenceA4() + 1.0f);
            repaint();
            return;
        }
    }

private:
    XVBassPedalAudioProcessor& processor;
    std::function<void()> onClose;
    xvdsp::TunerReading reading {};

    juce::Rectangle<float> closeRect, muteToggleRect, a4MinusRect, a4PlusRect;
};

//==============================================================================
class XVBassPedalAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit XVBassPedalAudioProcessorEditor (XVBassPedalAudioProcessor&);
    ~XVBassPedalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showHudMessage (const juce::String& text, int holdFrames = 42);
    void closeAllFloatingCardsExcept (juce::Component* keepOpen);
    void toggleTunerCard();
    void togglePitchCard();
    void toggleSavePresetCard();
    void toggleExportCard();
    void openApplePresetMenu();

    // Geometría calibrada con márgenes pequeños, limpios y perfectamente proporcionados
    static constexpr float kPlateWidth   = 1391.0f;
    static constexpr float kPlateHeight  = 1131.0f;
    static constexpr float kPedalOffsetX = -22.0f;
    static constexpr float kPedalOffsetY =  44.0f;
    static constexpr float kBottomTrim   =  10.0f;
    static constexpr float kRefWidth     = kPlateWidth  + 2.0f * kPedalOffsetX; // 1347.0f
    static constexpr float kRefHeight    = kPlateHeight + kPedalOffsetY - kBottomTrim; // 1165.0f

    XVBassPedalAudioProcessor& audioProcessor;
    XVApplePopupLookAndFeel appleMenuLnF;

    juce::Image plateActive;
    juce::Image plateBypass;
    juce::Image knobPointerStrip;
    bool pedalBypassed = false;

    struct KnobEntry
    {
        juce::String paramId;
        juce::Rectangle<float> refBounds;
        float defaultValue = 0.0f;
        std::unique_ptr<XVFilmstripKnob> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::vector<KnobEntry> knobs;
    std::unique_ptr<XVCabToggle>           cabToggle;
    std::unique_ptr<XVFootswitchButton>    footswitch;
    std::unique_ptr<XVAppleTunerCard>      tunerCard;
    std::unique_ptr<XVApplePitchCard>      pitchCard;
    std::unique_ptr<XVAppleSavePresetCard> savePresetCard;
    std::unique_ptr<XVAppleExportCard>     exportCard;

    // 5 Píldoras flotantes minimalistas simétricas estilo macOS Sonoma / iPhone
    juce::Rectangle<float> tunerPillRect;
    juce::Rectangle<float> pitchPillRect;
    juce::Rectangle<float> prevArrowRect;
    juce::Rectangle<float> presetPillRect;
    juce::Rectangle<float> nextArrowRect;
    juce::Rectangle<float> savePillRect;
    juce::Rectangle<float> exportPillRect;
    juce::Rectangle<float> updateBadgeRect;

    bool hoverTunerPill   = false;
    bool hoverPitchPill   = false;
    bool hoverPrevArrow   = false;
    bool hoverPresetPill  = false;
    bool hoverNextArrow   = false;
    bool hoverSavePill    = false;
    bool hoverExportPill  = false;
    bool hoverUpdateBadge = false;

    xvdsp::TunerReading latestTunerReading {};

    juce::String hudText;
    float hudAlpha = 0.0f;
    int   hudHoldFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XVBassPedalAudioProcessorEditor)
};
