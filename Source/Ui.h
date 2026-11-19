#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// The faltung controls: the rows, lists, switches and text buttons of the faltung plug-ins
// (github.com/femifleming/faltung, Source/Ui.h and Ui.cpp), copied here so that Leitner is drawn with
// the same parts and stays its own repository. Lowercase Inter and IBM Plex Mono, a thin rule with a
// slanted bar, the parallelogram as the house shape.
//
// What is different from faltung's copy: the "deep" look and the generic preset bar are left out
// (Leitner has its own, with your presets in it), and every row can be drawn mirrored (value · rule ·
// label, read from the right), because Leitner's window is mirror symmetrical.
namespace fal::ui
{
/** UTF-8 literal → String (literals with "·", "–", "…") */
inline juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

// ---- type -----------------------------------------------------------------------------------------
juce::Font sans (float size, bool medium = false);
juce::Font mono (float size, bool medium = false);

/** text colour for a ground */
inline juce::Colour inkFor (juce::Colour ground)
{
    return ground.getPerceivedBrightness() > 0.42f ? juce::Colour (0xff1d1c15) : juce::Colour (0xfff6f6f0);
}
/** the slider "edge" mark: white on mid grounds, ink on pale grounds */
inline juce::Colour edgeFor (juce::Colour ground)
{
    return ground.getPerceivedBrightness() > 0.86f ? inkFor (ground) : juce::Colours::white;
}

struct Theme
{
    juce::Colour ground, ink, fill, edge;
};

// ---- a parallelogram, the house shape -------------------------------------------------------------
juce::Path skewRect (juce::Rectangle<float> r, float skew = 0.55f);

// ---- parameter slider: label · thin rule · slanted fill · edge · value ------------------------------
class ParamSlider : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamSlider (juce::RangedAudioParameter& p, Theme t, juce::String label = {}, bool mirrored = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setFill (juce::Colour c) { theme.fill = c; repaint(); }
    void setLabelWidth (int w) { labelW = w; resized(); repaint(); }
    void setTheme (Theme t) { theme = t; repaint(); }
    /** where modulation has moved it (0..1), drawn as a hollow tick; < 0: none */
    void setGhost (float g) { if (std::abs (g - ghost) > 0.002f) { ghost = g; repaint(); } }
    std::function<void()> onTouch;
    juce::RangedAudioParameter& param;

private:
    Theme theme;
    juce::String label;
    bool mirrored;
    int labelW = 64;
    float value01 = 0, dragStart = 0, ghost = -1.0f;
    bool dragging = false;
    juce::ParameterAttachment attach;
    juce::Rectangle<float> track() const;
};

// ---- choice: "label  value" — click for a menu, drag to step ----------------------------------------
class ChoiceBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceBox (juce::RangedAudioParameter& p, Theme t, juce::String label = {}, bool mirrored = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::RangedAudioParameter& param;
    bool asTabs = false;   // draw every option as a row of tabs
    int labelW = 64;
    void setTheme (Theme t) { theme = t; repaint(); }

private:
    Theme theme; juce::String label; bool mirrored;
    int index = 0, dragStartIndex = 0; bool moved = false;
    juce::ParameterAttachment attach;
    void set (int i);
    int numItems() const;
};

// ---- toggle: small parallelogram, filled when on --------------------------------------------------
class ToggleBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleBox (juce::RangedAudioParameter& p, Theme t, juce::String label = {}, bool mirrored = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setTheme (Theme t) { theme = t; repaint(); }
    juce::RangedAudioParameter& param;

private:
    Theme theme; juce::String label; bool mirrored, on = false;
    juce::ParameterAttachment attach;
};

// ---- text button in the house style ----------------------------------------------------------------
class TextLink : public juce::Component, public juce::SettableTooltipClient
{
public:
    TextLink (juce::String t, Theme th) : text (std::move (t)), theme (th) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (isEnabled() && onClick) onClick(); }
    void setText (juce::String t) { if (t != text) { text = std::move (t); repaint(); } }
    void setLit (bool l) { if (l != lit) { lit = l; repaint(); } }
    std::function<void()> onClick;
    juce::String text; Theme theme; bool lit = false;
    juce::Justification align = juce::Justification::centred;
};
} // namespace fal::ui
