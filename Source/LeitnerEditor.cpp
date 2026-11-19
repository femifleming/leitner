#include "LeitnerEditor.h"

using namespace juce;
namespace ui = fal::ui;

namespace lt
{
// the cream palette: paper, the desk it lay on (now the one darker field, the room), ink, a faint grey
// for asides, the red of line 2; and a sand between paper and desk for the bars of the rows
const Colour paper (0xfff3f0e8), desk (0xffd9d5cb), ink (0xff141414), faint (0xff8c8880), red (0xffc4281c), sand (0xffc6bfae);
ui::Theme theme()       { return { paper, ink, sand, ink }; }
ui::Theme switchTheme() { return { paper, ink, ink, ink }; }

namespace
{
const String sep (CharPointer_UTF8 ("  \xc2\xb7  "));
const Colour rule = ink.withAlpha (0.18f);

String lower (const char* utf8) { return String::fromUTF8 (utf8).toLowerCase(); }

// a chip: the house parallelogram, centred on a point
Path chip (Point<float> c, float w, float h)
{
    return ui::skewRect ({ c.x - 0.5f * w - 0.275f * h, c.y - 0.5f * h, w, h });
}
void drawChip (Graphics& g, Point<float> c, float w, float h, Colour fill, Colour line, float thick = 1.1f)
{
    const auto p = chip (c, w, h);
    g.setColour (fill); g.fillPath (p);
    g.setColour (line); g.strokePath (p, PathStrokeType (thick));
}

// the name of a side or a column: mono, medium, a hairline under it
void heading (Graphics& g, Rectangle<float> r, const String& title, bool right)
{
    g.setColour (ink); g.setFont (ui::mono (12.0f, true));
    g.drawText (title, r.withHeight (18.0f), right ? Justification::centredRight : Justification::centredLeft);
    g.setColour (rule);
    g.fillRect (r.getX(), r.getY() + 21.0f, r.getWidth(), 1.0f);
}
void caption (Graphics& g, Rectangle<float> r, const String& t, bool right, Colour c = ink, float size = 10.0f)
{
    g.setColour (c); g.setFont (ui::mono (size));
    g.drawText (t, r, right ? Justification::centredRight : Justification::centredLeft, true);
}

// you, from above: head and shoulders, facing the front (up the page)
void listenerAbove (Graphics& g, Point<float> c, float s)
{
    g.fillEllipse (c.x - 0.24f * s, c.y - 0.06f * s, 0.48f * s, 0.15f * s);
    g.fillEllipse (c.x - 0.07f * s, c.y - 0.09f * s, 0.14f * s, 0.14f * s);
}
// you, standing, seen level: head, body, legs; 'nose' draws which way you look (to the right)
void listenerStanding (Graphics& g, Point<float> head, Point<float> feet, bool nose)
{
    const float h = feet.y - head.y, w = h * 0.2f;
    g.fillEllipse (head.x - w * 0.36f, head.y - h * 0.09f, w * 0.72f, h * 0.17f);
    g.fillRoundedRectangle (head.x - w * 0.5f, head.y + h * 0.11f, w, h * 0.42f, w * 0.3f);
    g.fillRect (head.x - w * 0.3f, head.y + h * 0.5f, w * 0.6f, h * 0.5f);
    if (nose) g.drawLine (head.x + w * 0.5f, head.y, head.x + w * 0.95f, head.y, 1.2f);
}
// the walls' loudspeakers round a square, five a side: scenery, as on Leitner's sheets
void wallSpeakers (Graphics& g, Rectangle<float> room)
{
    const auto c = room.getCentre(); const float s = 0.5f * room.getWidth();
    for (int i = 0; i < 5; ++i)
    {
        const float a = -1.0f + (2.0f * (float) i + 1.0f) / 5.0f;
        for (auto q : { Point<float> (c.x + a * s, c.y - s), Point<float> (c.x + a * s, c.y + s), Point<float> (c.x - s, c.y + a * s), Point<float> (c.x + s, c.y + a * s) })
        {
            g.setColour (paper); g.fillEllipse (q.x - 2.2f, q.y - 2.2f, 4.4f, 4.4f);
            g.setColour (ink.withAlpha (0.55f)); g.drawEllipse (q.x - 2.2f, q.y - 2.2f, 4.4f, 4.4f, 0.6f);
        }
    }
}

// the two squares across the top of a side, and the one under the first
Rectangle<float> cell (int column, int row)
{
    return { (float) (column * (lay::sq + lay::sqGap)), (float) (42 + row * (lay::sq + 26)), (float) lay::sq, (float) lay::sq };
}
Rectangle<float> cellLabel (int column, int row) { return cell (column, row).withHeight (12.0f).translated (0.0f, -16.0f); }
}

//==============================================================================
// Leitner's side. Plan: seen from above, the front at the top. Elevation: seen from the left, the
// front to the right. The room is the box the unit sphere sits in, the listener in the middle.
void Plan::paint (Graphics& g)
{
    using leitner::Vec3;
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const float W = (float) getWidth();
    float ws[leitner::kMaxRaster]; int idx[leitner::kMaxRaster];
    const int n = leitner::continuous (p) ? 0 : leitner::rasterPoints (p, ws, idx);

    // one view: the room, the listener, the two lines, their loudspeakers, where they are now
    auto view = [&] (Rectangle<float> box, bool fromAbove)
    {
        const float s = 0.5f * box.getWidth();
        const auto c = box.getCentre();
        auto P = [&] (Vec3 v) { return fromAbove ? Point<float> (c.x - v.y * s, c.y - v.x * s) : Point<float> (c.x + v.x * s, c.y - v.z * s); };
        g.setColour (ink);
        g.drawRect (box, 1.0f);
        wallSpeakers (g, box);
        g.setColour (ink);
        if (fromAbove)
        {
            listenerAbove (g, c, s);
            g.drawLine (c.x, c.y - 0.09f * s, c.x, c.y - 0.15f * s, 1.2f);
            caption (g, { c.x - 50.0f, box.getY() + 4.0f, 100.0f, 11.0f }, "front" + sep + "vorn", false, faint, 9.0f);
        }
        else
        {
            listenerStanding (g, P ({ 0, 0, 0 }), P ({ 0, 0, -1 }), true);
            caption (g, { box.getRight() - 84.0f, box.getY() + 4.0f, 80.0f, 11.0f }, "front" + sep + "vorn", true, faint, 9.0f);
            caption (g, { box.getX() + 5.0f, box.getBottom() - 15.0f, 60.0f, 11.0f }, "floor", false, faint, 9.0f);
        }
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (box.expanded (6.0f).toNearestInt());
        for (int li = 1; li >= 0; --li)
        {
            const Colour col = li == 0 ? ink : red;
            Path path;
            for (int i = 0; i <= 240; ++i)
            {
                const auto q = P (leitner::linePoint (li, p, st, (float) i / 240.0f));
                if (i == 0) path.startNewSubPath (q); else path.lineTo (q);
            }
            g.setColour (col);
            g.strokePath (path, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
            for (int k = 0; k < n; ++k) drawChip (g, P (leitner::linePoint (li, p, st, ws[k], idx[k])), 8.0f, 5.5f, paper, col, 1.0f);
            auto& v = proc.space.view[li];
            for (auto [x, y, z, gain] : { std::tuple { v.ax.load(), v.ay.load(), v.az.load(), v.ga.load() }, std::tuple { v.bx.load(), v.by.load(), v.bz.load(), v.gb.load() } })
            {
                if (gain < 0.02f) continue;
                g.setColour (col.withAlpha (jlimit (0.0f, 1.0f, gain)));
                g.fillPath (chip (P ({ x, y, z }), 9.0f + 5.0f * gain, 6.0f + 3.5f * gain));
            }
        }
    };

    heading (g, { 0.0f, 0.0f, W, 18.0f }, "ton-raum" + sep + "sound space", false);
    caption (g, cellLabel (0, 0), "grundriss" + sep + "plan, seen from above", false);
    caption (g, cellLabel (1, 0), "aufriss" + sep + "elevation, from the side", false);
    view (cell (0, 0), true);
    view (cell (1, 0), false);

    // the figure in words
    const int fig = p.figure;
    const float y0 = cell (0, 1).getY() - 8.0f;
    g.setColour (ink); g.setFont (ui::mono (12.0f, true));
    const String code = FigureStrip::code (fig);
    g.drawText (code, Rectangle<float> (0.0f, y0, 40.0f, 16.0f), Justification::centredLeft);
    g.drawText (lower (leitner::figureNames()[fig]), Rectangle<float> (40.0f, y0, W - 40.0f, 16.0f), Justification::centredLeft);
    caption (g, { 40.0f, y0 + 18.0f, W - 40.0f, 14.0f }, String (leitner::figureEnglish()[fig]).toLowerCase(), false, ink, 11.0f);
    const String pts = p.points >= leitner::kMaxPoints ? String ("a continuous line") : String (n) + " loudspeakers";
    caption (g, { 40.0f, y0 + 36.0f, W - 40.0f, 14.0f },
             pts + sep + "legato " + String (roundToInt (p.legato * 100.0f)) + " %" + sep + "one pass " + String (p.seconds, 2) + " s", false, ink, 11.0f);
    const String asides[] = { "sound as a building material: a sound sent from loudspeaker",
                              "to loudspeaker draws a line, and lines make a space",
                              "",
                              "after bernhard leitner, ton-raum, 1969 to now" };
    for (int i = 0; i < 4; ++i) caption (g, { 40.0f, y0 + 66.0f + 14.0f * (float) i, W - 40.0f, 13.0f }, asides[i], false, faint);
    // which line is which: a stroke of each
    const float ly = (float) getHeight() - 22.0f;
    g.setColour (ink); g.fillRect (40.0f, ly + 6.0f, 22.0f, 2.0f);
    caption (g, { 68.0f, ly, 150.0f, 13.0f }, "line 1, the left channel", false, faint);
    g.setColour (red); g.fillRect (226.0f, ly + 6.0f, 22.0f, 2.0f);
    caption (g, { 254.0f, ly, 162.0f, 13.0f }, "line 2, the right, its mirror", false, faint);
}

//==============================================================================
// Xenakis's side, right to left
void Chance::poll()
{
    const auto st = proc.space.stateNow();
    if (st.pass == seenPass) return;
    seenPass = st.pass;
    history.push_back ({ proc.space.passLength.load(), st.dTurn, st.dHeight });
    if (history.size() > 24) history.erase (history.begin());
    repaint();
}

void Chance::paint (Graphics& g)
{
    const auto p = proc.readSpace();
    const float W = (float) getWidth();
    heading (g, { 0.0f, 0.0f, W, 18.0f }, "stochastic space" + sep + "xenakis", true);

    // chance: the length of each of the last passes as slanted bars, the newest on the left
    caption (g, cellLabel (0, 0), "zufall" + sep + "chance: the length of each pass", true);
    {
        const auto area = cell (0, 0);
        g.setColour (ink);
        g.fillRect (area.getX(), area.getBottom() - 1.0f, area.getWidth(), 1.0f);
        const float unit = area.getHeight() / 4.0f;                    // bars up to four times the pass
        const float y1 = area.getBottom() - unit;
        g.setColour (ink.withAlpha (0.35f));
        g.fillRect (area.getX(), y1, area.getWidth(), 1.0f);
        if (history.empty()) { g.setColour (faint); g.setFont (ui::mono (9.5f)); g.drawText ("(waiting for a pass to end)", area.withTrimmedBottom (unit), Justification::centred); }
        const float bw = area.getWidth() / 24.0f;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const size_t age = history.size() - 1 - i;                 // 0: the newest
            const float x = area.getX() + (float) age * bw;
            const float h = jmin (area.getHeight() - 2.0f, history[i].length * unit);
            g.setColour (age == 0 ? red : ink);
            g.fillPath (ui::skewRect ({ x + 1.0f, area.getBottom() - 1.0f - h, bw - 3.5f, h }, 2.0f / jmax (2.0f, h)));
        }
        {   // (over the bars, on paper)
            const Rectangle<float> label (area.getRight() - 78.0f, y1 - 13.0f, 78.0f, 11.0f);
            g.setColour (paper); g.fillRect (label);
            caption (g, label, "the pass as set", true, faint, 9.0f);
        }
    }

    // the walk: where the steps have taken the figure, turn across and height up, the trail of the last passes
    caption (g, cellLabel (1, 0), "irrfahrt" + sep + "walk: where chance has stepped", true);
    {
        const auto area = cell (1, 0);
        g.setColour (ink);
        g.drawRect (area, 1.0f);
        g.setColour (rule);
        g.fillRect (area.getCentreX(), area.getY(), 1.0f, area.getHeight());
        g.fillRect (area.getX(), area.getCentreY(), area.getWidth(), 1.0f);
        const auto in = area.reduced (5.0f, 4.0f);
        g.setColour (faint); g.setFont (ui::mono (9.0f));
        g.drawText ("turned left", in.withTrimmedBottom (14.0f), Justification::centredLeft);
        g.drawText ("turned right", in.withTrimmedBottom (14.0f), Justification::centredRight);
        g.drawText ("higher", in, Justification::centredTop);
        g.drawText ("lower", in, Justification::centredBottom);
        auto at = [&] (float turnDeg, float h)
        {
            return Point<float> (area.getCentreX() - jlimit (-1.0f, 1.0f, turnDeg / 90.0f) * area.getWidth() * 0.46f,
                                 area.getCentreY() - jlimit (-1.0f, 1.0f, h / 0.9f) * area.getHeight() * 0.44f);
        };
        Path trail;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const auto q = at (history[i].turn, history[i].height);
            if (i == 0) trail.startNewSubPath (q); else trail.lineTo (q);
        }
        g.setColour (ink);
        g.strokePath (trail, PathStrokeType (1.0f));
        for (size_t i = 0; i < history.size(); ++i) drawChip (g, at (history[i].turn, history[i].height), 6.0f, 4.5f, paper, ink, 0.9f);
        const auto st = proc.space.stateNow();
        g.setColour (red);
        g.fillPath (chip (at (st.dTurn, st.dHeight), 10.0f, 7.0f));
    }

    // the sieve: one cell per loudspeaker of the raster, filled where it sounds, the first on the right
    const float y0 = cell (0, 1).getY() - 16.0f;
    caption (g, { 0.0f, y0, W, 12.0f }, "sieb" + sep + "sieve: which loudspeakers sound", true);
    {
        const int K = jlimit (2, leitner::kMaxPoints - 1, p.points >= leitner::kMaxPoints ? 32 : p.points);
        const float step = (W - 22.0f) / (float) jmax (1, K - 1);
        const float cw = jlimit (4.0f, 13.0f, step * 0.5f);
        int sounding = 0;
        for (int k = 0; k < K; ++k)
        {
            const Point<float> c (W - 10.0f - step * (float) k, y0 + 28.0f);
            const bool on = leitner::inSieve (p.sieve, k);
            sounding += on ? 1 : 0;
            drawChip (g, c, cw, 9.0f, on ? ink : paper, ink, 0.9f);
            if (K <= 16 || k % 4 == 0) caption (g, { c.x - 12.0f, c.y + 8.0f, 20.0f, 11.0f }, String (k), false, faint, 9.0f);
        }
        caption (g, { 0.0f, y0 + 52.0f, W, 14.0f },
                 String (leitner::sieveNames()[p.sieve]) + (p.sieve == 0 ? "" : "  (unions of residue classes)") + ":  " + String (sounding) + " of " + String (K) + " sound", true, ink, 11.0f);
    }
    // what chance is set to, in words
    const String words[] = {
        p.chance <= 0.0f ? String ("chance 0 %: every pass the same length")
                         : "chance " + String (roundToInt (p.chance * 100.0f)) + " %: lengths by an exponential law, as in his stochastic music",
        p.walk <= 0.0f ? String ("walk 0 %: the figure stays where it is set")
                       : "walk " + String (roundToInt (p.walk * 100.0f)) + " %: a gaussian step every pass, pulled gently home",
        p.scatter <= 0.0f ? String ("scatter 0 %: every loudspeaker on the line")
                          : "scatter " + String (roundToInt (p.scatter * 100.0f)) + " %: each loudspeaker thrown off the line, a cloud of points" };
    for (int i = 0; i < 3; ++i) caption (g, { 0.0f, y0 + 78.0f + 16.0f * (float) i, W, 14.0f }, words[i], true, ink, 10.5f);
    caption (g, { 0.0f, y0 + 136.0f, W, 13.0f }, "he filled rooms with sound by calculation and chance", true, faint);
    caption (g, { 0.0f, y0 + 164.0f, W, 13.0f }, "after iannis xenakis, polytopes, 1967 to 1978", true, faint);
    caption (g, { 0.0f, (float) getHeight() - 22.0f, W, 13.0f }, "set below: sieve, chance, walk, scatter", true, faint);
}

//==============================================================================
// The room is two units across (the unit sphere's box) and you stand at the middle of it. The camera
// looks at the middle from 2.64 away: from straight behind you to begin with (the back wall then
// comes out 0.45 of the opening, as on Leitner's sheets), and from wherever you drag it to. However
// it is turned, all of it stays inside its field.
constexpr float kCamera = 2.64f;
Room::View Room::view() const
{
    const float yaw = proc.viewYaw.load() * leitner::kPi / 180.0f, pitch = proc.viewPitch.load() * leitner::kPi / 180.0f;
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    return { { cp * cy, cp * sy, -sp }, { sy, -cy, 0.0f }, { sp * cy, sp * sy, cp } };
}
float Room::unit() const { return 0.31f * (float) getHeight(); }   // (so no corner of it ever runs under the words in the field's corners)
Point<float> Room::middle() const { return { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f + 4.0f }; }
Point<float> Room::project (const View& v, leitner::Vec3 p, float* scale) const
{
    constexpr float D = kCamera;
    const float depth = jmax (0.35f, p.dot (v.f) + D);
    const float s = (D - 1.0f) / depth;                      // 1 at the opening of the front view
    const float k = unit();
    if (scale != nullptr) *scale = s;
    return { middle().x + p.dot (v.r) * k * s, middle().y - p.dot (v.u) * k * s };
}
bool Room::editing() const { return proc.figure->getIndex() == leitner::fgDrawn; }
// a point on the screen, on the plane through the middle of the room that faces you
leitner::Vec3 Room::onMiddlePlane (const View& v, Point<float> q) const
{
    const float k = unit() * (kCamera - 1.0f) / kCamera;
    return v.r * ((q.x - middle().x) / k) + v.u * (-(q.y - middle().y) / k);
}
int Room::handleAt (Point<float> q) const
{
    if (! editing()) return -1;
    const auto p = proc.readSpace(); const auto st = proc.space.stateNow(); const auto V = view();
    const auto& d = drawing.get();
    int best = -1; float bestD = 9.0f;
    for (int i = d.count - 1; i >= 0; --i)
        if (const float dd = project (V, leitner::drawnToRoom (p, st, d.pt[i])).getDistanceFrom (q); dd < bestD) { bestD = dd; best = i; }
    return best;
}
void Room::mouseMove (const MouseEvent& e)
{
    setMouseCursor (editing() && handleAt (e.position) >= 0 ? MouseCursor::UpDownLeftRightResizeCursor
                    : editing() && e.mods.isShiftDown() ? MouseCursor::CrosshairCursor : MouseCursor::DraggingHandCursor);
}
void Room::mouseDown (const MouseEvent& e)
{
    yawDown = proc.viewYaw.load(); pitchDown = proc.viewPitch.load();
    dragPoint = -1; stroking = false;
    if (! editing()) return;
    if (const int h = handleAt (e.position); h >= 0)
    {
        drawing.picked = h; drawing.begin(); dragPoint = h;
        pointDown = leitner::drawnToRoom (proc.readSpace(), proc.space.stateNow(), drawing.get().pt[h]);
        return;
    }
    if (e.mods.isShiftDown()) { stroking = true; stroke = { onMiddlePlane (view(), e.position) }; strokeScreen = { e.position }; }
}
void Room::mouseUp (const MouseEvent&)
{
    if (stroking && stroke.size() >= 3)
    {
        const auto p = proc.readSpace(); const auto st = proc.space.stateNow();
        std::vector<leitner::Vec3> pts;
        for (auto v : stroke) pts.push_back (leitner::roomToDrawn (p, st, v));
        drawing.begin();
        drawing.setStroke (pts, true);
    }
    stroking = false; stroke.clear(); strokeScreen.clear(); dragPoint = -1;
    repaint();
}
void Room::mouseDrag (const MouseEvent& e)
{
    if (dragPoint >= 0)
    {
        // across the screen, at the point's own depth
        const auto V = view();
        float s = 1.0f; project (V, pointDown, &s);
        const float k = unit() * s;
        const auto moved = pointDown + V.r * ((float) e.getDistanceFromDragStartX() / k) - V.u * ((float) e.getDistanceFromDragStartY() / k);
        drawing.movePoint (dragPoint, leitner::roomToDrawn (proc.readSpace(), proc.space.stateNow(), moved));
        repaint();
        return;
    }
    if (stroking)
    {
        if (strokeScreen.back().getDistanceFrom (e.position) > 2.5f) { strokeScreen.push_back (e.position); stroke.push_back (onMiddlePlane (view(), e.position)); repaint(); }
        return;
    }
    float yaw = yawDown - 0.45f * (float) e.getDistanceFromDragStartX();
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    proc.viewYaw.store (yaw);
    proc.viewPitch.store (jlimit (-30.0f, 89.0f, pitchDown + 0.4f * (float) e.getDistanceFromDragStartY()));
    repaint();
}
void Room::mouseDoubleClick (const MouseEvent& e)
{
    if (const int h = handleAt (e.position); h >= 0) { drawing.begin(); drawing.removePoint (h); return; }
    proc.viewYaw.store (0.0f); proc.viewPitch.store (0.0f); repaint();
}

void Room::paint (Graphics& g)
{
    using leitner::Vec3;
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const auto V = view();
    auto P = [&] (Vec3 v) { return project (V, v); };
    // the one darker field
    g.setColour (desk);
    g.fillRect (getLocalBounds());
    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (getLocalBounds());

    // the cube's twelve edges, thin
    g.setColour (ink);
    for (int a = 0; a < 3; ++a)
        for (float s : { -1.0f, 1.0f }) for (float t : { -1.0f, 1.0f })
        {
            Vec3 from, to;
            if (a == 0) { from = { -1, s, t }; to = { 1, s, t }; } else if (a == 1) { from = { s, -1, t }; to = { s, 1, t }; } else { from = { s, t, -1 }; to = { s, t, 1 }; }
            g.drawLine ({ P (from), P (to) }, 1.0f);
        }

    // the wall you face: the white face. Seen from outside the room it is only its outline, so you can see in.
    {
        Path wall;
        wall.startNewSubPath (P ({ 1, -1, -1 })); wall.lineTo (P ({ 1, 1, -1 })); wall.lineTo (P ({ 1, 1, 1 })); wall.lineTo (P ({ 1, -1, 1 })); wall.closeSubPath();
        const bool between = V.f.x < -0.25f;
        if (! between) { g.setColour (Colours::white.withAlpha (0.62f)); g.fillPath (wall); }
        g.setColour (Colours::white);
        g.strokePath (wall, PathStrokeType (between ? 1.4f : 2.0f));
    }

    // the loudspeakers: small open circles in a grid on every face, each drawn as it lies on its wall.
    // A wall between you and the room is drawn faint so you can see in through it.
    constexpr int N = 5;
    for (int wallNo = 0; wallNo < 6; ++wallNo)
    {
        const int axis = wallNo / 2; const float side = wallNo % 2 == 0 ? -1.0f : 1.0f;
        Vec3 n {}, t1 {}, t2 {};
        if (axis == 0) { n = { side, 0, 0 }; t1 = { 0, 1, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 1) { n = { 0, side, 0 }; t1 = { 1, 0, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 2) { n = { 0, 0, side }; t1 = { 1, 0, 0 }; t2 = { 0, 1, 0 }; }
        const bool nearSide = n.dot (V.f) < -0.25f;
        g.setColour (ink.withAlpha (nearSide ? 0.12f : 0.5f));
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                const float a = -1.0f + (2.0f * (float) i + 1.0f) / (float) N, b = -1.0f + (2.0f * (float) j + 1.0f) / (float) N;
                const Vec3 c = n + t1 * a + t2 * b;
                const auto pc = P (c), pa = P (c + t1 * 0.04f), pb = P (c + t2 * 0.04f);
                Path e; e.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
                g.strokePath (e, PathStrokeType (0.6f), AffineTransform (pa.x - pc.x, pb.x - pc.x, pc.x, pa.y - pc.y, pb.y - pc.y, pc.y));
            }
    }

    // which way is the front: an arrow on the floor, and a word over the front wall
    {
        g.setColour (ink.withAlpha (0.8f));
        Path arrow; arrow.addArrow ({ P ({ 0.45f, 0, -1 }), P ({ 0.9f, 0, -1 }) }, 1.0f, 7.0f, 8.0f);
        g.fillPath (arrow);
        g.setFont (ui::mono (9.5f));   // (over the wall's left-hand corner, where the figure never hides it and the lines seldom go)
        g.drawText ("front" + sep + "vorn", Rectangle<float> (110.0f, 12.0f).withPosition (P ({ 1.0f, 1.0f, 1.0f }).translated (1.0f, -15.0f)), Justification::centredLeft);
    }

    // the figure: solid ink, standing in the middle, ears at the centre of the room
    {
        const auto head = P ({ 0, 0, 0 }), feet = P ({ 0, 0, -1 });
        const float h = jmax (4.0f, head.getDistanceFrom (feet)), w = h * 0.2f;
        g.setColour (ink);
        Path body;
        body.addEllipse (-w * 0.36f, -h * 0.09f, w * 0.72f, h * 0.17f);
        body.addRoundedRectangle (-w * 0.62f, h * 0.11f, w * 1.24f, h * 0.42f, w * 0.3f);
        body.addRectangle (-w * 0.5f, h * 0.5f, w * 0.44f, h * 0.5f);
        body.addRectangle (w * 0.06f, h * 0.5f, w * 0.44f, h * 0.5f);
        const float tilt = std::atan2 (feet.x - head.x, feet.y - head.y);
        g.fillPath (body, AffineTransform::rotation (-tilt).translated (head));
    }

    // the two lines of sound, with arrows; the loudspeakers each is built from, as chips; where it is now
    float ws[leitner::kMaxRaster]; int idx[leitner::kMaxRaster];
    const int n = leitner::continuous (p) ? 0 : leitner::rasterPoints (p, ws, idx);
    for (int li = 1; li >= 0; --li)
    {
        const Colour col = li == 0 ? ink : red;
        Path path;
        constexpr int S = 240;
        for (int i = 0; i <= S; ++i)
        {
            const auto q = P (leitner::linePoint (li, p, st, (float) i / (float) S));
            if (i == 0) path.startNewSubPath (q); else path.lineTo (q);
        }
        g.setColour (col);
        g.strokePath (path, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        for (float w : { 0.3f, 0.7f })
        {
            const auto a = P (leitner::linePoint (li, p, st, w)), b = P (leitner::linePoint (li, p, st, w + 0.012f));
            if (a.getDistanceFrom (b) < 0.5f) continue;
            Path head; head.addArrow ({ a, a + (b - a) * (10.0f / a.getDistanceFrom (b)) }, 0.0f, 9.0f, 10.0f);
            g.fillPath (head);
        }
        for (int k = 0; k < n; ++k)
        {
            float s; const auto c = project (V, leitner::linePoint (li, p, st, ws[k], idx[k]), &s);
            drawChip (g, c, 13.0f * s, 8.5f * s, desk, col, 1.2f);
        }
        auto& v = proc.space.view[li];
        const float lev = jlimit (0.0f, 1.0f, 0.35f + 4.0f * v.level.load());
        auto sounding = [&] (float x, float y, float z, float gain)
        {
            if (gain < 0.02f) return;
            float s; const auto c = project (V, { x, y, z }, &s);
            const float r = (6.0f + 7.0f * lev * gain) * s;
            g.setColour (col.withAlpha (jlimit (0.0f, 1.0f, gain)));
            g.fillPath (chip (c, 2.4f * r, 1.5f * r));
        };
        sounding (v.ax.load(), v.ay.load(), v.az.load(), v.ga.load());
        sounding (v.bx.load(), v.by.load(), v.bz.load(), v.gb.load());
    }

    // your own figure: your points, on line 1, as small squares to take hold of
    if (editing())
    {
        const auto& d = drawing.get();
        for (int i = 0; i < d.count; ++i)
        {
            float s; const auto c = project (V, leitner::drawnToRoom (p, st, d.pt[i]), &s);
            const float r = jmax (3.0f, 4.5f * s);
            const auto box = Rectangle<float> (2 * r, 2 * r).withCentre (c);
            g.setColour (i == drawing.picked ? ink : Colours::white); g.fillRect (box);
            g.setColour (ink); g.drawRect (box, 1.1f);
        }
        if (strokeScreen.size() > 1)
        {
            Path sp; sp.startNewSubPath (strokeScreen[0]);
            for (auto& q : strokeScreen) sp.lineTo (q);
            g.setColour (ink.withAlpha (0.5f)); g.strokePath (sp, PathStrokeType (1.4f));
        }
    }

    // what it is, and how to hold it
    const int fig = p.figure;
    g.setColour (ink); g.setFont (ui::mono (12.0f, true));
    const String name = FigureStrip::code (fig) + "  " + lower (leitner::figureNames()[fig]);
    g.drawText (name, 14, 10, 300, 16, Justification::centredLeft);
    caption (g, { 14.0f, 27.0f, 300.0f, 14.0f }, String (leitner::figureEnglish()[fig]).toLowerCase(), false, ink, 11.0f);
    const float hy = (float) getHeight() - 38.0f;
    caption (g, { 14.0f, hy, 180.0f, 13.0f }, editing() ? "drag a point to move it" : "drag the room to turn it", false, ink.withAlpha (0.6f));
    caption (g, { 14.0f, hy + 14.0f, 260.0f, 13.0f }, editing() ? "shift-drag: draw" + sep + "drag the room: turn" : String ("double-click: from the front again"), false, ink.withAlpha (0.6f));
}

//==============================================================================
// your own figure
void Drawing::begin()
{
    undos.push_back (get());
    if (undos.size() > 50) undos.erase (undos.begin());
}
void Drawing::set (const leitner::Drawn& d)
{
    proc.setDrawing (d);
    if (picked >= get().count) picked = get().count - 1;
    if (changed) changed();
}
void Drawing::undo()
{
    if (undos.empty()) return;
    const auto d = undos.back(); undos.pop_back();
    set (d);
}
static leitner::Vec3 clampRoom (leitner::Vec3 v) { return { jlimit (-1.0f, 1.0f, v.x), jlimit (-1.0f, 1.0f, v.y), jlimit (-1.0f, 1.0f, v.z) }; }
void Drawing::movePoint (int i, leitner::Vec3 v)
{
    auto d = get();
    if (! isPositiveAndBelow (i, d.count)) return;
    d.pt[i] = clampRoom (v);
    set (d);
}
int Drawing::insertPoint (int after, leitner::Vec3 v)
{
    auto d = get();
    if (d.count >= leitner::kMaxDrawn) return -1;
    const int at = jlimit (0, d.count, after + 1);
    for (int k = d.count; k > at; --k) d.pt[k] = d.pt[k - 1];
    d.pt[at] = clampRoom (v); ++d.count;
    set (d);
    return at;
}
void Drawing::removePoint (int i)
{
    auto d = get();
    if (! isPositiveAndBelow (i, d.count) || d.count <= 2) return;   // (a path needs two points)
    for (int k = i; k + 1 < d.count; ++k) d.pt[k] = d.pt[k + 1];
    --d.count;
    picked = -1;
    set (d);
}
void Drawing::setStroke (const std::vector<leitner::Vec3>& raw, bool closeIfEndsMeet)
{
    using leitner::Vec3;
    // only steps of some length, then the corners that matter (Douglas-Peucker), loosened until it fits in 64
    std::vector<Vec3> s;
    for (auto v : raw) if (s.empty() || (v - s.back()).length() > 0.02f) s.push_back (clampRoom (v));
    if (s.size() < 2) return;
    bool closed = false;
    if (closeIfEndsMeet && s.size() >= 6 && (s.front() - s.back()).length() < 0.2f) { closed = true; s.back() = s.front(); }
    std::vector<Vec3> kept;
    for (float eps = 0.012f;; eps *= 1.4f)
    {
        std::vector<bool> keep (s.size(), false);
        keep.front() = keep.back() = true;
        std::function<void (size_t, size_t)> dp = [&] (size_t a, size_t b)
        {
            if (b <= a + 1) return;
            const Vec3 ab = s[b] - s[a]; const float L = ab.length();
            float worst = -1.0f; size_t at = a;
            for (size_t i = a + 1; i < b; ++i)
            {
                const Vec3 ap = s[i] - s[a];
                float d;
                if (L < 1.0e-6f) d = ap.length();
                else { const float t = jlimit (0.0f, 1.0f, ap.dot (ab) / (L * L)); d = (ap - ab * t).length(); }
                if (d > worst) { worst = d; at = i; }
            }
            if (worst > eps) { keep[at] = true; dp (a, at); dp (at, b); }
        };
        dp (0, s.size() - 1);
        kept.clear();
        for (size_t i = 0; i < s.size(); ++i) if (keep[i]) kept.push_back (s[i]);
        if (closed && kept.size() > 2) kept.pop_back();          // (the last is the first again)
        if ((int) kept.size() <= leitner::kMaxDrawn) break;
    }
    leitner::Drawn d = get();
    d.count = (int) kept.size();
    for (int i = 0; i < d.count; ++i) d.pt[i] = kept[(size_t) i];
    d.closed = closed && d.count >= 3; d.curved = true;
    picked = -1;
    set (d);
}
void Drawing::fromFigure (int f)
{
    using namespace leitner;
    if (! isPositiveAndBelow (f, (int) fgDrawn)) return;
    Drawn d = get();
    d.closed = closedFigure (f);
    // the figures made of straight lines by their corners; the round ones by many points along them
    int n = d.closed ? 32 : 24; d.curved = true;
    switch (f)
    {
        case fgLine: case fgLiege: case fgVertical: case fgExpand: n = 2; d.curved = false; break;
        case fgCube: case fgPolytope: n = 8; d.curved = false; break;
        case fgParaboloid: n = 7; d.curved = false; break;
        case fgVault: case fgSoundcube: n = 48; d.curved = false; break;
        default: break;
    }
    d.count = n;
    for (int k = 0; k < n; ++k)
        d.pt[k] = clampRoom (figurePoint (f, d.closed ? (float) k / (float) n : (float) k / (float) (n - 1), 1.0f, 0.0f, 0.0f, 0u));
    picked = -1;
    set (d);
}

//==============================================================================
DrawSheet::DrawSheet (LeitnerProcessor& p, Drawing& d)
    : proc (p), drawing (d), closedBtn ({}, theme()), curvedBtn ({}, theme()), atPointsBtn ({}, theme()),
      fromBtn ("start from a figure...", theme()), clearBtn ("start again", theme()), undoBtn ("undo", theme())
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &closedBtn, &curvedBtn, &atPointsBtn, &fromBtn, &clearBtn, &undoBtn }) { b->align = Justification::centredLeft; addAndMakeVisible (b); }
    auto toggle = [this] (auto change) { return [this, change] { drawing.begin(); auto dd = drawing.get(); change (dd); drawing.set (dd); refreshButtons(); }; };
    closedBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.closed = ! dd.closed; });
    curvedBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.curved = ! dd.curved; });
    atPointsBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.atPoints = ! dd.atPoints; });
    closedBtn.setTooltip ("a closed loop runs round; an open line goes there and comes back");
    curvedBtn.setTooltip ("a curve through your points, or straight lines from one to the next");
    atPointsBtn.setTooltip ("the loudspeakers: spread evenly by 'points', or one at each of your points");
    fromBtn.setTooltip ("turn one of the fourteen figures into points you can move");
    clearBtn.setTooltip ("a plain line from back to front, to draw from");
    undoBtn.setTooltip ("undo (cmd-z), fifty steps back");
    clearBtn.onClick = [this]
    {
        drawing.begin();
        leitner::Drawn dd; dd.count = 2; dd.closed = false; dd.curved = true;
        dd.pt[0] = { -0.8f, 0.0f, 0.0f }; dd.pt[1] = { 0.8f, 0.0f, 0.0f };
        drawing.picked = 1;
        drawing.set (dd);
    };
    undoBtn.onClick = [this] { drawing.undo(); };
    fromBtn.onClick = [this]
    {
        PopupMenu m;
        m.addSectionHeader ("start again from");
        for (int f = 0; f < leitner::fgDrawn; ++f) m.addItem (f + 1, lower (leitner::figureNames()[f]) + sep + String (leitner::figureEnglish()[f]).toLowerCase());
        SafePointer<DrawSheet> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (fromBtn), [safe] (int r) { if (safe != nullptr && r > 0) { safe->drawing.begin(); safe->drawing.fromFigure (r - 1); } });
    };
    refreshButtons();
}

void DrawSheet::refreshButtons()
{
    const auto& d = drawing.get();
    closedBtn.setText (d.closed ? "a closed loop" : "open: there and back");
    curvedBtn.setText (d.curved ? "curved through them" : "straight lines");
    atPointsBtn.setText (d.atPoints ? "speakers at my points" : "speakers by 'points'");
    if (undoBtn.isEnabled() != drawing.canUndo()) { undoBtn.setEnabled (drawing.canUndo()); undoBtn.repaint(); }
}

void DrawSheet::resized()
{
    // the six, as a list beside the third view
    const auto c = cell (1, 1).toNearestInt();
    int y = c.getY() - 2;
    for (auto* b : { &closedBtn, &curvedBtn, &atPointsBtn, &fromBtn, &clearBtn, &undoBtn }) { b->setBounds (c.getX() + 18, y, c.getWidth() - 18, 20); y += 21; }
}

// from above (the front at the top), from the right-hand side (the front to the right), from behind
// (as the room is first seen: your left on the left)
std::array<DrawSheet::Ortho, 3> DrawSheet::views() const
{
    return { Ortho { cell (0, 0), { 0, -1, 0 }, { 1, 0, 0 }, "plan" },
             Ortho { cell (1, 0), { 1, 0, 0 }, { 0, 0, 1 }, "side" },
             Ortho { cell (0, 1), { 0, -1, 0 }, { 0, 0, 1 }, "behind" } };
}
static float orthoScale (const Rectangle<float>& b) { return 0.5f * jmin (b.getWidth(), b.getHeight()) - 10.0f; }
Point<float> DrawSheet::toScreen (const Ortho& o, leitner::Vec3 v)
{
    const auto c = o.box.getCentre(); const float s = orthoScale (o.box);
    return { c.x + v.dot (o.right) * s, c.y - v.dot (o.up) * s };
}
leitner::Vec3 DrawSheet::fromScreen (const Ortho& o, Point<float> q, leitner::Vec3 keep)
{
    const auto c = o.box.getCentre(); const float s = orthoScale (o.box);
    const float a = jlimit (-1.0f, 1.0f, (q.x - c.x) / s), b = jlimit (-1.0f, 1.0f, -(q.y - c.y) / s);
    return keep - o.right * keep.dot (o.right) - o.up * keep.dot (o.up) + o.right * a + o.up * b;
}
int DrawSheet::viewAt (Point<float> q) const
{
    const auto v = views();
    for (int i = 0; i < 3; ++i) if (v[(size_t) i].box.contains (q)) return i;
    return -1;
}
int DrawSheet::pointAt (const Ortho& o, Point<float> q) const
{
    const auto& d = drawing.get();
    int best = -1; float bestD = 8.0f;
    for (int i = d.count - 1; i >= 0; --i)   // (the later drawn on top)
        if (const float dd = toScreen (o, d.pt[i]).getDistanceFrom (q); dd < bestD) { bestD = dd; best = i; }
    if (best < 0 && drawing.picked >= 0 && drawing.picked < d.count && toScreen (o, d.pt[drawing.picked]).getDistanceFrom (q) < 8.0f) best = drawing.picked;
    return best;
}

void DrawSheet::paint (Graphics& g)
{
    using leitner::Vec3;
    const auto& d = drawing.get();
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const auto vs = views();
    const float W = (float) getWidth();
    heading (g, { 0.0f, 0.0f, W, 18.0f }, "zeichnung" + sep + "your own space", false);
    caption (g, cellLabel (0, 0), "grundriss" + sep + "your own, seen from above", false);
    caption (g, cellLabel (1, 0), "aufriss" + sep + "from the side (the right)", false);
    caption (g, cellLabel (0, 1), "from behind, facing front", false);

    // where line 1 is now, back in the drawing's own measure
    const Vec3 now = leitner::roomToDrawn (p, st, { proc.space.view[0].x.load(), proc.space.view[0].y.load(), proc.space.view[0].z.load() });

    for (int vi = 0; vi < 3; ++vi)
    {
        const auto& o = vs[(size_t) vi];
        const auto c = o.box.getCentre(); const float s = orthoScale (o.box);
        auto P = [&] (Vec3 v) { return toScreen (o, v); };
        const Rectangle<float> room (c.x - s, c.y - s, 2.0f * s, 2.0f * s);
        // the paper you draw on: a white face, the room on it, quarters of the room as a faint grid
        g.setColour (Colours::white.withAlpha (0.7f)); g.fillRect (o.box);
        g.setColour (ink); g.drawRect (room, 1.0f);
        g.setColour (rule);
        for (float k : { -0.5f, 0.0f, 0.5f })
        {
            g.fillRect (c.x + k * s, room.getY(), 1.0f, room.getHeight());
            g.fillRect (room.getX(), c.y + k * s, room.getWidth(), 1.0f);
        }
        // the listener
        g.setColour (ink.withAlpha (0.4f));
        if (vi == 0) listenerAbove (g, c, s);
        else         listenerStanding (g, P ({ 0, 0, 0 }), P ({ 0, 0, -1 }), vi == 1);
        if (vi == 0) caption (g, { c.x - 50.0f, room.getY() + 3.0f, 100.0f, 11.0f }, "front" + sep + "vorn", false, faint, 9.0f);
        if (vi == 1) caption (g, { room.getRight() - 54.0f, room.getY() + 3.0f, 50.0f, 11.0f }, "front", true, faint, 9.0f);
        if (vi == 2) { caption (g, { room.getX() + 4.0f, room.getY() + 3.0f, 50.0f, 11.0f }, "left", false, faint, 9.0f);
                       caption (g, { room.getRight() - 54.0f, room.getY() + 3.0f, 50.0f, 11.0f }, "right", true, faint, 9.0f); }

        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (o.box.toNearestInt());
        // guides through the picked point
        if (isPositiveAndBelow (drawing.picked, d.count))
        {
            const auto q = P (d.pt[drawing.picked]);
            g.setColour (faint.withAlpha (0.7f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine ({ room.getX(), q.y, room.getRight(), q.y }, dash, 2, 0.6f);
            g.drawDashedLine ({ q.x, room.getY(), q.x, room.getBottom() }, dash, 2, 0.6f);
        }
        // the path
        Path path;
        for (int k = 0; k <= leitner::Drawn::kTable; k += 2)
        {
            const auto q = P (d.table[k]);
            if (k == 0) path.startNewSubPath (q); else path.lineTo (q);
        }
        g.setColour (ink);
        g.strokePath (path, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        if (d.count >= 2)
        {
            const auto a = P (d.at (0.3f)), b = P (d.at (0.31f));
            if (a.getDistanceFrom (b) > 0.3f) { Path head; head.addArrow ({ a, a + (b - a) * (9.0f / a.getDistanceFrom (b)) }, 0.0f, 8.0f, 9.0f); g.fillPath (head); }
        }
        // your points, as squares, the first marked; the picked one filled
        for (int i = 0; i < d.count; ++i)
        {
            const auto box = Rectangle<float> (8.0f, 8.0f).withCentre (P (d.pt[i]));
            g.setColour (i == drawing.picked ? ink : Colours::white); g.fillRect (box);
            g.setColour (ink); g.drawRect (box, 1.1f);
            if (i == 0) caption (g, { box.getRight() + 2.0f, box.getY() - 9.0f, 14.0f, 11.0f }, "1", false, ink, 9.0f);
        }
        // where line 1 is now
        g.setColour (red);
        g.fillPath (chip (P (now), 8.0f, 5.5f));
        if (stroking && dragView == vi && stroke.size() > 1)
        {
            Path sp; sp.startNewSubPath (stroke[0]);
            for (auto& q : stroke) sp.lineTo (q);
            g.setColour (ink.withAlpha (0.45f)); g.strokePath (sp, PathStrokeType (1.4f));
        }
    }

    // the six, each with its mark: the three that are a choice show which way they stand
    {
        const auto c = cell (1, 1);
        const bool marks[] = { d.closed, d.curved, d.atPoints };
        for (int i = 0; i < 6; ++i)
        {
            const Point<float> at (c.getX() + 7.0f, c.getY() + 8.0f + 21.0f * (float) i);
            if (i < 3) drawChip (g, at, 9.0f, 6.0f, marks[i] ? ink : paper, ink, 0.9f);
            else { g.setColour (ink.withAlpha (i == 5 && ! drawing.canUndo() ? 0.3f : 1.0f)); g.fillRect (at.x - 4.0f, at.y, 8.0f, 1.0f); }
        }
        // which point, and where
        const float y = c.getY() + 136.0f;
        caption (g, { c.getX(), y, c.getWidth(), 14.0f }, String (d.count) + " points, " + (d.closed ? "a closed loop" : "there and back"), false, ink, 11.0f);
        if (isPositiveAndBelow (drawing.picked, d.count))
        {
            const auto v = d.pt[drawing.picked];
            auto pc = [] (float x) { return (x >= 0.0f ? "+" : "") + String (roundToInt (x * 100.0f)); };
            caption (g, { c.getX(), y + 17.0f, c.getWidth(), 14.0f }, "point " + String (drawing.picked + 1) + " of " + String (d.count), false, ink, 11.0f);
            caption (g, { c.getX(), y + 33.0f, c.getWidth(), 14.0f }, "front " + pc (v.x) + sep + "left " + pc (v.y) + sep + "up " + pc (v.z), false, ink, 11.0f);
        }
        else caption (g, { c.getX(), y + 17.0f, c.getWidth(), 14.0f }, "click a point to pick it", false, faint);
        caption (g, { c.getX(), c.getBottom() - 13.0f, c.getWidth(), 13.0f }, "a line of your own, as leitner drew his", false, faint);
    }
}

void DrawSheet::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    const auto q = e.position;
    const int vi = viewAt (q);
    if (vi < 0) return;
    const auto o = views()[(size_t) vi];
    const auto& d = drawing.get();
    if (const int i = pointAt (o, q); i >= 0)
    {
        drawing.picked = i;
        if (e.mods.isPopupMenu()) { drawing.begin(); drawing.removePoint (i); return; }
        drawing.begin();
        dragView = vi; dragPoint = i;
        repaint();
        return;
    }
    if (e.mods.isShiftDown()) { stroking = true; dragView = vi; stroke = { q }; return; }
    // a new point: between two if you clicked on the line, else after the picked one (or at the end)
    int after = isPositiveAndBelow (drawing.picked, d.count) ? drawing.picked : d.count - 1;
    leitner::Vec3 keep = d.count > 0 ? d.pt[jmax (0, after)] : leitner::Vec3 {};
    float bestD = 8.0f;
    const int segs = d.closed ? d.count : d.count - 1;
    for (int i = 0; i < segs; ++i)
    {
        const auto A = d.pt[i], B = d.pt[(i + 1) % d.count];
        const auto a = toScreen (o, A), b = toScreen (o, B);
        const auto ab = b - a; const float L2 = ab.x * ab.x + ab.y * ab.y;
        const float t = L2 > 0.0f ? jlimit (0.0f, 1.0f, ((q - a).x * ab.x + (q - a).y * ab.y) / L2) : 0.0f;
        if (const float dd = q.getDistanceFrom (a + ab * t); dd < bestD) { bestD = dd; after = i; keep = A + (B - A) * t; }
    }
    drawing.begin();
    const int at = drawing.insertPoint (after, fromScreen (o, q, keep));
    if (at >= 0) { drawing.picked = at; dragView = vi; dragPoint = at; }
    repaint();
}
void DrawSheet::mouseDrag (const MouseEvent& e)
{
    if (dragPoint >= 0 && dragView >= 0)
        drawing.movePoint (dragPoint, fromScreen (views()[(size_t) dragView], e.position, drawing.get().pt[dragPoint]));
    else if (stroking && (stroke.empty() || stroke.back().getDistanceFrom (e.position) > 2.5f)) { stroke.push_back (e.position); repaint(); }
}
void DrawSheet::mouseUp (const MouseEvent&)
{
    if (stroking && stroke.size() >= 3 && dragView >= 0)
    {
        const auto o = views()[(size_t) dragView];
        std::vector<leitner::Vec3> pts;
        for (auto q : stroke) pts.push_back (fromScreen (o, q, {}));   // (the third measure: through the middle; move points in the other views)
        drawing.begin();
        drawing.setStroke (pts, true);
    }
    stroking = false; stroke.clear(); dragView = dragPoint = -1;
    repaint();
}
void DrawSheet::mouseDoubleClick (const MouseEvent& e)
{
    const int vi = viewAt (e.position);
    if (vi < 0) return;
    if (const int i = pointAt (views()[(size_t) vi], e.position); i >= 0) { drawing.begin(); drawing.removePoint (i); }
}
bool DrawSheet::keyPressed (const KeyPress& k)
{
    if ((k.getKeyCode() == KeyPress::deleteKey || k.getKeyCode() == KeyPress::backspaceKey) && drawing.picked >= 0) { drawing.begin(); drawing.removePoint (drawing.picked); return true; }
    if (k == KeyPress ('z', ModifierKeys::commandModifier, 0)) { drawing.undo(); return true; }
    return false;
}

//==============================================================================
// The figures as folded paper strips: a flat face that leans, a darker sliver where the paper turns,
// the name along it and a code under it, as on the sleeves faltung is drawn after.
FigureStrip::FigureStrip (LeitnerProcessor& p)
    : att (*p.figure, [this] (float v) { index = roundToInt (v); repaint(); })
{
    att.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}
String FigureStrip::code (int f)
{
    static const char* const c[leitner::numFigures] = { "tl1", "sw2", "lg3", "tt4", "vt5", "kr6", "sc7", "sp8", "tw9", "ev10", "wb11", "pb12", "tk13", "pt14", "ez15" };
    return c[jlimit (0, (int) leitner::numFigures - 1, f)];
}
String FigureStrip::shortName (int f)
{
    static const char* const n[leitner::numFigures] = { "ton-linie", "schwingen", "ton-liege", "ton-tor", "vertikal", "kreis", "soundcube", "spirale", "ton-w\xc3\xbcrfel",
                                                         "erweitern", "w\xc3\xb6lbung", "paraboloid", "terretektorh", "polytope", "zeichnung" };
    return String::fromUTF8 (n[jlimit (0, (int) leitner::numFigures - 1, f)]);
}
namespace
{
constexpr float kLean = 15.0f, kFaceH = 27.0f, kFaceY = 4.0f, kFold = 9.0f;
float slotW (const Component& c) { return (float) c.getWidth() / (float) leitner::numFigures; }
}
Path FigureStrip::face (int i) const
{
    const float slot = slotW (*this), w = slot - 8.0f;
    const Point<float> s ((float) i * slot + 4.0f, kFaceY + kFaceH), D (w - kLean, 0.0f), Wv (kLean, -kFaceH);
    Path f;
    f.startNewSubPath (s); f.lineTo (s + D); f.lineTo (s + D + Wv); f.lineTo (s + Wv); f.closeSubPath();
    return f;
}
int FigureStrip::at (Point<float> q) const
{
    for (int i = 0; i < leitner::numFigures; ++i) if (face (i).contains (q)) return i;
    if (q.y > kFaceY + kFaceH) return jlimit (0, (int) leitner::numFigures - 1, (int) (q.x / slotW (*this)));   // (its code)
    return -1;
}
void FigureStrip::mouseDown (const MouseEvent& e) { if (const int i = at (e.position); i >= 0) att.setValueAsCompleteGesture ((float) i); }
void FigureStrip::mouseMove (const MouseEvent& e) { if (const int i = at (e.position); i != hover) { hover = i; repaint(); } }
void FigureStrip::mouseExit (const MouseEvent&) { if (hover != -1) { hover = -1; repaint(); } }
String FigureStrip::getTooltip()
{
    if (hover < 0) return {};
    return lower (leitner::figureNames()[hover]) + sep + String (leitner::figureEnglish()[hover]).toLowerCase();
}
void FigureStrip::paint (Graphics& g)
{
    // Leitner's eleven in three sands of the paper, Xenakis's three with the red in them, your own on paper
    static const Colour sands[3] = { Colour (0xffe4dfd2), Colour (0xffd9d5cb), Colour (0xffcbc4b4) };
    static const Colour roses[3] = { paper.interpolatedWith (red, 0.20f), paper.interpolatedWith (red, 0.32f), paper.interpolatedWith (red, 0.44f) };
    const float slot = slotW (*this), w = slot - 8.0f;
    for (int i = 0; i < leitner::numFigures; ++i)
    {
        const bool sel = i == index, own = i == leitner::fgDrawn;
        Colour c = own ? paper : i >= leitner::fgParaboloid ? roses[i - leitner::fgParaboloid] : sands[i % 3];
        if (i == hover && ! sel) c = c.interpolatedWith (Colours::white, 0.35f);
        const Point<float> s ((float) i * slot + 4.0f, kFaceY + kFaceH), D (w - kLean, 0.0f), Wv (kLean, -kFaceH);
        const auto f = face (i);
        Path fold;
        fold.startNewSubPath (s + D); fold.lineTo (s + D + Point<float> (0.0f, kFold)); fold.lineTo (s + D + Wv); fold.closeSubPath();
        g.setColour ((own ? sands[1] : c).darker (0.28f)); g.fillPath (fold);
        g.setColour (sel ? Colours::white : c); g.fillPath (f);
        if (sel) { g.setColour (ink); g.strokePath (f, PathStrokeType (1.6f)); }
        else if (own)
        {
            // the one you draw: a dashed edge
            Path dashed; const float dash[] = { 3.0f, 3.0f };
            PathStrokeType (1.0f).createDashedStroke (dashed, f, dash, 2);
            g.setColour (ink.withAlpha (0.7f)); g.fillPath (dashed);
        }
        const auto mid = s + D * 0.5f + Wv * 0.5f;
        g.setColour (ink); g.setFont (ui::sans (12.0f, sel));
        g.drawText (shortName (i), Rectangle<float> (mid.x - w * 0.5f, mid.y - 8.0f, w, 16.0f), Justification::centred, false);
        caption (g, { (float) i * slot, kFaceY + kFaceH + kFold + 3.0f, slot - 8.0f, 11.0f }, code (i), false, sel ? ink : faint, 9.0f);
    }
}

//==============================================================================
// the programs: after Leitner, after Xenakis, yours, and saving
Presets::Presets (LeitnerProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip ("programs and your own presets: the arrows step, the name opens the list");
    seenProgram = proc.getCurrentProgram();
}
void Presets::poll()
{
    if (const int pr = proc.getCurrentProgram(); pr != seenProgram) { seenProgram = pr; userShown.clear(); repaint(); }
}
void Presets::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (ink);
    g.setFont (ui::mono (11.0f));
    g.drawText ("<", b.withWidth (18.0f), Justification::centredLeft);
    g.drawText (">", b.withTrimmedLeft (b.getWidth() - 18.0f), Justification::centredRight);
    g.setFont (ui::mono (11.0f, true));
    g.drawText (userShown.isNotEmpty() ? userShown : proc.getProgramName (proc.getCurrentProgram()).toLowerCase(), b.reduced (20.0f, 0.0f), Justification::centred, true);
    g.setColour (ink.withAlpha (0.35f));
    g.fillRect (b.getX(), b.getBottom() - 1.0f, b.getWidth(), 1.0f);
}
void Presets::mouseDown (const MouseEvent& e)
{
    if (e.position.x < 22.0f) step (-1);
    else if (e.position.x > (float) getWidth() - 22.0f) step (1);
    else menu();
}
void Presets::step (int d)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((jmax (0, proc.getCurrentProgram()) + d + n) % n);
    seenProgram = proc.getCurrentProgram(); userShown.clear();
    repaint();
}
void Presets::menu()
{
    PopupMenu m; String group;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        if (proc.getProgramGroup (i) != group) { group = proc.getProgramGroup (i); m.addSectionHeader ("after " + group.toLowerCase()); }
        m.addItem (i + 1, proc.getProgramName (i).toLowerCase(), true, userShown.isEmpty() && i == proc.getCurrentProgram());
    }
    const auto files = proc.userPresets();
    if (! files.isEmpty()) m.addSectionHeader ("yours");
    for (int i = 0; i < files.size(); ++i) m.addItem (1001 + i, files[i].getFileNameWithoutExtension(), true, files[i].getFileNameWithoutExtension() == userShown);
    m.addSeparator();
    m.addItem (2000, "save this as a preset...");
    m.addItem (2001, "show the presets folder");
    SafePointer<Presets> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe, files] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe; auto& lp = self.proc;
        if (r >= 1 && r <= 1000) { lp.setCurrentProgram (r - 1); self.seenProgram = lp.getCurrentProgram(); self.userShown.clear(); }
        else if (r > 1000 && r < 2000)
        {
            // (a preset carries the program it was made from: that is not a change of program)
            if (lp.loadUserPreset (files[r - 1001])) { self.seenProgram = lp.getCurrentProgram(); self.userShown = files[r - 1001].getFileNameWithoutExtension(); }
        }
        else if (r == 2000)
        {
            lp.userPresetFolder().createDirectory();
            self.chooser = std::make_unique<FileChooser> ("Save this as a preset", lp.userPresetFolder().getChildFile ("my space" + lp.presetExtension()), "*" + lp.presetExtension());
            self.chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                       [safe] (const FileChooser& fc)
                                       {
                                           if (safe == nullptr) return;
                                           const auto f = fc.getResult();
                                           if (f != File() && safe->proc.saveUserPreset (f)) { safe->userShown = f.getFileNameWithoutExtension(); safe->repaint(); }
                                       });
        }
        else if (r == 2001) { lp.userPresetFolder().createDirectory(); lp.userPresetFolder().revealToUser(); }
        self.repaint();
    });
}

} // namespace lt

//==============================================================================
LeitnerEditor::Look::Look()
{
    setColour (PopupMenu::backgroundColourId, lt::paper);
    setColour (PopupMenu::textColourId, lt::ink);
    setColour (PopupMenu::headerTextColourId, lt::faint);
    setColour (PopupMenu::highlightedBackgroundColourId, lt::sand);
    setColour (PopupMenu::highlightedTextColourId, lt::ink);
    setColour (TooltipWindow::backgroundColourId, lt::ink);
    setColour (TooltipWindow::textColourId, lt::paper);
}
void LeitnerEditor::Look::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.fillAll (findColour (TooltipWindow::backgroundColourId));
    g.setColour (findColour (TooltipWindow::textColourId));
    g.setFont (ui::mono (11.0f));
    g.drawFittedText (text, Rectangle<int> (w, h).reduced (8, 4), Justification::centredLeft, 3);
}
Rectangle<int> LeitnerEditor::Look::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> parent)
{
    const int w = jmin (420, GlyphArrangement::getStringWidthInt (ui::mono (11.0f), text) + 18), h = 22;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12, pos.y + 16, w, h).constrainedWithin (parent);
}

LeitnerEditor::LeitnerEditor (LeitnerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    using namespace lt::lay;
    setLookAndFeel (&look);
    addAndMakeVisible (content);

    // the pictures: Leitner's side, the room, Xenakis's side
    room.setBounds (stage());
    plan.setBounds (left());
    drawSheet.setBounds (left());
    chance.setBounds (right());
    strips.setBounds (lt::lay::strips());
    content.addAndMakeVisible (room); content.addAndMakeVisible (plan); content.addAndMakeVisible (chance); content.addChildComponent (drawSheet);
    content.addAndMakeVisible (strips);
    drawing.changed = [this] { drawSheet.refreshButtons(); drawSheet.repaint(); room.repaint(); };

    // the top line: the programs on the left, a random patch on the right
    presets.setBounds (margin, 26, 260, 20);
    randomBtn.setBounds (W - margin - 130, 26, 130, 20);
    randomBtn.align = Justification::centredRight;
    randomBtn.onClick = [this] { proc.randomize(); };
    randomBtn.setTooltip ("a new space: a figure, a raster, and some of xenakis's chance (the order, level and hold stay)");
    content.addAndMakeVisible (presets); content.addAndMakeVisible (randomBtn);

    // the rows. Columns on the right-hand side of the window are read from the right.
    const auto th = lt::theme();
    auto add = [this] (Component* c, Rectangle<int> r) { c->setBounds (r); parts.add (c); content.addAndMakeVisible (c); };
    struct Col
    {
        decltype (add)& put; ui::Theme th; int x, y; bool mirrored;
        void rule (RangedAudioParameter* prm, const String& name) { put (new ui::ParamSlider (*prm, th, name, mirrored), { x, y, colW, rowH - 1 }); y += rowH; }
        void choice (RangedAudioParameter* prm, const String& name) { put (new ui::ChoiceBox (*prm, th, name, mirrored), { x, y, colW, rowH - 1 }); y += rowH; }
    };
    const int cx[4] = { margin, margin + colW + colGap, W - margin - 2 * colW - colGap, W - margin - colW };
    const int y0 = rowsY + 28;
    columns = { { "the line in time", cx[0], false }, { String ("by chance") + String::fromUTF8 ("  \xc2\xb7  ") + "after xenakis", cx[1], false },
                { String ("intensity") + String::fromUTF8 ("  \xc2\xb7  ") + "the room round it", cx[2], true }, { "the line in the room", cx[3], true } };
    {
        Col a { add, th, cx[0], y0, false };
        a.choice (proc.figure, "figure"); a.rule (proc.seconds, "pass"); a.rule (proc.rest, "rest"); a.rule (proc.points, "points"); a.rule (proc.legato, "legato");
        Col b { add, th, cx[1], y0, false };
        b.choice (proc.sieve, "sieve"); b.rule (proc.chance, "chance"); b.rule (proc.walk, "walk"); b.rule (proc.scatter, "scatter");
        Col c { add, th, cx[2], y0, true };
        c.rule (proc.swell, "swell"); c.rule (proc.lift, "lift"); c.rule (proc.room, "room"); c.rule (proc.roomDecay, "decay");
        Col d { add, th, cx[3], y0, true };
        d.rule (proc.size, "size"); d.rule (proc.height, "height"); d.rule (proc.turn, "turn"); d.rule (proc.apart, "apart"); d.rule (proc.counter, "counter");
    }
    // the control line under the strips: level and hold on the left; on the right, mirrored, your own figure and the order
    add (new ui::ParamSlider (*proc.level, th, "level", false), { cx[0], lineY, colW, rowH - 1 });
    {
        auto* hold = new ui::ToggleBox (*proc.hold, lt::switchTheme(), "hold the lines still", false);
        hold->setTooltip ("stop both lines where they are");
        add (hold, { cx[1], lineY, 190, rowH - 1 });
    }
    add (new ui::ChoiceBox (*proc.order, th, "order", true), { cx[3], lineY, colW, rowH - 1 });
    drawBtn.setBounds (cx[2] + colW - 220, lineY, 220, rowH - 1);
    drawBtn.align = Justification::centredRight;
    drawBtn.setTooltip ("the fifteenth figure: your own path, drawn in three dimensions (and back again)");
    drawBtn.onClick = [this]
    {
        // your own figure: to the drawing and back
        const bool mine = proc.figure->getIndex() == leitner::fgDrawn;
        proc.figure->beginChangeGesture();
        proc.figure->setValueNotifyingHost (proc.figure->convertTo0to1 ((float) (mine ? lastFigure : (int) leitner::fgDrawn)));
        proc.figure->endChangeGesture();
    };
    content.addAndMakeVisible (drawBtn);

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, (int) (designW * 1.6), (int) (designH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize ((int) (designW * 0.85), (int) (designH * 0.85));
    timerCallback();
    startTimerHz (30);
}

LeitnerEditor::~LeitnerEditor() { stopTimer(); setLookAndFeel (nullptr); }

void LeitnerEditor::paint (Graphics& g) { g.fillAll (lt::paper); }
void LeitnerEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) designW));
}

void LeitnerEditor::paintContent (Graphics& g)
{
    using namespace lt::lay;
    const String sep (CharPointer_UTF8 ("  \xc2\xb7  "));
    g.fillAll (lt::paper);

    // the title block, in the middle: what it is, and what is really coming out
    g.setColour (lt::ink);
    g.setFont (ui::sans (22.0f, true));
    g.drawText ("leitner", 0, 16, W, 28, Justification::centred);
    g.setFont (ui::mono (11.0f));
    const int o = proc.orderNow.load();
    const String out = o >= 1 ? String (o) + (o == 3 ? "rd" : "th") + " order ambisonics" + sep + String (proc.channelsNow.load()) + " channels" + sep + "acn / sn3d" + sep + "route it to a decoder"
                              : "stereo output: a left-right fold only" + sep + "give the track 16 to 64 channels";
    g.drawText (out, 0, 47, W, 14, Justification::centred);
    g.setColour (lt::faint);
    g.setFont (ui::mono (10.0f));
    g.drawText ("a sound space after bernhard leitner and iannis xenakis", 0, 63, W, 13, Justification::centred);

    // the rule under the pictures, the control line, and the columns' names
    g.setColour (lt::ink);
    g.fillRect ((float) margin, (float) ruleY, (float) (W - 2 * margin), 1.0f);
    g.setColour (lt::ink.withAlpha (0.18f));
    g.fillRect ((float) margin, (float) rowsY - 8.0f, (float) (W - 2 * margin), 1.0f);
    for (auto& c : columns)
    {
        g.setColour (lt::ink); g.setFont (ui::mono (12.0f, true));
        g.drawText (c.title, c.x, rowsY, colW, 18, c.mirrored ? Justification::centredRight : Justification::centredLeft);
        g.setColour (lt::ink.withAlpha (0.18f));
        g.fillRect ((float) c.x, (float) rowsY + 21.0f, (float) colW, 1.0f);
    }

    // along the bottom: what Leitner does, or, while you draw, how
    const bool mine = proc.figure->getIndex() == leitner::fgDrawn;
    g.setColour (lt::ink.withAlpha (0.7f));
    g.setFont (ui::mono (10.0f));
    g.drawText (mine ? "click: a new point (on the line: between two)" + sep + "drag one to move it" + sep + "double-click a point, or delete: take it away" + sep + "cmd-z: undo" + sep
                         + "shift-drag on the paper, or in the room: a whole new path" + sep + "turn the room to reach depth"
                     : "leitner" + sep + "the sound itself is not changed: leitner only says where it is" + sep + "click a strip for its figure" + sep
                         + "rows: drag, shift = fine, double-click = where it started",
                margin, H - 26, W - 2 * margin, 14, Justification::centredLeft);
    // the resize corner, as on the others
    g.setColour (lt::ink.withAlpha (0.5f));
    for (int i = 0; i < 3; ++i) g.drawLine ((float) W - 4.0f - (float) i * 5.0f, (float) H - 2.0f, (float) W - 2.0f, (float) H - 4.0f - (float) i * 5.0f, 1.0f);
}

void LeitnerEditor::timerCallback()
{
    const int fig = proc.figure->getIndex(), out = proc.orderNow.load() * 100 + proc.channelsNow.load();
    const bool mine = fig == leitner::fgDrawn;
    if (! mine) lastFigure = fig;
    if (plan.isVisible() == mine) { plan.setVisible (! mine); drawSheet.setVisible (mine); }
    drawBtn.setText (mine ? "back to " + String::fromUTF8 (leitner::figureNames()[lastFigure]).toLowerCase() : String ("draw your own figure"));
    room.repaint(); chance.poll(); chance.repaint();
    if (mine) { drawSheet.repaint(); drawSheet.refreshButtons(); } else plan.repaint();
    if (fig != seenFigure || out != seenOut) { seenFigure = fig; seenOut = out; content.repaint(); }
    presets.poll();
}
