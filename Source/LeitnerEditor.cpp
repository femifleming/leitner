#include "LeitnerEditor.h"

using namespace juce;

namespace lt
{
const Colour paper (0xfff3f0e8), ink (0xff141414), faint (0xff8c8880), red (0xffc4281c);
// a typewriter face: American Typewriter on macOS, Courier New on Windows, the system's monospace elsewhere
Font typed (float h, bool bold)
{
   #if JUCE_MAC
    return Font (FontOptions ("American Typewriter", h, bold ? Font::bold : Font::plain));
   #elif JUCE_WINDOWS
    return Font (FontOptions ("Courier New", h * 0.98f, Font::bold));
   #else
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), h * 0.92f, bold ? Font::bold : Font::plain));
   #endif
}

//==============================================================================
Rule::Rule (RangedAudioParameter& main, RangedAudioParameter* d, const String& n, bool m)
    : param (main), driftParam (d), att (main, [this] (float v) { value = param.convertTo0to1 (v); repaint(); }), name (n), mirrored (m)
{
    att.sendInitialUpdate();
    if (driftParam != nullptr)
    {
        driftAtt = std::make_unique<ParameterAttachment> (*driftParam, [this] (float v) { drift = v; repaint(); });
        driftAtt->sendInitialUpdate();
    }
}
// name | line | value on the left-hand sheets; value | line | name on the right-hand ones
Rectangle<int> Rule::lineArea() const
{
    auto r = getLocalBounds();
    if (mirrored) { r.removeFromRight (nameW); r.removeFromLeft (valueW); } else { r.removeFromLeft (nameW); r.removeFromRight (valueW); }
    return r.reduced (5, 0);
}
float Rule::along (int x) const
{
    const auto l = lineArea();
    const float t = (float) (x - l.getX()) / (float) l.getWidth();
    return jlimit (0.0f, 1.0f, mirrored ? 1.0f - t : t);
}

void Rule::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto b = getLocalBounds();
    g.drawText (name, mirrored ? b.withLeft (b.getRight() - nameW) : b.withWidth (nameW), mirrored ? Justification::centredRight : Justification::centredLeft);
    g.drawText (param.getCurrentValueAsText(), mirrored ? b.withWidth (valueW) : b.withLeft (b.getRight() - valueW),
                mirrored ? Justification::centredLeft : Justification::centredRight);
    const auto l = lineArea().toFloat();
    const float y = l.getCentreY() + 1.0f;
    auto X = [&] (float t) { return mirrored ? l.getRight() - l.getWidth() * t : l.getX() + l.getWidth() * t; };
    g.drawLine (l.getX(), y, l.getRight(), y, 0.8f);
    for (int i = 0; i <= 4; ++i)   // the scale
        g.drawLine (X ((float) i / 4.0f), y - (i % 4 == 0 ? 4.0f : 2.0f), X ((float) i / 4.0f), y, 0.8f);
    if (driftParam != nullptr && std::abs (drift) > 0.004f)
    {
        // its drift: a red stroke under the line as far as chance may carry it, and a circle where it is now
        g.setColour (red);
        g.drawLine (X (value), y + 4.0f, X (jlimit (0.0f, 1.0f, value + drift)), y + 4.0f, 1.6f);
        if (live != nullptr) { const float x = X (jlimit (0.0f, 1.0f, live->load (std::memory_order_relaxed))); g.drawEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f); }
        g.setColour (ink);
    }
    g.fillRect (Rectangle<float> (X (value) - 1.5f, y - 7.0f, 3.0f, 12.0f));
}
void Rule::setFrom (const MouseEvent& e)
{
    if (dragging == 1) att.setValueAsPartOfGesture (param.convertFrom0to1 (along (e.x)));
    else if (dragging == 2)
    {
        float v = jlimit (-1.0f, 1.0f, driftAtDown + 2.0f * (along (e.x) - downAt));
        if (std::abs (v) < 0.04f) v = 0.0f;
        driftAtt->setValueAsPartOfGesture (v);
    }
}
void Rule::mouseDown (const MouseEvent& e)
{
    dragging = (e.mods.isAltDown() && driftParam != nullptr) ? 2 : 1;
    downAt = along (e.x); driftAtDown = drift;
    if (dragging == 1) { att.beginGesture(); setFrom (e); } else driftAtt->beginGesture();
}
void Rule::mouseDrag (const MouseEvent& e) { setFrom (e); }
void Rule::mouseUp (const MouseEvent&)
{
    if (dragging == 1) att.endGesture();
    if (dragging == 2) driftAtt->endGesture();
    dragging = 0;
}
void Rule::mouseDoubleClick (const MouseEvent& e)
{
    if (e.mods.isAltDown() && driftParam != nullptr) driftAtt->setValueAsCompleteGesture (0.0f);
    else att.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

//==============================================================================
Choice::Choice (RangedAudioParameter& p, const String& n, bool m) : param (p), att (p, [this] (float) { repaint(); }), name (n), mirrored (m) { att.sendInitialUpdate(); }
void Choice::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto b = getLocalBounds();
    g.drawText (name, mirrored ? b.withLeft (b.getRight() - nameW) : b.withWidth (nameW), mirrored ? Justification::centredRight : Justification::centredLeft);
    auto r = mirrored ? b.withTrimmedRight (nameW + 5) : b.withTrimmedLeft (nameW + 5);
    g.drawLine ((float) r.getX(), (float) getHeight() - 3.0f, (float) r.getRight(), (float) getHeight() - 3.0f, 0.8f);
    const float ax = mirrored ? (float) r.getX() + 5.0f : (float) r.getRight() - 5.0f;
    Path t; t.addTriangle (ax - 4.0f, 7.0f, ax + 4.0f, 7.0f, ax, 13.0f);
    g.fillPath (t);
    g.drawText (param.getCurrentValueAsText(), mirrored ? r.withTrimmedLeft (13) : r.withTrimmedRight (13), mirrored ? Justification::centredRight : Justification::centredLeft, true);
}
void Choice::mouseDown (const MouseEvent&)
{
    PopupMenu m;
    const auto names = param.getAllValueStrings();
    const int now = roundToInt (param.convertFrom0to1 (param.getValue()));
    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == now);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) att.setValueAsCompleteGesture ((float) (r - 1)); });
}

Check::Check (RangedAudioParameter& p, const String& n, bool m) : param (p), att (p, [this] (float v) { on = v > 0.5f; repaint(); }), name (n), mirrored (m) { att.sendInitialUpdate(); }
void Check::paint (Graphics& g)
{
    g.setFont (typed (11.0f));
    g.setColour (ink);
    const auto box = Rectangle<float> (mirrored ? (float) getWidth() - 12.0f : 1.0f, (float) getHeight() * 0.5f - 5.5f, 11.0f, 11.0f);
    g.drawRect (box, 0.9f);
    if (on) { g.drawLine ({ box.getTopLeft(), box.getBottomRight() }, 1.4f); g.drawLine ({ box.getTopRight(), box.getBottomLeft() }, 1.4f); }
    if (mirrored) g.drawText (name, 0, 0, getWidth() - 18, getHeight(), Justification::centredRight);
    else          g.drawText (name, 18, 0, getWidth() - 18, getHeight(), Justification::centredLeft);
}
void Check::mouseDown (const MouseEvent&) { att.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

//==============================================================================
// Leitner's sheet. Plan: seen from above, the front at the top. Elevation: seen from the left, the
// front to the right. The room is the box the unit sphere sits in, the listener in the middle.
void Plan::paint (Graphics& g)
{
    using leitner::Vec3;
    const auto p = proc.readSpace();
    const auto st = proc.space.stateNow();
    const float W = (float) getWidth();
    float ws[leitner::kMaxRaster]; int idx[leitner::kMaxRaster];
    const int n = leitner::continuous (p) ? 0 : leitner::rasterPoints (p, ws, idx);

    auto heading = [&] (const String& t, int y)
    {
        g.setColour (ink); g.setFont (typed (11.5f, true));
        g.drawText (t, 0, y, getWidth(), 18, Justification::centredLeft);
        g.drawLine (0.0f, (float) y + 18.0f, W, (float) y + 18.0f, 1.2f);
    };
    // one view: the room, the listener, the two lines, their loudspeakers, where they are now
    auto view = [&] (Rectangle<float> box, bool plan)
    {
        const float s = 0.5f * jmin (box.getWidth(), box.getHeight()) - 10.0f;
        const auto c = box.getCentre();
        auto P = [&] (Vec3 v) { return plan ? Point<float> (c.x - v.y * s, c.y - v.x * s) : Point<float> (c.x + v.x * s, c.y - v.z * s); };
        g.setColour (ink);
        g.drawRect (Rectangle<float> (c.x - s, c.y - s, 2.0f * s, 2.0f * s), 1.0f);
        // the loudspeakers in the walls, five a side
        g.setColour (ink.withAlpha (0.7f));
        for (int i = 0; i < 5; ++i)
        {
            const float a = -1.0f + (2.0f * (float) i + 1.0f) / 5.0f;
            for (auto q : { Point<float> (c.x + a * s, c.y - s), Point<float> (c.x + a * s, c.y + s), Point<float> (c.x - s, c.y + a * s), Point<float> (c.x + s, c.y + a * s) })
                g.drawEllipse (q.x - 2.5f, q.y - 2.5f, 5.0f, 5.0f, 0.7f);
        }
        // the listener: from above a head and shoulders facing the front; from the side a standing figure
        g.setColour (ink);
        if (plan)
        {
            g.fillEllipse (c.x - 0.24f * s, c.y - 0.06f * s, 0.48f * s, 0.15f * s);
            g.fillEllipse (c.x - 0.07f * s, c.y - 0.09f * s, 0.14f * s, 0.14f * s);
            g.drawLine (c.x, c.y - 0.09f * s, c.x, c.y - 0.15f * s, 1.4f);
            g.setFont (typed (9.5f));
            g.drawText ("front  vorn", Rectangle<float> (c.x - 50.0f, c.y - s - 15.0f, 100.0f, 12.0f), Justification::centred);
        }
        else
        {
            const auto head = P ({ 0, 0, 0 }), feet = P ({ 0, 0, -1 });
            const float h = feet.y - head.y, w = h * 0.2f;
            g.fillEllipse (head.x - w * 0.36f, head.y - h * 0.09f, w * 0.72f, h * 0.17f);
            g.fillRoundedRectangle (head.x - w * 0.5f, head.y + h * 0.11f, w, h * 0.42f, w * 0.3f);
            g.fillRect (head.x - w * 0.3f, head.y + h * 0.5f, w * 0.6f, h * 0.5f);
            g.drawLine (head.x + w * 0.5f, head.y, head.x + w * 0.95f, head.y, 1.4f);   // looking to the front
            g.setFont (typed (9.5f));
            g.drawText ("front  vorn", Rectangle<float> (c.x + s - 70.0f, c.y - s - 15.0f, 70.0f, 12.0f), Justification::centredRight);
            g.drawText ("floor", Rectangle<float> (c.x - s, c.y + s + 2.0f, 60.0f, 12.0f), Justification::centredLeft);
        }
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (box.toNearestInt());
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
            g.strokePath (path, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
            for (int k = 0; k < n; ++k)
            {
                const auto q = P (leitner::linePoint (li, p, st, ws[k], idx[k]));
                g.setColour (paper); g.fillEllipse (q.x - 4.0f, q.y - 4.0f, 8.0f, 8.0f);
                g.setColour (col);   g.drawEllipse (q.x - 4.0f, q.y - 4.0f, 8.0f, 8.0f, 1.1f);
            }
            auto& v = proc.space.view[li];
            for (auto [x, y, z, gain] : { std::tuple { v.ax.load(), v.ay.load(), v.az.load(), v.ga.load() }, std::tuple { v.bx.load(), v.by.load(), v.bz.load(), v.gb.load() } })
            {
                if (gain < 0.02f) continue;
                const auto q = P ({ x, y, z });
                const float r = 4.0f + 4.0f * gain;
                g.setColour (col.withAlpha (jlimit (0.0f, 1.0f, gain)));
                g.fillEllipse (q.x - r, q.y - r, 2.0f * r, 2.0f * r);
            }
        }
    };

    heading ("GRUNDRISS  xx  PLAN, SEEN FROM ABOVE", 0);
    view ({ 0.0f, 22.0f, W, 318.0f }, true);
    heading ("AUFRISS  xx  ELEVATION, SEEN FROM THE SIDE", 352);
    view ({ 0.0f, 374.0f, W, 222.0f }, false);

    // the sheet's typed caption
    g.setColour (ink);
    g.setFont (typed (11.0f));
    const int fig = p.figure;
    const String pts = p.points >= leitner::kMaxPoints ? String ("a continuous line") : String (n) + " loudspeakers";
    const String lines[] = {
        String::fromUTF8 (leitner::figureNames()[fig]) + "  -  " + leitner::figureEnglish()[fig],
        pts + ", legato " + String (roundToInt (p.legato * 100.0f)) + " %, one pass " + String (p.seconds, 2) + " s",
        "sound as a building material: a sound sent from loudspeaker",
        "to loudspeaker draws a line, and lines make a space",
    };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i < 2 ? ink : faint);
        g.drawText (lines[i], 0, 610 + i * 16, getWidth(), 15, Justification::centredLeft);
    }
}

// Xenakis's sheet, right to left
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
    auto heading = [&] (const String& t, int y)
    {
        g.setColour (ink); g.setFont (typed (11.5f, true));
        g.drawText (t, 0, y, getWidth(), 18, Justification::centredRight);
        g.drawLine (0.0f, (float) y + 18.0f, W, (float) y + 18.0f, 1.2f);
    };
    auto caption = [&] (const String& t, int y, bool strong = true)
    {
        g.setColour (strong ? ink : faint); g.setFont (typed (11.0f));
        g.drawText (t, 0, y, getWidth(), 15, Justification::centredRight);
    };

    // the sieve: one circle per loudspeaker of the raster, filled where it sounds, the first on the right
    heading ("SIEB  xx  SIEVE: WHICH LOUDSPEAKERS SOUND", 0);
    {
        const int K = jlimit (2, leitner::kMaxPoints - 1, p.points >= leitner::kMaxPoints ? 32 : p.points);
        const float step = (W - 16.0f) / (float) jmax (1, K - 1);
        int sounding = 0;
        for (int k = 0; k < K; ++k)
        {
            const float x = W - 8.0f - step * (float) k, y = 46.0f;
            const bool on = leitner::inSieve (p.sieve, k);
            sounding += on ? 1 : 0;
            g.setColour (ink);
            if (on) g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f); else g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
            if (K <= 16 || k % 4 == 0)
            {
                g.setFont (typed (9.0f)); g.setColour (faint);
                g.drawText (String (k), Rectangle<float> (x - 10.0f, y + 7.0f, 20.0f, 11.0f), Justification::centred);
            }
        }
        caption (String (leitner::sieveNames()[p.sieve]) + (p.sieve == 0 ? "" : "  (unions of residue classes)") + ":  " + String (sounding) + " of " + String (K) + " sound", 70);
    }

    // chance: the length of each of the last passes, the newest on the left
    heading ("ZUFALL  xx  CHANCE: THE LENGTH OF EACH PASS", 102);
    {
        const Rectangle<float> area (0.0f, 128.0f, W, 170.0f);
        g.setColour (ink);
        g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getBottom(), 1.0f);
        const float unit = area.getHeight() / 4.0f;                    // bars up to four times the pass
        const float y1 = area.getBottom() - unit;
        for (float x = 0.0f; x < W; x += 8.0f) g.drawLine (x, y1, jmin (W, x + 4.0f), y1, 0.7f);
        if (history.empty()) { g.setFont (typed (9.5f)); g.setColour (faint); g.drawText ("(waiting for a pass to end)", area.toNearestInt(), Justification::centred); }
        const float bw = W / 24.0f;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const size_t age = history.size() - 1 - i;                 // 0: the newest
            const float x = (float) age * bw;
            const float h = jmin (area.getHeight(), history[i].length * unit);
            g.setColour (age == 0 ? red : ink);
            g.fillRect (Rectangle<float> (x + 2.0f, area.getBottom() - h, bw - 4.0f, h));
        }
        {   // (drawn over the bars, on paper)
            const Rectangle<float> label (W - 96.0f, y1 - 15.0f, 94.0f, 13.0f);
            g.setColour (paper); g.fillRect (label);
            g.setFont (typed (9.5f)); g.setColour (faint);
            g.drawText ("the pass as set", label, Justification::centredRight);
        }
        caption (p.chance <= 0.0f ? String ("chance 0 %: every pass the same length")
                                  : "chance " + String (roundToInt (p.chance * 100.0f)) + " %: lengths by an exponential law, as in his stochastic music", 304);
    }

    // the walk: where the steps have taken the figure, turn across and height up, the trail of the last passes
    heading ("IRRFAHRT  xx  WALK: WHERE CHANCE HAS STEPPED", 336);
    {
        const Rectangle<float> area (0.0f, 362.0f, W, 232.0f);
        g.setColour (ink);
        g.drawRect (area, 1.0f);
        g.setColour (faint);
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 0.7f);
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 0.7f);
        g.setFont (typed (9.5f));
        g.drawText ("turned left", area.reduced (4.0f).toNearestInt(), Justification::centredLeft);
        g.drawText ("turned right", area.reduced (4.0f).toNearestInt(), Justification::centredRight);
        g.drawText ("higher", area.reduced (4.0f).toNearestInt(), Justification::centredTop);
        g.drawText ("lower", area.reduced (4.0f).toNearestInt(), Justification::centredBottom);
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
        g.strokePath (trail, PathStrokeType (1.2f));
        for (size_t i = 0; i < history.size(); ++i)
        {
            const auto q = at (history[i].turn, history[i].height);
            g.drawEllipse (q.x - 3.0f, q.y - 3.0f, 6.0f, 6.0f, 1.0f);
        }
        const auto st = proc.space.stateNow();
        const auto now = at (st.dTurn, st.dHeight);
        g.setColour (red);
        g.fillEllipse (now.x - 5.0f, now.y - 5.0f, 10.0f, 10.0f);
        caption (p.walk <= 0.0f ? String ("walk 0 %: the figure stays where it is set")
                                : "walk " + String (roundToInt (p.walk * 100.0f)) + " %: a Gaussian step every pass, pulled gently home", 600);
    }
    caption (p.scatter <= 0.0f ? String ("scatter 0 %: every loudspeaker on the line")
                               : "scatter " + String (roundToInt (p.scatter * 100.0f)) + " %: each loudspeaker thrown off the line, a cloud of points", 626);
    caption ("he filled rooms with sound by calculation and chance", 650, false);
}

//==============================================================================
// The room is two units across (the unit sphere's box) and you stand at the middle of it. The camera
// looks at the middle from 2.64 away: from straight behind you to begin with (the back wall then
// comes out 0.45 of the opening, as on the sheets), and from wherever you drag it to.
constexpr float kCamera = 2.64f;
Room::View Room::view() const
{
    const float yaw = proc.viewYaw.load() * leitner::kPi / 180.0f, pitch = proc.viewPitch.load() * leitner::kPi / 180.0f;
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    return { { cp * cy, cp * sy, -sp }, { sy, -cy, 0.0f }, { sp * cy, sp * sy, cp } };
}
Point<float> Room::project (const View& v, leitner::Vec3 p, float* scale) const
{
    constexpr float D = kCamera;
    const float depth = jmax (0.35f, p.dot (v.f) + D);
    const float s = (D - 1.0f) / depth;                      // 1 at the opening of the front view
    const float H = (float) getHeight() * 0.47f;
    if (scale != nullptr) *scale = s;
    return { (float) getWidth() * 0.5f + p.dot (v.r) * H * s, (float) getHeight() * 0.5f - p.dot (v.u) * H * s };
}
bool Room::editing() const { return proc.figure->getIndex() == leitner::fgDrawn; }
// a point on the screen, on the plane through the middle of the room that faces you
leitner::Vec3 Room::onMiddlePlane (const View& v, Point<float> q) const
{
    const float H = (float) getHeight() * 0.47f, s0 = (kCamera - 1.0f) / kCamera;
    return v.r * ((q.x - (float) getWidth() * 0.5f) / (H * s0)) + v.u * (-(q.y - (float) getHeight() * 0.5f) / (H * s0));
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
        const float k = (float) getHeight() * 0.47f * s;
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
    g.setColour (ink);
    g.drawRect (getLocalBounds().toFloat(), 1.2f);
    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // the cube's twelve edges
    for (int a = 0; a < 3; ++a)
        for (float s : { -1.0f, 1.0f }) for (float t : { -1.0f, 1.0f })
        {
            Vec3 from, to;
            if (a == 0) { from = { -1, s, t }; to = { 1, s, t }; } else if (a == 1) { from = { s, -1, t }; to = { s, 1, t }; } else { from = { s, t, -1 }; to = { s, t, 1 }; }
            g.drawLine ({ P (from), P (to) }, 1.0f);
        }

    // the loudspeakers: small open circles in a grid on every face, each drawn as it lies on its wall.
    // A wall between you and the room is drawn faint so you can see in through it.
    constexpr int N = 5;
    for (int face = 0; face < 6; ++face)
    {
        const int axis = face / 2; const float side = face % 2 == 0 ? -1.0f : 1.0f;
        Vec3 n {}, t1 {}, t2 {};
        if (axis == 0) { n = { side, 0, 0 }; t1 = { 0, 1, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 1) { n = { 0, side, 0 }; t1 = { 1, 0, 0 }; t2 = { 0, 0, 1 }; }
        if (axis == 2) { n = { 0, 0, side }; t1 = { 1, 0, 0 }; t2 = { 0, 1, 0 }; }
        const bool nearSide = n.dot (V.f) < -0.25f;
        g.setColour (ink.withAlpha (nearSide ? 0.16f : 0.75f));
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
            {
                const float a = -1.0f + (2.0f * (float) i + 1.0f) / (float) N, b = -1.0f + (2.0f * (float) j + 1.0f) / (float) N;
                const Vec3 c = n + t1 * a + t2 * b;
                const auto pc = P (c), pa = P (c + t1 * 0.05f), pb = P (c + t2 * 0.05f);
                Path e; e.addEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
                g.strokePath (e, PathStrokeType (0.7f), AffineTransform (pa.x - pc.x, pb.x - pc.x, pc.x, pa.y - pc.y, pb.y - pc.y, pc.y));
            }
    }

    // which way is the front: an arrow on the floor
    {
        g.setColour (ink.withAlpha (0.8f));
        Path arrow; arrow.addArrow ({ P ({ 0.45f, 0, -1 }), P ({ 0.9f, 0, -1 }) }, 1.2f, 8.0f, 9.0f);
        g.fillPath (arrow);
        g.setFont (typed (9.5f));   // (and a word over the front wall, where the figure never hides it)
        g.drawText ("front  vorn", Rectangle<float> (90.0f, 12.0f).withCentre (P ({ 1.0f, 0, 1.0f }).translated (0.0f, -8.0f)), Justification::centred);
    }

    // the figure: solid black, standing in the middle, ears at the centre of the room
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

    // the two lines of sound: heavy, with arrows; the loudspeakers each is built from; where it is now
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
        g.strokePath (path, PathStrokeType (2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        for (float w : { 0.3f, 0.7f })
        {
            const auto a = P (leitner::linePoint (li, p, st, w)), b = P (leitner::linePoint (li, p, st, w + 0.012f));
            if (a.getDistanceFrom (b) < 0.5f) continue;
            Path head; head.addArrow ({ a, a + (b - a) * (11.0f / a.getDistanceFrom (b)) }, 0.0f, 10.0f, 11.0f);
            g.fillPath (head);
        }
        for (int k = 0; k < n; ++k)
        {
            float s; const auto c = project (V, leitner::linePoint (li, p, st, ws[k], idx[k]), &s);
            const float r = 6.0f * s;
            g.setColour (paper); g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
            g.setColour (col);   g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.3f);
        }
        auto& v = proc.space.view[li];
        const float lev = jlimit (0.0f, 1.0f, 0.35f + 4.0f * v.level.load());
        auto sounding = [&] (float x, float y, float z, float gain)
        {
            if (gain < 0.02f) return;
            float s; const auto c = project (V, { x, y, z }, &s);
            const float r = (6.0f + 7.0f * lev * gain) * s;
            g.setColour (col.withAlpha (jlimit (0.0f, 1.0f, gain)));
            g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
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
            g.setColour (i == drawing.picked ? ink : paper); g.fillRect (box);
            g.setColour (ink); g.drawRect (box, 1.1f);
        }
        if (strokeScreen.size() > 1)
        {
            Path sp; sp.startNewSubPath (strokeScreen[0]);
            for (auto& q : strokeScreen) sp.lineTo (q);
            g.setColour (ink.withAlpha (0.5f)); g.strokePath (sp, PathStrokeType (1.6f));
        }
    }
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
DrawSheet::DrawSheet (LeitnerProcessor& p, Drawing& d) : proc (p), drawing (d)
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &closedBtn, &curvedBtn, &atPointsBtn, &fromBtn, &clearBtn, &undoBtn }) addAndMakeVisible (b);
    auto toggle = [this] (auto change) { return [this, change] { drawing.begin(); auto dd = drawing.get(); change (dd); drawing.set (dd); refreshButtons(); }; };
    closedBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.closed = ! dd.closed; });
    curvedBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.curved = ! dd.curved; });
    atPointsBtn.onClick = toggle ([] (leitner::Drawn& dd) { dd.atPoints = ! dd.atPoints; });
    clearBtn.onClick = [this]
    {
        drawing.begin();
        leitner::Drawn dd; dd.count = 2; dd.closed = false; dd.curved = true;
        dd.pt[0] = { -0.8f, 0.0f, 0.0f }; dd.pt[1] = { 0.8f, 0.0f, 0.0f };
        drawing.picked = 1;
        drawing.set (dd);
    };
    undoBtn.onClick = [this] { drawing.undo(); };
    fromBtn.setButtonText ("start from a figure...");
    fromBtn.onClick = [this]
    {
        PopupMenu m;
        m.addSectionHeader ("start again from");
        for (int f = 0; f < leitner::fgDrawn; ++f) m.addItem (f + 1, String::fromUTF8 (leitner::figureNames()[f]) + "  -  " + leitner::figureEnglish()[f]);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (fromBtn), [this] (int r) { if (r > 0) { drawing.begin(); drawing.fromFigure (r - 1); } });
    };
    refreshButtons();
}

void DrawSheet::refreshButtons()
{
    const auto& d = drawing.get();
    auto put = [] (TextButton& b, const String& t) { if (b.getButtonText() != t) b.setButtonText (t); };
    put (closedBtn, d.closed ? "a closed loop" : "open: there and back");
    put (curvedBtn, d.curved ? "curved through them" : "straight lines");
    put (atPointsBtn, d.atPoints ? "speakers at my points" : "speakers by 'points'");
    put (clearBtn, "start again");
    put (undoBtn, "undo");
    undoBtn.setEnabled (drawing.canUndo());
}

void DrawSheet::resized()
{
    const int w = (getWidth() - 16) / 3;
    closedBtn.setBounds (0, 566, w, 20); curvedBtn.setBounds (w + 8, 566, w, 20); atPointsBtn.setBounds (2 * w + 16, 566, w, 20);
    fromBtn.setBounds (0, 592, w, 20); clearBtn.setBounds (w + 8, 592, w, 20); undoBtn.setBounds (2 * w + 16, 592, w, 20);
}

// from above (the front at the top), from the right-hand side (the front to the right), from behind
// (as the room is first seen: your left on the left)
std::array<DrawSheet::Ortho, 3> DrawSheet::views() const
{
    const float W = (float) getWidth(), half = (W - 12.0f) * 0.5f;
    return { Ortho { { 0.0f, 22.0f, W, 282.0f }, { 0, -1, 0 }, { 1, 0, 0 }, "plan" },
             Ortho { { 0.0f, 338.0f, half, 212.0f }, { 1, 0, 0 }, { 0, 0, 1 }, "side" },
             Ortho { { W - half, 338.0f, half, 212.0f }, { 0, -1, 0 }, { 0, 0, 1 }, "behind" } };
}
static float orthoScale (const Rectangle<float>& b) { return 0.5f * jmin (b.getWidth(), b.getHeight()) - 12.0f; }
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
    auto heading = [&] (const String& t, int y)
    {
        g.setColour (ink); g.setFont (typed (11.5f, true));
        g.drawText (t, 0, y, getWidth(), 18, Justification::centredLeft);
        g.drawLine (0.0f, (float) y + 18.0f, (float) getWidth(), (float) y + 18.0f, 1.2f);
    };
    heading ("GRUNDRISS  xx  YOUR OWN, SEEN FROM ABOVE", 0);
    heading ("AUFRISS  xx  FROM THE SIDE", 316);
    g.drawText ("FROM BEHIND", 0, 316, getWidth(), 18, Justification::centredRight);

    // where line 1 is now, back in the drawing's own measure
    const Vec3 now = leitner::roomToDrawn (p, st, { proc.space.view[0].x.load(), proc.space.view[0].y.load(), proc.space.view[0].z.load() });

    for (int vi = 0; vi < 3; ++vi)
    {
        const auto& o = vs[(size_t) vi];
        const auto c = o.box.getCentre(); const float s = orthoScale (o.box);
        auto P = [&] (Vec3 v) { return toScreen (o, v); };
        const Rectangle<float> room (c.x - s, c.y - s, 2.0f * s, 2.0f * s);
        g.setColour (ink); g.drawRect (room, 1.0f);
        // a faint grid, quarters of the room
        g.setColour (faint.withAlpha (0.35f));
        for (float k : { -0.5f, 0.0f, 0.5f })
        {
            g.drawLine (c.x + k * s, room.getY(), c.x + k * s, room.getBottom(), k == 0.0f ? 0.8f : 0.5f);
            g.drawLine (room.getX(), c.y + k * s, room.getRight(), c.y + k * s, k == 0.0f ? 0.8f : 0.5f);
        }
        // the listener
        g.setColour (ink.withAlpha (0.55f));
        if (vi == 0)
        {
            g.fillEllipse (c.x - 0.24f * s, c.y - 0.06f * s, 0.48f * s, 0.15f * s);
            g.fillEllipse (c.x - 0.07f * s, c.y - 0.09f * s, 0.14f * s, 0.14f * s);
        }
        else
        {
            const auto head = P ({ 0, 0, 0 }), feet = P ({ 0, 0, -1 });
            const float h = feet.y - head.y, w = h * 0.2f;
            g.fillEllipse (head.x - w * 0.36f, head.y - h * 0.09f, w * 0.72f, h * 0.17f);
            g.fillRoundedRectangle (head.x - w * 0.5f, head.y + h * 0.11f, w, h * 0.42f, w * 0.3f);
            g.fillRect (head.x - w * 0.3f, head.y + h * 0.5f, w * 0.6f, h * 0.5f);
            if (vi == 1) g.drawLine (head.x + w * 0.5f, head.y, head.x + w * 0.95f, head.y, 1.4f);
        }
        g.setColour (ink); g.setFont (typed (9.5f));
        if (vi == 0) g.drawText ("front  vorn", Rectangle<float> (c.x - 50.0f, room.getY() - 13.0f, 100.0f, 12.0f), Justification::centred);
        if (vi == 1) { g.drawText ("front", Rectangle<float> (room.getRight() - 52.0f, room.getY() + 2.0f, 50.0f, 12.0f), Justification::centredRight);
                       g.drawText ("from the right", Rectangle<float> (room.getX(), room.getBottom() + 1.0f, 120.0f, 12.0f), Justification::centredLeft); }
        if (vi == 2) { g.drawText ("left", Rectangle<float> (room.getX() + 3.0f, room.getY() + 2.0f, 50.0f, 12.0f), Justification::centredLeft);
                       g.drawText ("right", Rectangle<float> (room.getRight() - 53.0f, room.getY() + 2.0f, 50.0f, 12.0f), Justification::centredRight);
                       g.drawText ("from behind, facing front", Rectangle<float> (room.getRight() - 160.0f, room.getBottom() + 1.0f, 160.0f, 12.0f), Justification::centredRight); }

        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (o.box.toNearestInt());
        // guides through the picked point
        if (isPositiveAndBelow (drawing.picked, d.count))
        {
            const auto q = P (d.pt[drawing.picked]);
            g.setColour (faint.withAlpha (0.6f));
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
        g.strokePath (path, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        if (d.count >= 2)
        {
            const auto a = P (d.at (0.3f)), b = P (d.at (0.31f));
            if (a.getDistanceFrom (b) > 0.3f) { Path head; head.addArrow ({ a, a + (b - a) * (10.0f / a.getDistanceFrom (b)) }, 0.0f, 9.0f, 10.0f); g.fillPath (head); }
        }
        // your points, the first marked; the picked one filled
        g.setFont (typed (9.0f));
        for (int i = 0; i < d.count; ++i)
        {
            const auto q = P (d.pt[i]);
            const bool pk = i == drawing.picked;
            g.setColour (paper); g.fillEllipse (q.x - 5.0f, q.y - 5.0f, 10.0f, 10.0f);
            g.setColour (ink);
            if (pk) g.fillEllipse (q.x - 5.0f, q.y - 5.0f, 10.0f, 10.0f); else g.drawEllipse (q.x - 4.5f, q.y - 4.5f, 9.0f, 9.0f, 1.2f);
            if (i == 0) g.drawText ("1", Rectangle<float> (q.x + 5.0f, q.y - 13.0f, 14.0f, 11.0f), Justification::centredLeft);
        }
        // where line 1 is now
        {
            const auto q = P (now);
            g.setColour (red); g.fillEllipse (q.x - 3.5f, q.y - 3.5f, 7.0f, 7.0f);
        }
        if (stroking && dragView == vi && stroke.size() > 1)
        {
            Path sp; sp.startNewSubPath (stroke[0]);
            for (auto& q : stroke) sp.lineTo (q);
            g.setColour (ink.withAlpha (0.45f)); g.strokePath (sp, PathStrokeType (1.4f));
        }
    }

    // the caption: which point, and how to draw
    g.setFont (typed (11.0f));
    g.setColour (ink);
    String what = String (d.count) + " points, " + (d.closed ? "a closed loop" : "there and back");
    if (isPositiveAndBelow (drawing.picked, d.count))
    {
        const auto v = d.pt[drawing.picked];
        auto pc = [] (float x) { return (x >= 0.0f ? "+" : "") + String (roundToInt (x * 100.0f)); };
        what << "  xx  point " << (drawing.picked + 1) << ": front " << pc (v.x) << ", left " << pc (v.y) << ", up " << pc (v.z);
    }
    g.drawText (what, 0, 620, getWidth(), 15, Justification::centredLeft);
    g.setColour (faint); g.setFont (typed (10.0f));
    const char* help[] = { "click: a new point (on the line: between two)  -  drag one to move it",
                           "double-click a point, or delete: take it away  -  cmd-Z: undo",
                           "shift-drag on the paper, or in the room: draw a whole new path",
                           "in the room: drag a point across your view of it; turn the room to reach depth" };
    for (int i = 0; i < 4; ++i) g.drawText (help[i], 0, 638 + i * 13, getWidth(), 13, Justification::centredLeft);
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

} // namespace lt

//==============================================================================
namespace
{
constexpr int kRow = 20;
const Rectangle<int> sheetL (12, 12, 464, 766), sheetC (488, 12, 524, 766), sheetR (1024, 12, 464, 766);
}

Font LeitnerEditor::Look::getTextButtonFont (TextButton&, int) { return lt::typed (11.0f); }
Font LeitnerEditor::Look::getPopupMenuFont() { return lt::typed (13.0f); }
void LeitnerEditor::Look::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    g.setColour (down ? lt::ink.withAlpha (0.15f) : over ? lt::ink.withAlpha (0.06f) : lt::paper);
    g.fillRect (b.getLocalBounds());
    g.setColour (lt::ink);
    g.drawRect (b.getLocalBounds().toFloat(), 0.9f);
}

LeitnerEditor::LeitnerEditor (LeitnerProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    look.setColour (PopupMenu::backgroundColourId, lt::paper);
    look.setColour (PopupMenu::textColourId, lt::ink);
    look.setColour (PopupMenu::headerTextColourId, lt::faint);
    look.setColour (PopupMenu::highlightedBackgroundColourId, lt::ink);
    look.setColour (PopupMenu::highlightedTextColourId, lt::paper);
    look.setColour (TextButton::textColourOffId, lt::ink);
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.addAndMakeVisible (room);
    room.setBounds (502, 86, 496, 318);

    // one column of controls. Columns on the right-hand side of the window are drawn reversed.
    struct Column
    {
        LeitnerEditor& ed; int x, y, w; bool mirrored;
        void add (Component* c, int h = kRow) { c->setBounds (x, y, w, h); ed.parts.add (c); ed.content.addAndMakeVisible (c); y += h; }
        void rule (RangedAudioParameter* prm, const String& name) { add (new lt::Rule (*prm, nullptr, name, mirrored)); }
        void choice (RangedAudioParameter* prm, const String& name, int nameW = 66) { auto* c = new lt::Choice (*prm, name, mirrored); c->nameW = nameW; add (c); }
        void check (RangedAudioParameter* prm, const String& name) { add (new lt::Check (*prm, name, mirrored)); }
        void gap() { y += 14; }
    };
    // the side sheets: after Leitner on the left, after Xenakis on the right
    plan.setBounds (26, 74, 436, 676);
    drawSheet.setBounds (plan.getBounds());
    chance.setBounds (designW - 26 - 436, 74, 436, 676);
    content.addAndMakeVisible (plan); content.addAndMakeVisible (chance); content.addChildComponent (drawSheet);
    drawing.changed = [this] { drawSheet.refreshButtons(); drawSheet.repaint(); room.repaint(); };

    // the middle sheet, under the room: the space, in two facing columns
    {
        Column a { *this, 506, 474, 238, false };
        a.choice (proc.figure, "figure");
        a.rule (proc.seconds, "pass"); a.rule (proc.rest, "rest"); a.rule (proc.points, "points"); a.rule (proc.legato, "legato");
        a.choice (proc.sieve, "sieve");
        a.rule (proc.chance, "chance"); a.rule (proc.walk, "walk"); a.rule (proc.scatter, "scatter");
        a.rule (proc.level, "level");
        Column b { *this, 756, 474, 238, true };
        b.rule (proc.size, "size"); b.rule (proc.height, "height"); b.rule (proc.turn, "turn"); b.rule (proc.apart, "apart"); b.rule (proc.counter, "counter");
        b.rule (proc.swell, "swell"); b.rule (proc.lift, "lift"); b.rule (proc.room, "room"); b.rule (proc.roomDecay, "decay");
        b.choice (proc.order, "order");
        auto* hold = new lt::Check (*proc.hold, "hold the lines still", false);
        hold->setBounds (556, 681, 170, kRow);
        parts.add (hold); content.addAndMakeVisible (hold);
        // your own figure: to the drawing and back
        drawBtn.setBounds (774, 680, 170, kRow + 2);
        drawBtn.onClick = [this]
        {
            const bool mine = proc.figure->getIndex() == leitner::fgDrawn;
            proc.figure->beginChangeGesture();
            proc.figure->setValueNotifyingHost (proc.figure->convertTo0to1 ((float) (mine ? lastFigure : (int) leitner::fgDrawn)));
            proc.figure->endChangeGesture();
        };
        content.addAndMakeVisible (drawBtn);

        presetBtn.setBounds (506, 738, 238, kRow + 2);
        presetBtn.onClick = [this] { programMenu(); };
        randomBtn.setButtonText ("a random patch");
        randomBtn.setBounds (756, 738, 238, kRow + 2);
        randomBtn.onClick = [this] { proc.randomize(); };
        content.addAndMakeVisible (presetBtn); content.addAndMakeVisible (randomBtn);
    }

    setResizable (true, true);
    setResizeLimits (designW / 2, designH / 2, (int) (designW * 1.6), (int) (designH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / designH);
    setSize ((int) (designW * 0.85), (int) (designH * 0.85));
    timerCallback();
    startTimerHz (30);
}

LeitnerEditor::~LeitnerEditor() { stopTimer(); setLookAndFeel (nullptr); }

void LeitnerEditor::paint (Graphics& g) { g.fillAll (Colour (0xffd9d5cb)); }
void LeitnerEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (AffineTransform::scale ((float) getWidth() / (float) designW));
}

void LeitnerEditor::paintSheets (Graphics& g)
{
    using lt::typed;
    for (auto sheet : { sheetL, sheetC, sheetR })
    {
        g.setColour (Colours::black.withAlpha (0.16f)); g.fillRect (sheet.translated (0, 4).expanded (1, 0));
        g.setColour (lt::paper); g.fillRect (sheet);
    }
    // ring-binder holes down the outer edge of each side sheet
    g.setColour (Colour (0xffd9d5cb));
    for (int i = 0; i < 26; ++i)
    {
        g.fillEllipse (15.5f, 28.0f + (float) i * 28.6f, 7.0f, 7.0f);
        g.fillEllipse ((float) designW - 22.5f, 28.0f + (float) i * 28.6f, 7.0f, 7.0f);
    }

    // the middle sheet
    const auto mid = sheetC.reduced (14, 0);
    g.setColour (lt::ink);
    g.setFont (typed (21.0f));
    g.drawText ("L  E  I  T  N  E  R", mid.getX(), 22, mid.getWidth(), 26, Justification::centred);
    g.setFont (typed (10.5f));
    const int o = proc.orderNow.load();
    const String out = o >= 1 ? String (o) + (o == 3 ? "rd" : "th") + " order Ambisonics, " + String (proc.channelsNow.load()) + " channels, ACN / SN3D: route it to a decoder"
                              : String ("stereo output: a left-right fold only. Give the track 16 to 64 channels.");
    g.drawText (out, mid.getX(), 50, mid.getWidth(), 14, Justification::centred);
    g.setColour (lt::faint);
    g.drawText ("a sound space after Bernhard Leitner and Iannis Xenakis", mid.getX(), 65, mid.getWidth(), 14, Justification::centred);

    const int fig = proc.figure->getIndex();
    g.setColour (lt::ink);
    g.setFont (typed (12.0f, true));
    g.drawText (String::fromUTF8 (leitner::figureNames()[fig]).toUpperCase() + "  xxxxx  " + String (leitner::figureEnglish()[fig]), mid.getX(), 426, mid.getWidth(), 16, Justification::centred);
    g.setFont (typed (10.5f));
    g.drawText ("line 1 black, the left channel  xxxxx  line 2 red, the right channel, its mirror image", mid.getX(), 443, mid.getWidth(), 14, Justification::centred);
    g.drawLine ((float) mid.getX(), 464.0f, (float) mid.getRight(), 464.0f, 0.8f);
    g.setColour (lt::faint);
    g.drawText (fig == leitner::fgDrawn ? "drag a point to move it  xx  shift-drag: draw  xx  drag the room to turn it"
                                        : "drag the room to turn it  xxxxx  double-click: from the front again", mid.getX(), 407, mid.getWidth(), 14, Justification::centred);

    g.setColour (lt::faint);
    g.setFont (typed (10.0f));
    g.drawText ("the sound itself is not changed: Leitner only says where it is", mid.getX(), 708, mid.getWidth(), 13, Justification::centred);

    // the side sheets
    g.setColour (lt::ink);
    g.setFont (typed (12.0f, true));
    g.drawText (fig == leitner::fgDrawn ? "ZEICHNUNG  xxxxx  YOUR OWN SPACE" : "TON-RAUM  xxxxx  SOUND SPACE", 26, 26, 436, 16, Justification::centredLeft);
    g.drawText ("STOCHASTIC SPACE  xxxxx  XENAKIS", designW - 26 - 436, 26, 436, 16, Justification::centredRight);
    g.setFont (typed (10.5f));
    g.setColour (lt::faint);
    g.drawText (fig == leitner::fgDrawn ? "a line of your own through the room, as Leitner drew his" : "after Bernhard Leitner, Ton-Raum, 1969 to now", 26, 46, 436, 14, Justification::centredLeft);
    g.drawText ("after Iannis Xenakis, Polytopes, 1967 to 1978", designW - 26 - 436, 46, 436, 14, Justification::centredRight);
    if (fig != leitner::fgDrawn) g.drawText ("line 1 black, the left channel; line 2 red, the right", 26, 754, 436, 14, Justification::centredLeft);
    g.drawText ("set on the middle sheet: chance, walk, scatter, sieve", designW - 26 - 436, 754, 436, 14, Justification::centredRight);
}

// the programs: after Leitner, after Xenakis, yours, and saving
void LeitnerEditor::programMenu()
{
    PopupMenu m; String group;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        if (proc.getProgramGroup (i) != group) { group = proc.getProgramGroup (i); m.addSectionHeader (group); }
        m.addItem (i + 1, proc.getProgramName (i), true, userShown.isEmpty() && i == proc.getCurrentProgram());
    }
    const auto files = proc.userPresets();
    if (! files.isEmpty()) m.addSectionHeader ("yours");
    for (int i = 0; i < files.size(); ++i) m.addItem (1001 + i, files[i].getFileNameWithoutExtension(), true, files[i].getFileNameWithoutExtension() == userShown);
    m.addSeparator();
    m.addItem (2000, "save this as a preset...");
    m.addItem (2001, "show the presets folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBtn), [this, files] (int r)
    {
        if (r >= 1 && r <= 1000) { proc.setCurrentProgram (r - 1); userShown.clear(); }
        else if (r > 1000 && r < 2000) { if (proc.loadUserPreset (files[r - 1001])) userShown = files[r - 1001].getFileNameWithoutExtension(); }
        else if (r == 2000)
        {
            proc.userPresetFolder().createDirectory();
            chooser = std::make_unique<FileChooser> ("Save this as a preset", proc.userPresetFolder().getChildFile ("my space" + proc.presetExtension()), "*" + proc.presetExtension());
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const FileChooser& fc) { const auto f = fc.getResult(); if (f != File() && proc.saveUserPreset (f)) userShown = f.getFileNameWithoutExtension(); });
        }
        else if (r == 2001) { proc.userPresetFolder().createDirectory(); proc.userPresetFolder().revealToUser(); }
    });
}

void LeitnerEditor::timerCallback()
{
    const int fig = proc.figure->getIndex(), out = proc.orderNow.load() * 100 + proc.channelsNow.load();
    const bool mine = fig == leitner::fgDrawn;
    if (! mine) lastFigure = fig;
    if (plan.isVisible() == mine) { plan.setVisible (! mine); drawSheet.setVisible (mine); }
    const String drawText = mine ? "back to " + String::fromUTF8 (leitner::figureNames()[lastFigure]) : String ("draw your own figure");
    if (drawBtn.getButtonText() != drawText) drawBtn.setButtonText (drawText);
    room.repaint(); chance.poll(); chance.repaint();
    if (mine) { drawSheet.repaint(); drawSheet.refreshButtons(); } else plan.repaint();
    if (fig != seenFigure || out != seenOut) { seenFigure = fig; seenOut = out; content.repaint(); }
    if (const int pr = proc.getCurrentProgram(); pr != seenProgram) { if (seenProgram >= 0) userShown.clear(); seenProgram = pr; }
    const String name = "program: " + (userShown.isNotEmpty() ? userShown : proc.getProgramName (proc.getCurrentProgram()));
    if (presetBtn.getButtonText() != name) presetBtn.setButtonText (name);
    for (auto* c : parts) if (auto* r = dynamic_cast<lt::Rule*> (c); r != nullptr && r->live != nullptr) r->repaint();
}
