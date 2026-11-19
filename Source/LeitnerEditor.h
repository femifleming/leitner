#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "LeitnerProcessor.h"
#include "Ui.h"

//==============================================================================
// Leitner in the faltung look, on its own cream paper. One flat ground; on it one darker field, the
// room, drawn as a thin wireframe with a single white face (the wall you face) and the two lines of
// sound in it: ink for line 1, red for line 2, the one red thing, after the red rods of Bernhard
// Leitner's Plexiglas models. Lowercase Inter and IBM Plex Mono, the faltung rows (label · thin rule ·
// slanted bar · value), the figures as a row of folded paper strips.
//
// It is mirror symmetrical: the room in the middle; on its left what comes from Leitner (the figure in
// plan and elevation, as an architect draws a building), on its right, mirrored, what comes from
// Xenakis (the lengths chance gave the passes, the random walk, the sieve); under them the strips, and
// the controls in four columns, the two on the right read from the right. Drag the room to turn it and
// see where things are from any side; double-click it to face front again.
//
// The colours are the ones Leitner has always had: paper, the desk under it, ink, a faint grey, red.
namespace lt
{
extern const juce::Colour paper, desk, ink, faint, red, sand;
fal::ui::Theme theme();          // the rows: paper, ink, a sand bar, an ink edge
fal::ui::Theme switchTheme();    // switches: filled with ink when on

// where things are in the window, in design pixels (the test tool cuts the manual's pictures by these)
namespace lay
{
constexpr int W = 1500, H = 850, margin = 36;
constexpr int bandY = 92, bandH = 468;                 // the pictures
constexpr int sideW = 416, sq = 200, sqGap = 16;       // a side: two squares across, two down
constexpr int stageX = 472, stageW = 556;              // the room
constexpr int stripY = 572, stripH = 56;               // the figures
constexpr int ruleY = 636, lineY = 645;                // the rule under the pictures, and the control line
constexpr int rowsY = 682, rowH = 21, colW = 336, colGap = 28;
inline juce::Rectangle<int> left()   { return { margin, bandY, sideW, bandH }; }
inline juce::Rectangle<int> stage()  { return { stageX, bandY, stageW, bandH }; }
inline juce::Rectangle<int> right()  { return { W - margin - sideW, bandY, sideW, bandH }; }
inline juce::Rectangle<int> strips() { return { margin, stripY, W - 2 * margin, stripH }; }
inline juce::Rectangle<int> controls() { return { margin - 4, ruleY, W - 2 * margin + 8, H - ruleY - 28 }; }
}

//==============================================================================
// Leitner's side: the figure seen from above (plan) and from the side (elevation), with the raster's
// loudspeakers and where each line is now, and what it is in words
class Plan : public juce::Component
{
public:
    explicit Plan (LeitnerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    LeitnerProcessor& proc;
};

// Xenakis's side: the length chance gave each recent pass, the trail of the random walk, and which
// loudspeakers the sieve lets through. Read right to left, the mirror of Leitner's side.
class Chance : public juce::Component
{
public:
    explicit Chance (LeitnerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void poll();   // (on the timer) note each new pass
private:
    LeitnerProcessor& proc;
    struct Pass { float length, turn, height; };
    std::vector<Pass> history;
    uint32_t seenPass = 0;
};

// Your own figure, while you draw it: the drawing, which point is picked, and undo. The sheet and the
// room both edit through this, so a point picked in one is picked in the other.
class Drawing
{
public:
    explicit Drawing (LeitnerProcessor& p) : proc (p) {}
    const leitner::Drawn& get() const { return proc.drawing(); }
    void begin();                                // before a change: remember it for undo
    void set (const leitner::Drawn&);            // the change (as often as you like during a drag)
    void undo();
    bool canUndo() const { return ! undos.empty(); }
    void movePoint (int i, leitner::Vec3);
    int insertPoint (int after, leitner::Vec3);  // returns its index
    void removePoint (int i);
    // a freehand stroke (points in the room's -1..1), thinned to at most 64 points; ends that meet close the loop
    void setStroke (const std::vector<leitner::Vec3>&, bool closeIfEndsMeet);
    void fromFigure (int figure);                // start from one of the figures
    int picked = -1;
    std::function<void()> changed;
private:
    LeitnerProcessor& proc;
    std::vector<leitner::Drawn> undos;
};

// The sheet you draw on, in place of Leitner's while the figure is your own: the path from above,
// from the side and from behind, every point draggable in each; click to add a point, shift-drag to
// draw a new path freehand, double-click a point to take it away.
class DrawSheet : public juce::Component
{
public:
    DrawSheet (LeitnerProcessor&, Drawing&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void refreshButtons();
private:
    struct Ortho { juce::Rectangle<float> box; leitner::Vec3 right, up; const char* name; };
    std::array<Ortho, 3> views() const;
    static juce::Point<float> toScreen (const Ortho&, leitner::Vec3);
    static leitner::Vec3 fromScreen (const Ortho&, juce::Point<float>, leitner::Vec3 keep);
    int viewAt (juce::Point<float>) const;
    int pointAt (const Ortho&, juce::Point<float>) const;
    LeitnerProcessor& proc;
    Drawing& drawing;
    fal::ui::TextLink closedBtn, curvedBtn, atPointsBtn, fromBtn, clearBtn, undoBtn;
    int dragView = -1, dragPoint = -1;
    bool stroking = false;
    std::vector<juce::Point<float>> stroke;
};

// the room, in perspective, with the two lines of sound in it. Drag to turn it. While the figure is
// your own, your points are drawn in it too: drag one to move it (across your view of the room), or
// shift-drag on the room to draw a new path on the plane through the middle, facing you.
class Room : public juce::Component
{
public:
    Room (LeitnerProcessor& p, Drawing& d) : proc (p), drawing (d) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    struct View { leitner::Vec3 f, r, u; };
    View view() const;
    float unit() const;                           // half the opening of the room seen from the front, in pixels
    juce::Point<float> middle() const;
    juce::Point<float> project (const View&, leitner::Vec3, float* scale = nullptr) const;
    leitner::Vec3 onMiddlePlane (const View&, juce::Point<float>) const;
    int handleAt (juce::Point<float>) const;
    bool editing() const;
    LeitnerProcessor& proc;
    Drawing& drawing;
    float yawDown = 0.0f, pitchDown = 0.0f;
    int dragPoint = -1;
    leitner::Vec3 pointDown {};
    bool stroking = false;
    std::vector<leitner::Vec3> stroke;
    std::vector<juce::Point<float>> strokeScreen;
};

// the figures as a row of folded paper strips: Leitner's eleven in the sands of the paper, Xenakis's
// three with the red in them, and your own, dashed. Click one to choose it.
class FigureStrip : public juce::Component, public juce::TooltipClient
{
public:
    explicit FigureStrip (LeitnerProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String getTooltip() override;
    static juce::String code (int figure);        // "tl1", "kr6", ... as on the strips
    static juce::String shortName (int figure);   // the name on the strip
private:
    juce::Path face (int i) const;
    int at (juce::Point<float>) const;
    juce::ParameterAttachment att;
    int index = 0, hover = -1;
};

// the programs: ‹ name ›; click the name for the list (after Leitner, after Xenakis, yours, save, the folder)
class Presets : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Presets (LeitnerProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void poll();
private:
    void step (int d);
    void menu();
    LeitnerProcessor& proc;
    juce::String userShown;                      // your preset's name, while it is the one loaded
    int seenProgram = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};
}

//==============================================================================
class LeitnerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LeitnerEditor (LeitnerProcessor&);
    ~LeitnerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int designW = lt::lay::W, designH = lt::lay::H;

private:
    struct Content : juce::Component
    {
        explicit Content (LeitnerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        LeitnerEditor& owner;
    };
    struct Look : juce::LookAndFeel_V4
    {
        Look();
        juce::Font getPopupMenuFont() override { return fal::ui::mono (12.0f); }
        void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    };
    struct Column { juce::String title; int x; bool mirrored; };

    void paintContent (juce::Graphics&);
    void timerCallback() override;

    LeitnerProcessor& proc;
    Look look;
    juce::TooltipWindow tips { this, 600 };
    Content content { *this };
    lt::Drawing drawing { proc };
    lt::Room room { proc, drawing };
    lt::Plan plan { proc };
    lt::DrawSheet drawSheet { proc, drawing };
    lt::Chance chance { proc };
    lt::FigureStrip strips { proc };
    lt::Presets presets { proc };
    juce::OwnedArray<juce::Component> parts;     // the rows
    std::vector<Column> columns;
    fal::ui::TextLink randomBtn { "a random patch", lt::theme() }, drawBtn { "draw your own figure", lt::theme() };
    int lastFigure = 0;                          // to go back to from your own
    int seenFigure = -1, seenOut = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeitnerEditor)
};
