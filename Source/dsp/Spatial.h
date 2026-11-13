#pragma once
#include "Ambi.h"

// Leitner's sound space: the two channels you give it become two lines of sound that travel through
// the room round you, after Bernhard Leitner's Ton-Raum work. He treated sound as a building
// material: a sound sent from loudspeaker to loudspeaker draws a line, and lines moving in time make
// a space you stand, sit or lie in. So here:
//
//   the figure   which space the lines draw (see Figure below)
//   the raster   Leitner's sound did not glide: it was switched from one loudspeaker to the next.
//                'points' is how many loudspeakers the figure is built from, and 'legato' how far
//                each one overlaps the next (0: it jumps, 1: it melts from point to point)
//   the time     how long one pass takes, with a rest at the end of each pass if you want one. Nothing
//                here counts bars: 'chance' lets every pass take its own length (see Xenakis below)
//   intensity    'swell' makes the line louder where it comes closest to the body, 'lift' thins it as it rises
//   two lines    the left channel is one line and the right its mirror image, 'apart' to either side
//                of the figure's direction, in step or 'counter' to each other
//
// And after Iannis Xenakis, who filled rooms with sound by calculation and chance:
//   chance       the length of each pass is drawn from an exponential law, as the durations in his
//                stochastic pieces were (0: every pass the same, 1: fully by chance)
//   walk         a random walk: at every pass the figure takes a Gaussian step in direction and height
//   scatter      each loudspeaker of the raster is thrown off the line by a Gaussian deviation, so the
//                line becomes a cloud of points round where it would have been
//   sieve        which loudspeakers of the raster sound at all: unions of residue classes, his sieves
//   and three figures: the Paraboloid (straight lines sweeping a curved surface, as the glissandi of
//   Metastaseis became the Philips Pavilion), Terretektorh (a spiral that speeds up as it closes in)
//   and Polytope (points thrown over the sphere, new ones every pass)
//
// Each line is encoded to Ambisonics (ACN, SN3D, 3rd to 7th order) at the points it's sounding
// from, inside the sphere as well as on it, so a line can pass straight through you.
//
// And a figure of your own: a path you draw through the room, point by point in three dimensions
// (see Drawn below). Everything else (the raster, the sieve, size, height, turn, apart, the mirror
// image, chance) works on it as on the others.
namespace leitner
{
using fp::ambi::Vec3;
using fp::ambi::Coefs;
using fp::ambi::kCh;
constexpr float kPi = 3.14159265f;
constexpr int kBlock = 32;
constexpr int kMaxPoints = 33;   // (33: no raster, the line is continuous)

// The figures. The first eleven are after things Leitner built or drew, the last three after Xenakis
// (see the README for where each comes from).
enum Figure
{
    fgLine = 0,      // Ton-Linie, the leading path: a straight line across the room, there and back; 'height' 0 goes through the head
    fgSwing,         // the swinging space of the Ton-Röhre: a pendulum across you, slowest at the ends
    fgLiege,         // Ton-Liege: to and fro along the body, low, as on the sound chair
    fgGate,          // Ton-Tor: up one side, over your head and down the other (with 'counter' at 100 %: one line rising as the other falls)
    fgVertical,      // the vertically bouncing space, the Wand-Raster's vertical lines: floor to ceiling and back
    fgCircle,        // circling: a ring round you at one height
    fgSoundcube,     // Soundcube 1969: four circles, one after another, at four heights
    fgSpiral,        // Spiral-Räume: winding up round the body and down again
    fgCube,          // Ton-Würfel: eight loudspeakers, corner to corner along the edges
    fgExpand,        // Erweitern-Verengen: out from the centre and back in
    fgVault,         // Wölbung: a vault overhead, drawn rib by rib
    fgParaboloid,    // Xenakis: straight lines zigzagging between two skew edges, sweeping a hyperbolic paraboloid
    fgTerretektorh,  // Xenakis: a flat spiral from the wall in towards you, faster and faster, and out again
    fgPolytope,      // Xenakis: eight points thrown over the sphere, joined in turn; new points every pass
    fgDrawn,         // your own: the path you draw (Drawn below)
    numFigures
};

inline const char* const* figureNames()
{
    static const char* const n[numFigures] = { "Ton-Linie", "Schwingen", "Ton-Liege", "Ton-Tor", "Vertikal", "Kreis", "Soundcube", "Spirale",
                                               "Ton-W\xc3\xbcrfel", "Erweitern-Verengen", "W\xc3\xb6lbung", "Paraboloid", "Terretektorh", "Polytope", "Eigene Zeichnung" };
    return n;
}
inline const char* const* figureEnglish()
{
    static const char* const n[numFigures] = { "line of sound", "swinging space", "sound chair", "sound gate", "vertical space", "circling", "four circles, four heights",
                                               "spiral space", "sound cube", "widening, narrowing", "vault", "ruled surface (Xenakis)", "accelerating spiral (Xenakis)",
                                               "points by chance (Xenakis)", "your own, drawn in three dimensions" };
    return n;
}

// the sieves: which of the raster's loudspeakers sound. Each is a union of residue classes (modulus, residue).
constexpr int kNumSieves = 5;
inline const char* const* sieveNames()
{
    static const char* const n[kNumSieves] = { "every point", "2/0", "3/0 + 4/1", "5/0 + 7/3", "3/1 + 5/2 + 7/0" };
    return n;
}
inline bool inSieve (int sieve, int k) noexcept
{
    switch (sieve)
    {
        case 1:  return k % 2 == 0;
        case 2:  return k % 3 == 0 || k % 4 == 1;
        case 3:  return k % 5 == 0 || k % 7 == 3;
        case 4:  return k % 3 == 1 || k % 5 == 2 || k % 7 == 0;
        default: return true;
    }
}

inline float hash01 (uint32_t a, uint32_t b, uint32_t c) noexcept;

// A path you draw: up to 64 points in the room (the room is -1..1 on every axis, you stand at the
// middle: x to the front, y to the left, z up), joined straight or in a curve through them, as a
// closed loop that runs round or an open line that goes there and comes back. prepare() measures it
// so the line moves along it at an even speed; it must be called after any change (the processor
// does, in setDrawing). The loudspeakers of the raster are spread evenly along it, or stand at your
// points if 'atPoints'.
constexpr int kMaxDrawn = 64;
struct Drawn
{
    static constexpr int kTable = 512;
    int count = 0;
    bool closed = true, curved = true, atPoints = false;
    Vec3 pt[kMaxDrawn] {};
    // (made by prepare)
    Vec3 table[kTable + 1] {};      // the path at kTable + 1 evenly spaced places along it
    float vertexW[kMaxDrawn] {};    // where along it (0..1) each of your points is

    // the default: a ring round you that rises and dips twice, a saddle
    static Drawn saddle()
    {
        Drawn d; d.count = 8;
        for (int k = 0; k < 8; ++k)
        {
            const float a = 2.0f * 3.14159265f * (float) k / 8.0f;
            d.pt[k] = { 0.8f * std::cos (a), 0.8f * std::sin (a), 0.5f * std::cos (2.0f * a) };
        }
        d.prepare();
        return d;
    }

    // point i of the curve's segment from i to i + 1, f = 0..1 (Catmull-Rom through the points when curved)
    Vec3 segment (int i, float f) const noexcept
    {
        auto P = [&] (int k)
        {
            if (closed) return pt[((k % count) + count) % count];
            if (k < 0) return pt[0] * 2.0f - pt[1];                               // (beyond the ends: straight on)
            if (k >= count) return pt[count - 1] * 2.0f - pt[count - 2];
            return pt[k];
        };
        const Vec3 p1 = P (i), p2 = P (i + 1);
        if (! curved) return p1 + (p2 - p1) * f;
        const Vec3 p0 = P (i - 1), p3 = P (i + 2);
        const float f2 = f * f, f3 = f2 * f;
        return (p1 * 2.0f + (p2 - p0) * f + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * f2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * f3) * 0.5f;
    }

    void prepare() noexcept
    {
        count = std::clamp (count, 0, kMaxDrawn);
        if (count < 2)
        {
            const Vec3 v = count == 1 ? pt[0] : Vec3 {};
            for (auto& t : table) t = v;
            for (auto& w : vertexW) w = 0.0f;
            return;
        }
        // the curve, finely, and how far along it each fine step is
        constexpr int Sub = 16;
        const int segs = closed ? count : count - 1;
        static thread_local std::vector<Vec3> fine; static thread_local std::vector<float> len;
        fine.resize ((size_t) (segs * Sub + 1)); len.resize (fine.size());
        for (int i = 0; i < segs; ++i)
            for (int k = 0; k < Sub; ++k) fine[(size_t) (i * Sub + k)] = segment (i, (float) k / (float) Sub);
        fine.back() = closed ? pt[0] : pt[count - 1];
        len[0] = 0.0f;
        for (size_t i = 1; i < fine.size(); ++i) len[i] = len[i - 1] + (fine[i] - fine[i - 1]).length();
        const float total = len.back();
        if (total < 1.0e-6f) { for (auto& t : table) t = pt[0]; for (auto& w : vertexW) w = 0.0f; return; }
        for (int i = 0; i < count; ++i) vertexW[i] = len[(size_t) (std::min (i, segs) * Sub)] / total;
        size_t j = 0;
        for (int k = 0; k <= kTable; ++k)
        {
            const float s = total * (float) k / (float) kTable;
            while (j + 2 < fine.size() && len[j + 1] < s) ++j;
            const float d = len[j + 1] - len[j], f = d > 1.0e-9f ? std::clamp ((s - len[j]) / d, 0.0f, 1.0f) : 0.0f;
            table[k] = fine[j] + (fine[j + 1] - fine[j]) * f;
        }
    }

    // where on it, w = 0..1 along it
    Vec3 at (float w) const noexcept
    {
        const float x = std::clamp (w, 0.0f, 1.0f) * (float) kTable;
        const int i = std::min ((int) x, kTable - 1);
        const float f = x - (float) i;
        return table[i] + (table[i + 1] - table[i]) * f;
    }
};

struct Params
{
    int order = 3, figure = fgLine;
    float seconds = 4.0f;        // one pass
    float rest = 0.0f;           // how much of each pass is spent waiting at its end (0..0.9)
    float size = 0.9f, height = 0.0f;
    float turn = 0.0f;           // the whole figure, degrees to the left of straight ahead
    float apart = 30.0f;         // line 1 is this many degrees to the left of that, line 2 its mirror image to the right
    float counter = 0.0f;        // line 2 behind line 1, 0..1 of half a cycle (1: exactly opposite)
    int points = 8;              // loudspeakers along the figure; kMaxPoints: continuous
    float legato = 0.5f;
    float swell = 0.0f;          // louder where the line comes nearest the body (Leitner: an approaching sound gets a crescendo)
    float lift = 0.0f;           // quieter as it rises, louder as it falls (his 'lifting' and 'dropping')
    float chance = 0.0f, walk = 0.0f, scatter = 0.0f; int sieve = 0;   // Xenakis (see the top of this file)
    float room = 0.15f, roomDecay = 3.0f, level = 1.0f;
    bool hold = false;           // stop the lines where they are
    const Drawn* drawn = nullptr; // your path, for fgDrawn
};

// what chance has decided so far: which pass this is, and where the random walk has taken the figure
struct State { uint32_t pass = 0; float dTurn = 0.0f, dHeight = 0.0f; };

// a number in 0..1 from three whole ones (the same every time: the drawing and the sound must agree)
inline float hash01 (uint32_t a, uint32_t b, uint32_t c) noexcept
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}
// roughly Gaussian, mean 0, deviation 1
inline float gauss (uint32_t a, uint32_t b, uint32_t c) noexcept
{
    return 2.0f * (hash01 (a, b, c) + hash01 (a, b, c + 101u) + hash01 (a, b, c + 202u) - 1.5f);
}

// is the figure a closed loop (it runs round) or a line (it goes there and comes back)?
inline bool closedFigure (int f) noexcept { return f == fgCircle || f == fgSoundcube || f == fgCube || f == fgVault || f == fgPolytope; }
inline bool closedFigure (const Params& p) noexcept { return p.figure == fgDrawn ? (p.drawn != nullptr && p.drawn->closed) : closedFigure (p.figure); }

// where on the figure, w = 0..1 along it, in the figure's own frame; 'az' is line 1's angle (radians, to the left of ahead)
inline Vec3 figurePoint (int figure, float w, float size, float height, float az, uint32_t pass = 0, const Drawn* drawn = nullptr) noexcept
{
    const float cz = std::cos (az), sz = std::sin (az);
    auto along = [&] (float d, float side, float z) { return Vec3 { d * cz - side * sz, d * sz + side * cz, z }; };   // d: along the direction, side: to its left
    switch (figure)
    {
        case fgSwing:    { const float th = (2.0f * w - 1.0f) * 1.2f;           return along (0.0f, size * std::sin (th), height + 0.45f - size * std::cos (th)); }
        case fgLiege:    return along ((2.0f * w - 1.0f) * 0.7f * size, 0.0f, height - 0.55f);
        case fgGate:     { const float th = (2.0f * w - 1.0f) * 0.5f * kPi;     return along (0.0f, size * std::sin (th), height - 0.2f + size * std::cos (th)); }
        case fgVertical: return along (0.6f * size, 0.0f, height + (2.0f * w - 1.0f) * 0.95f);
        case fgCircle:   { const float a = 2.0f * kPi * w;                       return along (size * std::cos (a), size * std::sin (a), height); }
        case fgSoundcube:
        {
            const float x = w * 4.0f; const int i = std::min (3, (int) x); const float a = 2.0f * kPi * (x - (float) i);
            static const float h[4] = { -0.6f, 0.2f, -0.2f, 0.6f };   // alternating, as on the sheet
            const float r = size * 0.8f;
            return along (r * std::cos (a), r * std::sin (a), height + h[i]);
        }
        case fgSpiral:   { const float a = 2.0f * kPi * 3.0f * w;                return along (0.6f * size * std::cos (a), 0.6f * size * std::sin (a), height + (2.0f * w - 1.0f) * 0.9f); }
        case fgCube:
        {
            // the eight corners in an order that only ever moves along an edge
            static const float c[8][3] = { { 1, 1, -1 }, { 1, -1, -1 }, { -1, -1, -1 }, { -1, 1, -1 }, { -1, 1, 1 }, { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 } };
            const float x = w * 8.0f; const int i = (int) x & 7, j = (i + 1) & 7; const float f = x - std::floor (x);
            const float k = size * 0.577f;
            return along (k * (c[i][0] + f * (c[j][0] - c[i][0])), k * (c[i][1] + f * (c[j][1] - c[i][1])), height + k * (c[i][2] + f * (c[j][2] - c[i][2])));
        }
        case fgExpand:   return along (w * size, 0.0f, height);
        case fgVault:
        {
            // six ribs, each at its own angle: up one from the rim to the top, down the next
            const float x = w * 6.0f; const int i = (int) x % 6; const float f = x - std::floor (x);
            const float el = ((i % 2 == 0) ? f : 1.0f - f) * 0.5f * kPi;   // 0 at the rim, a quarter turn at the top
            const float ang = 2.0f * kPi * (float) i / 6.0f;
            return along (size * std::cos (el) * std::cos (ang), size * std::cos (el) * std::sin (ang), height + size * std::sin (el));
        }
        case fgParaboloid:
        {
            // two edges that cross in the air (one climbs as the other falls); the line runs straight
            // from one to the other and back, a step further along each time: straight lines, a curved surface
            constexpr int R = 6;
            const float x = std::min (w * (float) R, (float) R - 1.0e-4f); const int i = (int) x; const float f = x - (float) i;
            auto edge = [&] (bool left, float s) { return Vec3 { (2.0f * s - 1.0f) * 0.62f * size, (left ? 0.62f : -0.62f) * size, (left ? 1.0f : -1.0f) * (0.7f - 1.4f * s) * size }; };
            const float s0 = (float) i / (float) R, s1 = (float) (i + 1) / (float) R;
            const Vec3 a = edge (i % 2 == 0, s0), b = edge (i % 2 != 0, s1);
            return along (a.x + f * (b.x - a.x), a.y + f * (b.y - a.y), height + a.z + f * (b.z - a.z));
        }
        case fgTerretektorh: { const float a = 2.0f * kPi * 3.0f * w, r = size * (1.0f - 0.85f * w);   return along (r * std::cos (a), r * std::sin (a), height); }
        case fgPolytope:
        {
            constexpr int N = 8;
            auto pt = [&] (uint32_t ps, int k)
            {
                const float z = 2.0f * hash01 (ps, (uint32_t) k, 1u) - 1.0f, a = 2.0f * kPi * hash01 (ps, (uint32_t) k, 2u), r = std::sqrt (std::max (0.0f, 1.0f - z * z));
                return Vec3 { size * r * std::cos (a), size * r * std::sin (a), size * z };
            };
            const float x = std::min (w * (float) N, (float) N - 1.0e-4f); const int i = (int) x; const float f = x - (float) i;
            const Vec3 a = pt (pass, i), b = i + 1 < N ? pt (pass, i + 1) : pt (pass + 1u, 0);
            return along (a.x + f * (b.x - a.x), a.y + f * (b.y - a.y), height * 0.5f + a.z + f * (b.z - a.z));
        }
        case fgDrawn:    { const Vec3 q = drawn != nullptr ? drawn->at (w) : Vec3 {};     return along (size * q.x, size * q.y, height + size * q.z); }
        default:         return along ((2.0f * w - 1.0f) * size, 0.0f, height);
    }
}

// one of your drawn points, as line 1 places it in the room (size, height, apart, turn and the walk
// applied; not pulled in to the sphere), and back: where to put a point so that line 1 has it at v
inline Vec3 drawnToRoom (const Params& p, const State& st, Vec3 q) noexcept
{
    const float az = p.apart * kPi / 180.0f, t = (p.turn + st.dTurn) * kPi / 180.0f;
    const Vec3 a { p.size * q.x, p.size * q.y, p.height + st.dHeight + p.size * q.z };
    const Vec3 b { a.x * std::cos (az) - a.y * std::sin (az), a.x * std::sin (az) + a.y * std::cos (az), a.z };
    return { b.x * std::cos (t) - b.y * std::sin (t), b.x * std::sin (t) + b.y * std::cos (t), b.z };
}
inline Vec3 roomToDrawn (const Params& p, const State& st, Vec3 v) noexcept
{
    const float ang = -((p.turn + st.dTurn) + p.apart) * kPi / 180.0f, k = 1.0f / std::max (0.05f, p.size);
    const Vec3 a { v.x * std::cos (ang) - v.y * std::sin (ang), v.x * std::sin (ang) + v.y * std::cos (ang), v.z - p.height - st.dHeight };
    return { std::clamp (a.x * k, -1.0f, 1.0f), std::clamp (a.y * k, -1.0f, 1.0f), std::clamp (a.z * k, -1.0f, 1.0f) };
}

// line 0 or 1 at w. Line 2 is line 1's mirror image (left for right), and 'turn' swings the pair round
// together. 'point' >= 0: this is loudspeaker number 'point' of the raster, thrown off the line by 'scatter'.
inline Vec3 linePoint (int line, const Params& p, const State& st, float w, int point = -1) noexcept
{
    Vec3 v = figurePoint (p.figure, w, p.size, p.height + st.dHeight, p.apart * kPi / 180.0f, st.pass, p.drawn);
    if (point >= 0 && p.scatter > 0.0f)
    {
        const float k = 0.3f * p.scatter;
        v += Vec3 { k * gauss (st.pass, (uint32_t) point, 11u), k * gauss (st.pass, (uint32_t) point, 12u), k * gauss (st.pass, (uint32_t) point, 13u) };
    }
    if (line == 1) v.y = -v.y;
    const float t = (p.turn + st.dTurn) * kPi / 180.0f, c = std::cos (t), s = std::sin (t);
    v = { v.x * c - v.y * s, v.x * s + v.y * c, v.z };
    const float l = v.length();
    return l > 1.0f ? v * (1.0f / l) : v;
}

// is the line continuous (no raster)? Not when your drawn points are the loudspeakers.
inline bool continuous (const Params& p) noexcept
{
    return p.points >= kMaxPoints && ! (p.figure == fgDrawn && p.drawn != nullptr && p.drawn->atPoints && p.drawn->count >= 2);
}
constexpr int kMaxRaster = kMaxDrawn;   // the most loudspeakers a raster can have (arrays for rasterPoints are this long)

// the raster: the loudspeakers the sieve lets through, as places along the figure (w). Returns how many.
inline int rasterPoints (const Params& p, float* w, int* index) noexcept
{
    const bool closed = closedFigure (p);
    int n = 0;
    if (p.figure == fgDrawn && p.drawn != nullptr && p.drawn->atPoints && p.drawn->count >= 2)
    {
        // your points are the loudspeakers (the sieve still picks among them)
        const auto& d = *p.drawn;
        for (int k = 0; k < d.count; ++k) if (inSieve (p.sieve, k)) { w[n] = d.vertexW[k]; index[n] = k; ++n; }
        if (n < 2) { n = 0; for (int k = 0; k < d.count; ++k) { w[n] = d.vertexW[k]; index[n] = k; ++n; } }
        return n;
    }
    const int K = std::clamp (p.points, 2, kMaxPoints - 1);
    for (int k = 0; k < K; ++k)
        if (inSieve (p.sieve, k)) { w[n] = closed ? (float) k / (float) K : (float) k / (float) (K - 1); index[n] = k; ++n; }
    if (n < 2) { n = 0; for (int k = 0; k < K; ++k) { w[n] = closed ? (float) k / (float) K : (float) k / (float) (K - 1); index[n] = k; ++n; } }
    return n;
}

class Space
{
public:
    // for the drawing: where each line is, the two loudspeakers it's sounding from and how much of each
    struct LineView { std::atomic<float> x { 0 }, y { 0 }, z { 0 }, ax { 0 }, ay { 0 }, az { 0 }, bx { 0 }, by { 0 }, bz { 0 }, ga { 0 }, gb { 0 }, level { 0 }; };
    LineView view[2];
    std::atomic<float> phaseView { 0.0f }, dTurnView { 0.0f }, dHeightView { 0.0f }, passLength { 1.0f };
    std::atomic<uint32_t> passView { 0 };
    State stateNow() const noexcept { return { passView.load(), dTurnView.load(), dHeightView.load() }; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reverb.prepare (sr);
        for (auto& b : send) b.assign (2048, 0.0f);
        for (auto& b : wet) b.assign (2048, 0.0f);
        for (auto& l : lines) l = Line {};
        phase = 0.0; first = true; roomSm = 0.0f; levelSm = 1.0f; guard = 1.0f;
        st = State {}; turnTarget = heightTarget = 0.0f; length = 1.0f;
    }

    // inL/inR: n samples of stereo. out: nch channel pointers (cleared here, then written).
    void process (const float* inL, const float* inR, float* const* out, int nch, int n, const Params& p) noexcept
    {
        const int order = std::clamp (std::min (p.order, fp::ambi::orderFor (nch)), 1, fp::ambi::kMaxOrder);
        const int used = fp::ambi::chFor (order);
        for (int c = 0; c < nch; ++c) std::fill (out[c], out[c] + n, 0.0f);
        reverb.setChannels (used);
        reverb.setDecay (p.roomDecay);

        const float* in[2] = { inL, inR };
        for (int off = 0; off < n; off += kBlock)
        {
            const int m = std::min (kBlock, n - off);
            if (! p.hold)
            {
                phase += (double) m / (sr * std::max (0.05, (double) p.seconds) * (double) length);
                if (phase >= 1.0) { phase -= std::floor (phase); nextPass (p); }
            }
            // the walk glides to where chance sent it (about a fifth of a second)
            if (p.walk <= 0.0f) turnTarget = heightTarget = 0.0f;
            st.dTurn += 0.004f * (turnTarget - st.dTurn);
            st.dHeight += 0.004f * (heightTarget - st.dHeight);
            phaseView.store ((float) phase, std::memory_order_relaxed);
            passView.store (st.pass, std::memory_order_relaxed);
            dTurnView.store (st.dTurn, std::memory_order_relaxed); dHeightView.store (st.dHeight, std::memory_order_relaxed);

            for (int li = 0; li < 2; ++li)
            {
                auto& L = lines[li];
                Coefs target;
                float lvl = 0.0f;
                place (li, p, order, target, lvl);
                const float* x = in[li] + off;

                if (first) L.g = target;
                const float inv = 1.0f / (float) m;
                float pk = 0.0f;
                for (int c = 0; c < used; ++c)
                {
                    float g = L.g[(size_t) c];
                    const float dg = (target[(size_t) c] - g) * inv;
                    float* o = out[c] + off;
                    for (int i = 0; i < m; ++i) { g += dg; o[i] += x[i] * g; }
                    L.g[(size_t) c] = target[(size_t) c];
                }
                for (int i = 0; i < m; ++i) pk = std::max (pk, std::abs (x[i]));
                L.level = std::max (pk * lvl, L.level * 0.92f);
                view[li].level.store (L.level, std::memory_order_relaxed);
            }
            first = false;
        }

        // the room: one reverb for the whole sphere, fed by everything the lines play
        const float roomTarget = p.room * p.room * 1.4f;
        if (roomTarget > 1.0e-4f || roomSm > 1.0e-4f || tail > 0)
        {
            for (int off = 0; off < n; off += 2048)
            {
                const int m = std::min (2048, n - off);
                const float* ins[kCh]; float* outs[kCh];
                for (int c = 0; c < used; ++c)
                {
                    std::copy (out[c] + off, out[c] + off + m, send[(size_t) c].begin());
                    ins[c] = send[(size_t) c].data(); outs[c] = wet[(size_t) c].data();
                }
                reverb.process (ins, outs, m);
                const float step = (roomTarget - roomSm) / (float) m;
                float g = roomSm;
                for (int i = 0; i < m; ++i) { g += step; for (int c = 0; c < used; ++c) out[c][off + i] += g * wet[(size_t) c][(size_t) i]; }
                roomSm = roomTarget;
            }
            tail = roomTarget > 1.0e-4f ? (int) (sr * 30.0) : std::max (0, tail - n);
        }

        const float lstep = (p.level - levelSm) / (float) n;
        for (int i = 0; i < n; ++i) { levelSm += lstep; for (int c = 0; c < used; ++c) out[c][i] *= levelSm; }
        levelSm = p.level;

        // a ceiling: if the omni channel would pass full scale, every channel is turned down together (so the
        // directions stay true), at once, and let back up over a fifth of a second. It is a safety net: at
        // ordinary levels it does nothing.
        {
            const float up = 1.0f / (float) (0.2 * sr);
            for (int i = 0; i < n; ++i)
            {
                const float w = std::abs (out[0][i]);
                guard = std::min (1.0f, guard + up);
                if (w * guard > 0.95f) guard = 0.95f / w;
                if (guard < 1.0f) for (int c = 0; c < used; ++c) out[c][i] *= guard;
            }
        }
    }

private:
    struct Line { Coefs g {}; float level = 0.0f; };

    // a pass is over: chance decides how long the next one takes and where the walk steps to
    void nextPass (const Params& p) noexcept
    {
        ++st.pass;
        // an exponential law (mean 1), as for the gaps between events in a Poisson process; kept between a sixth and four times
        const float e = std::clamp (-std::log (std::max (1.0e-4f, hash01 (st.pass, 7u, 3u))), 0.16f, 4.0f);
        length = 1.0f + p.chance * (e - 1.0f);
        passLength.store (length, std::memory_order_relaxed);
        // a Gaussian step, with a gentle pull home so it never wanders off for good
        turnTarget = 0.85f * turnTarget + p.walk * 60.0f * gauss (st.pass, 5u, 1u);
        heightTarget = std::clamp (0.85f * heightTarget + p.walk * 0.35f * gauss (st.pass, 5u, 2u), -0.9f, 0.9f);
    }

    // where line 'li' is now: the gains of all its channels, and its level along the path
    void place (int li, const Params& p, int order, Coefs& target, float& lvl) noexcept
    {
        const bool closed = closedFigure (p);
        double u = phase + (li == 1 ? 0.5 * (double) p.counter : 0.0);
        u -= std::floor (u);
        const float rest = std::clamp (p.rest, 0.0f, 0.9f);
        float w, prox;
        if (closed)
        {
            w = std::min (1.0f, (float) u / (1.0f - rest));
            if (w >= 1.0f) w = p.figure == fgPolytope ? 0.9999f : 0.0f;     // waiting (a polytope waits at its last point: the next pass starts from there)
            prox = 1.0f;
        }
        else
        {
            // there (first half) and back (second half), resting at each end
            const float h = (float) (u < 0.5 ? u * 2.0 : (u - 0.5) * 2.0);
            float t = std::min (1.0f, h / (1.0f - rest));
            if (p.figure == fgSwing || p.figure == fgLiege) t = 0.5f - 0.5f * std::cos (kPi * t);   // a swing: slow at the ends
            if (p.figure == fgTerretektorh) t = u < 0.5 ? t * t : 1.0f - (1.0f - t) * (1.0f - t);   // faster and faster going in; coming out, slower and slower
            w = u < 0.5 ? t : 1.0f - t;
            prox = (p.figure == fgExpand || p.figure == fgTerretektorh) ? (p.figure == fgExpand ? 1.0f - w : w) : std::sin (kPi * w);
        }
        lvl = 1.0f - p.swell + p.swell * (closed ? 1.0f : 0.25f + 1.25f * prox);

        Vec3 a, b; float ga = 1.0f, gb = 0.0f;
        const Vec3 here = linePoint (li, p, st, w);
        if (continuous (p)) { a = b = here; }
        else
        {
            // the two loudspeakers of the raster it is between, and how far it has got from one to the other
            float ws[kMaxRaster]; int idx[kMaxRaster];
            const int n = rasterPoints (p, ws, idx);
            int i = 0;
            if (closed) { while (i + 1 < n && ws[i + 1] <= w) ++i; }
            else        { while (i + 2 < n && ws[i + 1] <= w) ++i; }
            const int j = closed ? (i + 1) % n : i + 1;
            const float wa = ws[i], wb = (closed && j == 0) ? 1.0f : ws[j];
            const float f = std::clamp ((w - wa) / std::max (1.0e-5f, wb - wa), 0.0f, 1.0f);
            a = linePoint (li, p, st, ws[i], idx[i]); b = linePoint (li, p, st, ws[j], idx[j]);
            const float width = std::max (0.03f, p.legato);
            const float c = fp::smoothstep ((f - 0.5f) / width + 0.5f);
            ga = std::cos (0.5f * kPi * c); gb = std::sin (0.5f * kPi * c);
        }
        lvl *= std::clamp (1.0f - 0.7f * p.lift * here.z, 0.0f, 2.0f);   // lifting: it thins as it rises
        // (inside the sphere the encoder lifts a sound by up to 6 dB at the very middle; two lines meeting there
        // would add to 12, so half of that lift is taken back)
        lvl *= std::sqrt (0.5f + 0.5f * std::min (1.0f, 0.5f * (a.length() + b.length()) / 0.7f));
        const auto ya = fp::ambi::encodeInside (a, order);
        target = {};
        const int used = fp::ambi::chFor (order);
        if (gb > 1.0e-4f)
        {
            const auto yb = fp::ambi::encodeInside (b, order);
            for (int c = 0; c < used; ++c) target[(size_t) c] = lvl * (ga * ya[(size_t) c] + gb * yb[(size_t) c]);
        }
        else for (int c = 0; c < used; ++c) target[(size_t) c] = lvl * ga * ya[(size_t) c];

        auto& v = view[li];
        v.x.store (here.x, std::memory_order_relaxed); v.y.store (here.y, std::memory_order_relaxed); v.z.store (here.z, std::memory_order_relaxed);
        v.ax.store (a.x, std::memory_order_relaxed); v.ay.store (a.y, std::memory_order_relaxed); v.az.store (a.z, std::memory_order_relaxed);
        v.bx.store (b.x, std::memory_order_relaxed); v.by.store (b.y, std::memory_order_relaxed); v.bz.store (b.z, std::memory_order_relaxed);
        v.ga.store (ga, std::memory_order_relaxed); v.gb.store (gb, std::memory_order_relaxed);
    }

    double sr = 48000.0, phase = 0.0;
    Line lines[2];
    State st;
    float turnTarget = 0.0f, heightTarget = 0.0f, length = 1.0f;
    fp::ambi::SphereReverb reverb;
    std::array<std::vector<float>, kCh> send, wet;
    float roomSm = 0.0f, levelSm = 1.0f, guard = 1.0f;
    int tail = 0;
    bool first = true;
};
} // namespace leitner
