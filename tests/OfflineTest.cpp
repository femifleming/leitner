// Offline checks for Leitner (not shipped): the Ambisonic encoding (direction, mirror image, the
// raster's steps, every order), the state, what a host sees, CPU, the window's controls worked with
// the mouse, and pictures of the window, whole and in parts (the manual is made of them).
#include "../Source/LeitnerEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cstdio>
#include <set>

using namespace juce;
static constexpr double SR = 48000.0;
static constexpr int BLOCK = 256;
static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }

static void setP (RangedAudioParameter* p, float real) { p->setValueNotifyingHost (p->convertTo0to1 (real)); }

// the window's parts, and the mouse on them (no screen needed)
template <class T> static T* find (Component& root)
{
    for (auto* c : root.getChildren()) { if (auto* t = dynamic_cast<T*> (c)) return t; if (auto* t = find<T> (*c)) return t; }
    return nullptr;
}
template <class T> static void all (Component& root, std::vector<T*>& out)
{
    for (auto* c : root.getChildren()) { if (auto* t = dynamic_cast<T*> (c)) out.push_back (t); all (*c, out); }
}
static MouseEvent ev (Component& c, Point<float> at, Point<float> down, ModifierKeys mods = ModifierKeys::leftButtonModifier, int clicks = 1, bool dragged = false)
{
    return MouseEvent (Desktop::getInstance().getMainMouseSource(), at, mods, MouseInputSource::defaultPressure, MouseInputSource::defaultOrientation, MouseInputSource::defaultRotation,
                       MouseInputSource::defaultTiltX, MouseInputSource::defaultTiltY, &c, &c, Time::getCurrentTime(), down, Time::getCurrentTime(), clicks, dragged);
}
static void click (Component& c, Point<float> at) { c.mouseDown (ev (c, at, at)); c.mouseUp (ev (c, at, at)); }
static void drag (Component& c, Point<float> from, Point<float> to)
{
    c.mouseDown (ev (c, from, from)); c.mouseDrag (ev (c, to, from, ModifierKeys::leftButtonModifier, 1, true)); c.mouseUp (ev (c, to, from, ModifierKeys::leftButtonModifier, 1, true));
}

// no room: the lines carry the input and nothing else
static void dry (LeitnerProcessor& p) { setP (p.room, 0.0f); }

// a 440 Hz tone into the left, the right or both, for 'seconds'; returns all 64 channels
static AudioBuffer<float> run (LeitnerProcessor& p, double seconds, bool left, bool right, int channels = 64, double* realtime = nullptr)
{
    p.setPlayConfigDetails (2, channels, SR, BLOCK);
    p.prepareToPlay (SR, BLOCK);
    const int total = (int) (seconds * SR) / BLOCK * BLOCK;
    AudioBuffer<float> all (channels, total), buf (channels, BLOCK);
    MidiBuffer midi;
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += BLOCK)
    {
        buf.clear();
        for (int i = 0; i < BLOCK; ++i)
        {
            const float s = 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 440.0 * (pos + i) / SR);
            if (left) buf.setSample (0, i, s);
            if (right) buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < channels; ++c) all.copyFrom (c, pos, buf, c, 0, BLOCK);
    }
    if (realtime != nullptr) *realtime = seconds / std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return all;
}

// the direction a first-order decoder would point to over a stretch: (X, Y, Z) / W by least squares
static leitner::Vec3 directionOf (const AudioBuffer<float>& a, int from, int len)
{
    double ww = 0, wx = 0, wy = 0, wz = 0;
    for (int i = from; i < from + len; ++i)
    {
        const double w = a.getSample (0, i);
        ww += w * w; wy += w * a.getSample (1, i); wz += w * a.getSample (2, i); wx += w * a.getSample (3, i);
    }
    return { (float) (wx / ww), (float) (wy / ww), (float) (wz / ww) };
}

// LeitnerTest --render in.wav out.wav channels [index=value ...]: the processor run on a file as a host runs it
// (parameters by their number, values 0..1), written as 32-bit float. What a host renders is compared with this.
static int renderFile (const StringArray& args)
{
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> in (fm.createReaderFor (File (args[0])));
    const int channels = args[2].getIntValue();
    if (in == nullptr || channels < 1 || channels > 64) { std::printf ("usage: LeitnerTest --render in.wav out.wav channels [index=value ...]\n"); return 2; }
    LeitnerProcessor p;
    for (int i = 3; i < args.size(); ++i)
    {
        const int index = args[i].upToFirstOccurrenceOf ("=", false, false).getIntValue();
        if (isPositiveAndBelow (index, p.getParameters().size())) p.getParameters()[index]->setValueNotifyingHost (args[i].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    }
    constexpr int block = 512;
    p.setPlayConfigDetails (2, channels, in->sampleRate, block);
    p.prepareToPlay (in->sampleRate, block);
    const int total = (int) in->lengthInSamples;
    AudioBuffer<float> src (2, total), all (channels, total), buf (jmax (2, channels), block);
    in->read (&src, 0, total, 0, true, true);
    MidiBuffer midi;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> view (buf.getArrayOfWritePointers(), jmax (2, channels), n);
        view.clear();
        for (int c = 0; c < 2; ++c) view.copyFrom (c, 0, src, c, pos, n);
        AudioBuffer<float> io (view.getArrayOfWritePointers(), channels == 1 ? 2 : channels, n);   // (a host hands it as many channels as the wider side)
        p.processBlock (io, midi);
        for (int c = 0; c < channels; ++c) all.copyFrom (c, pos, io, c, 0, n);
    }
    File (args[1]).deleteFile();
    std::unique_ptr<OutputStream> os = std::make_unique<FileOutputStream> (File (args[1]));
    auto w = WavAudioFormat().createWriterFor (os, AudioFormatWriterOptions().withSampleRate (in->sampleRate).withNumChannels (channels).withBitsPerSample (32));
    if (w == nullptr) return 2;
    w->writeFromAudioSampleBuffer (all, 0, total);
    std::printf ("%d channels, %d samples, order %d out\n", channels, total, p.orderNow.load());
    return 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    if (argc > 4 && String (argv[1]) == "--render") return renderFile (StringArray (argv + 2, argc - 2));
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    {   // the harmonics themselves: SN3D, orthogonal, at every order (a mean over many directions)
        const auto dirs = fp::ambi::fibonacci (4000);
        bool ok = true; double worst = 0.0;
        for (int a = 0; a < 64; ++a)
            for (int b = a; b < 64; ++b)
            {
                double s = 0;
                for (auto& d : dirs) { const auto y = fp::ambi::sh (d, 7); s += (double) y[(size_t) a] * y[(size_t) b]; }
                s /= (double) dirs.size();
                const double want = a == b ? 1.0 / (2.0 * fp::ambi::orderOf (a) + 1.0) : 0.0;
                worst = std::max (worst, std::abs (s - want));
                if (std::abs (s - want) > 0.01) ok = false;
            }
        check (ok, String::formatted ("the 64 harmonics are orthogonal with SN3D weights (worst error %.4f)", worst));
    }
    {   // a held line: the encoded direction is where the figure says the line is, and line 2 is its mirror image
        LeitnerProcessor p; dry (p);
        setP (p.hold, 1.0f); setP (p.points, (float) leitner::kMaxPoints); setP (p.figure, (float) leitner::fgLine);
        setP (p.size, 0.9f); setP (p.height, 0.3f); setP (p.apart, 40.0f); setP (p.turn, 0.0f);
        const auto sp = p.readSpace();
        const auto want = leitner::linePoint (0, sp, {}, 0.0f);
        auto a = run (p, 0.5, true, false);
        auto d = directionOf (a, 12000, 8000);
        const auto wn = want.normalised();
        check (std::abs (d.x - wn.x) < 0.01f && std::abs (d.y - wn.y) < 0.01f && std::abs (d.z - wn.z) < 0.01f,
               String::formatted ("line 1 is encoded at (%.3f, %.3f, %.3f); the figure says (%.3f, %.3f, %.3f)", d.x, d.y, d.z, wn.x, wn.y, wn.z));
        check (std::abs (a.getRMSLevel (0, 12000, 8000) - 0.25f / std::sqrt (2.0f)) < 0.003f, "the omni channel carries the input at unity");
        LeitnerProcessor q; dry (q);
        setP (q.hold, 1.0f); setP (q.points, (float) leitner::kMaxPoints); setP (q.size, 0.9f); setP (q.height, 0.3f); setP (q.apart, 40.0f);
        auto b = run (q, 0.5, false, true);
        auto e = directionOf (b, 12000, 8000);
        check (std::abs (e.x - d.x) < 0.01f && std::abs (e.y + d.y) < 0.01f && std::abs (e.z - d.z) < 0.01f,
               String::formatted ("line 2 is its mirror image: (%.3f, %.3f, %.3f)", e.x, e.y, e.z));
    }
    {   // every order: (N + 1)^2 channels carry sound, the rest are silent
        bool ok = true; String detail;
        for (int o = 3; o <= 7; ++o)
        {
            LeitnerProcessor p; dry (p);
            setP (p.order, (float) (o - 3)); setP (p.figure, (float) leitner::fgSpiral);
            const auto a = run (p, 1.0, true, true);
            int live = 0; bool finite = true;
            for (int c = 0; c < 64; ++c)
            {
                if (a.getMagnitude (c, 0, a.getNumSamples()) > 1.0e-6f) live = c + 1;
                for (int i = 0; i < a.getNumSamples(); i += 97) if (! std::isfinite (a.getSample (c, i))) finite = false;
            }
            detail << o << ":" << live << " ";
            if (live != (o + 1) * (o + 1) || ! finite) ok = false;
        }
        check (ok, "channels in use at orders 3 to 7: " + detail);
        // a 16-channel track asked for 7th order gets 3rd
        LeitnerProcessor p; dry (p); setP (p.order, 4.0f);
        const auto a = run (p, 0.3, true, true, 16);
        check (p.orderNow.load() == 3 && a.getMagnitude (13, 0, a.getNumSamples()) > 1.0e-6f   /* (ACN 13: even left to right, so the two mirrored lines add) */, "7th order on a 16-channel track comes out as 3rd");
    }
    {   // the raster: with 4 loudspeakers and no legato, the line only ever sounds from 4 places
        LeitnerProcessor p; dry (p);
        setP (p.figure, (float) leitner::fgCircle); setP (p.points, 4.0f); setP (p.legato, 0.0f); setP (p.seconds, 1.0f);
        const auto a = run (p, 2.0, true, false);
        std::set<int> places; int between = 0, n = 0;
        for (int i = 4800; i + 240 < a.getNumSamples(); i += 240)
        {
            const auto d = directionOf (a, i, 240);
            const float az = std::atan2 (d.y, d.x) * 180.0f / MathConstants<float>::pi;
            const float len = std::sqrt (d.x * d.x + d.y * d.y);
            ++n;
            if (len > 0.97f) places.insert ((int) std::lround (az / 10.0f)); else ++between;
        }
        check (places.size() == 4 && between < n / 5, String::formatted ("4 points, no legato: it sounds from %d places (%d of %d moments in a switch)", (int) places.size(), between, n));
        LeitnerProcessor q; dry (q);
        setP (q.figure, (float) leitner::fgCircle); setP (q.points, (float) leitner::kMaxPoints); setP (q.seconds, 1.0f);
        const auto b = run (q, 2.0, true, false);
        std::set<int> smooth;
        for (int i = 4800; i + 240 < b.getNumSamples(); i += 240) { const auto d = directionOf (b, i, 240); smooth.insert ((int) std::lround (std::atan2 (d.y, d.x) * 18.0f / MathConstants<float>::pi)); }
        check (smooth.size() > 30, String::formatted ("continuous: it passes through %d of 36 directions", (int) smooth.size()));
    }
    {   // Xenakis: a sieve thins the raster; chance makes the passes unequal; with neither, every pass is the same
        LeitnerProcessor p; dry (p);
        setP (p.figure, (float) leitner::fgCircle); setP (p.points, 12.0f); setP (p.legato, 0.0f); setP (p.seconds, 1.0f); setP (p.sieve, 1.0f);
        const auto a = run (p, 2.0, true, false);
        std::set<int> places;
        for (int i = 4800; i + 240 < a.getNumSamples(); i += 240)
        {
            const auto d = directionOf (a, i, 240);
            if (std::sqrt (d.x * d.x + d.y * d.y) > 0.97f) places.insert ((int) std::lround (std::atan2 (d.y, d.x) * 18.0f / MathConstants<float>::pi));
        }
        check (places.size() == 6, String::formatted ("12 points through the sieve 2/0: it sounds from %d places", (int) places.size()));
        float w[leitner::kMaxPoints]; int idx[leitner::kMaxPoints];
        leitner::Params sp; sp.points = 16; sp.sieve = 2;
        const int n = leitner::rasterPoints (sp, w, idx);
        String got; for (int i = 0; i < n; ++i) got << idx[i] << " ";
        check (got == "0 1 3 5 6 9 12 13 15 ", "the sieve 3/0 + 4/1 on 16 points lets through " + got);

        auto passLengths = [] (float chance)
        {
            LeitnerProcessor q; dry (q);
            setP (q.seconds, 0.2f); setP (q.chance, chance);
            q.setPlayConfigDetails (2, 16, SR, BLOCK); q.prepareToPlay (SR, BLOCK);
            AudioBuffer<float> buf (16, BLOCK); MidiBuffer midi;
            std::vector<int> lengths; uint32_t pass = 0; int since = 0;
            for (int b = 0; b < 4000; ++b)
            {
                buf.clear(); q.processBlock (buf, midi); since += BLOCK;
                if (const auto now = q.space.passView.load(); now != pass) { pass = now; lengths.push_back (since); since = 0; }
            }
            return lengths;
        };
        auto spread = [] (const std::vector<int>& v) { int lo = 1 << 30, hi = 0; for (size_t i = 1; i < v.size(); ++i) { lo = std::min (lo, v[i]); hi = std::max (hi, v[i]); } return (double) hi / (double) std::max (1, lo); };
        const auto even = passLengths (0.0f), byChance = passLengths (1.0f);
        check (spread (even) < 1.1 && spread (byChance) > 4.0, String::formatted ("chance 0: longest pass %.2fx the shortest; chance 100 %%: %.1fx", spread (even), spread (byChance)));
    }
    {   // every figure at 7th order with the room: finite, and how fast
        bool ok = true; double slowest = 1.0e9; float peak = 0.0f;
        for (int f = 0; f < leitner::numFigures; ++f)
        {
            LeitnerProcessor p;
            setP (p.order, 4.0f); setP (p.figure, (float) f); setP (p.room, 0.5f); setP (p.seconds, 0.7f); setP (p.swell, 0.6f); setP (p.lift, 0.6f); setP (p.counter, 1.0f);
            double rt = 0;
            const auto a = run (p, 2.0, true, true, 64, &rt);
            slowest = std::min (slowest, rt);
            for (int c = 0; c < 64; ++c) { peak = std::max (peak, a.getMagnitude (c, 0, a.getNumSamples())); for (int i = 0; i < a.getNumSamples(); i += 61) if (! std::isfinite (a.getSample (c, i))) ok = false; }
        }
        check (ok && slowest > 3.0 && peak < 2.0f, String::formatted ("all %d figures at 7th order with the room: finite, peak %.2f, slowest %.1fx realtime", (int) leitner::numFigures, peak, slowest));
    }
    {   // the programs: every one sounds and stays finite; a program keeps the order; your own presets; old sessions
        bool ok = true; String quiet;
        LeitnerProcessor names;
        for (int k = 0; k < names.getNumPrograms(); ++k)
        {
            LeitnerProcessor p; p.setCurrentProgram (k);
            const auto a = run (p, 3.0, true, true, 16);
            bool finite = true; float peak = 0.0f;
            for (int c = 0; c < 16; ++c) { peak = std::max (peak, a.getMagnitude (c, 0, a.getNumSamples())); for (int i = 0; i < a.getNumSamples(); i += 53) if (! std::isfinite (a.getSample (c, i))) finite = false; }
            if (! finite || peak < 0.02f || peak > 1.5f) { ok = false; quiet << p.getProgramName (k) << String::formatted (" (peak %.3f) ", peak); }
        }
        check (ok, String::formatted ("all %d programs sound and stay finite ", names.getNumPrograms()) + quiet);
        int leitnerN = 0, xenakisN = 0;
        for (int k = 0; k < names.getNumPrograms(); ++k) { leitnerN += names.getProgramGroup (k) == "Leitner"; xenakisN += names.getProgramGroup (k) == "Xenakis"; }
        check (leitnerN + xenakisN == names.getNumPrograms(), String::formatted ("%d after Leitner, %d after Xenakis, nothing else", leitnerN, xenakisN));

        LeitnerProcessor p;
        p.setCurrentProgram (2);                                             // Ton-Tor
        check (p.figure->getIndex() == leitner::fgGate && p.getProgramName (p.getCurrentProgram()) == "Ton-Tor", "a program sets its figure");
        setP (p.order, 2.0f); setP (p.walk, 0.9f);
        p.setCurrentProgram (13);                                            // Polytope
        check (p.order->getIndex() == 2 && p.figure->getIndex() == leitner::fgPolytope && p.walk->get() < 0.01f, "a program keeps the order you set and resets the rest");

        const auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("leitner-preset-test");
        dir.deleteRecursively();
        setP (p.turn, 77.0f);
        check (p.saveUserPreset (dir.getChildFile ("mine")) && dir.getChildFile ("mine.leitner").existsAsFile(), "a preset saves to a file");
        LeitnerProcessor q;
        check (q.loadUserPreset (dir.getChildFile ("mine.leitner")) && std::abs (q.turn->get() - 77.0f) < 0.5f && q.figure->getIndex() == leitner::fgPolytope, "and loads back");
        dir.deleteRecursively();
        check (names.userPresetFolder().getFullPathName().contains ("Leitner"), "your presets live in " + names.userPresetFolder().getFullPathName());

        // a session from Leitner 0.2: Cloudfin's state with the space's values as attributes
        XmlElement old ("CloudfinState");
        old.setAttribute ("spFigure", (double) leitner::fgSpiral / 13.0);   // (then there were 14 figures)
        old.setAttribute ("spPoints", p.points->convertTo0to1 (12.0f));
        old.setAttribute ("dlMix", 0.5);
        MemoryBlock mb; AudioProcessor::copyXmlToBinary (old, mb);
        LeitnerProcessor r; r.setStateInformation (mb.getData(), (int) mb.getSize());
        check (r.figure->getIndex() == leitner::fgSpiral && r.points->get() == 12, "a session saved by Leitner 0.2 opens with its space");
    }
    {   // your own figure: the sound goes where you drew it, at your points if you ask, and it is kept with the session
        LeitnerProcessor p; dry (p);
        leitner::Drawn d; d.count = 4; d.closed = true; d.curved = false; d.atPoints = true;
        d.pt[0] = { 0.0f, 0.9f, 0.3f }; d.pt[1] = { 0.0f, 0.0f, 0.9f }; d.pt[2] = { 0.0f, -0.9f, -0.3f }; d.pt[3] = { -0.9f, 0.0f, 0.0f };
        p.setDrawing (d);
        setP (p.figure, (float) leitner::fgDrawn); setP (p.apart, 0.0f); setP (p.size, 1.0f); setP (p.legato, 0.0f); setP (p.seconds, 2.0f);
        const auto a = run (p, 4.0, true, false);
        // with no legato and the speakers at the points, it should sound from the four points, one after another
        std::set<int> places; int other = 0, n = 0;
        for (int i = 9600; i + 480 < a.getNumSamples(); i += 480)
        {
            const auto dir = directionOf (a, i, 480);
            int best = -1; float bestD = 1.0e9f;
            for (int k = 0; k < 4; ++k) { const auto w = d.pt[k].normalised(); const float dd = (dir - w).length(); if (dd < bestD) { bestD = dd; best = k; } }
            ++n;
            if (bestD < 0.08f) places.insert (best); else ++other;
        }
        check (places.size() == 4 && other < n / 5, String::formatted ("your drawing, 4 points as loudspeakers: it sounds from %d of them (%d of %d moments elsewhere)", (int) places.size(), other, n));

        // a held line on an open drawing sits at its first point
        LeitnerProcessor h; dry (h);
        leitner::Drawn e; e.count = 3; e.closed = false; e.pt[0] = { 0.2f, 0.6f, 0.5f }; e.pt[1] = { 0.7f, 0.0f, 0.0f }; e.pt[2] = { 0.0f, -0.5f, -0.5f };
        h.setDrawing (e);
        setP (h.figure, (float) leitner::fgDrawn); setP (h.apart, 0.0f); setP (h.size, 1.0f); setP (h.hold, 1.0f); setP (h.points, (float) leitner::kMaxPoints);
        const auto b = run (h, 0.5, true, false);
        const auto dir = directionOf (b, 12000, 8000), want = e.pt[0].normalised();
        check ((dir - want).length() < 0.02f, String::formatted ("held at the start of your line: (%.3f, %.3f, %.3f), drawn (%.3f, %.3f, %.3f)", dir.x, dir.y, dir.z, want.x, want.y, want.z));

        // even speed: the table's steps are all the same length
        const auto& pe = h.drawing();
        float lo = 1.0e9f, hi = 0.0f;
        for (int k = 0; k < leitner::Drawn::kTable; ++k) { const float l = (pe.table[k + 1] - pe.table[k]).length(); lo = std::min (lo, l); hi = std::max (hi, l); }
        check (hi < lo * 1.05f, String::formatted ("the line moves along your drawing at an even speed (steps %.4f to %.4f)", lo, hi));

        MemoryBlock mb; p.getStateInformation (mb);
        LeitnerProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto& r = q.drawing();
        bool same = r.count == 4 && r.closed && ! r.curved && r.atPoints;
        for (int k = 0; k < 4 && same; ++k) same = (r.pt[k] - d.pt[k]).length() < 1.0e-3f;
        check (same && q.figure->getIndex() == leitner::fgDrawn, "your drawing comes back with the session");

        // a session saved by 0.3 (14 figures) opens on the same figure
        XmlElement old ("LEITNER"); old.setAttribute ("version", 3); old.setAttribute ("spFigure", (double) leitner::fgPolytope / 13.0);
        MemoryBlock ob; AudioProcessor::copyXmlToBinary (old, ob);
        LeitnerProcessor o; o.setStateInformation (ob.getData(), (int) ob.getSize());
        check (o.figure->getIndex() == leitner::fgPolytope, "a session saved by Leitner 0.3 opens on its figure");

        // starting from a figure, and a freehand stroke thinned to at most 64 points
        LeitnerProcessor f; lt::Drawing dr (f);
        dr.fromFigure (leitner::fgCube);
        check (f.drawing().count == 8 && f.drawing().closed && ! f.drawing().curved, "the sound cube as a drawing: its 8 corners, closed, straight");
        std::vector<leitner::Vec3> stroke;
        for (int i = 0; i <= 2000; ++i) { const float t = (float) i / 2000.0f * 6.2831853f; stroke.push_back ({ 0.8f * std::cos (t), 0.8f * std::sin (3.0f * t), 0.5f * std::sin (t) }); }
        dr.begin(); dr.setStroke (stroke, true);   // (the window begins every change, for undo)
        check (f.drawing().count >= 8 && f.drawing().count <= leitner::kMaxDrawn && f.drawing().closed, String::formatted ("a freehand stroke of 2001 points becomes %d, a closed loop", f.drawing().count));
        dr.undo();
        check (f.drawing().count == 8, "undo");
    }
    {   // the input is not changed: held still and continuous, the omni channel is the input times one gain
        LeitnerProcessor p; dry (p);
        setP (p.hold, 1.0f); setP (p.points, (float) leitner::kMaxPoints);
        auto a = run (p, 0.3, true, false, 16);
        auto in = [] (int i) { return 0.25f * (float) std::sin (2.0 * MathConstants<double>::pi * 440.0 * i / SR); };
        const float gain = a.getSample (0, 8001) / in (8001);
        double err = 0.0;
        for (int i = 4000; i < 12000; ++i) err = std::max (err, (double) std::abs (a.getSample (0, i) - gain * in (i)));
        check (err < 1.0e-4, String::formatted ("the sound itself passes unchanged (largest difference from a pure gain %.6f)", err));
    }
    {   // the state, and a stereo track
        LeitnerProcessor p;
        setP (p.figure, (float) leitner::fgGate); setP (p.order, 2.0f); setP (p.points, 12.0f); setP (p.lift, 0.7f); p.viewYaw.store (33.0f);
        MemoryBlock mb; p.getStateInformation (mb);
        LeitnerProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.figure->getIndex() == leitner::fgGate && q.order->getIndex() == 2 && q.points->get() == 12 && std::abs (q.lift->get() - 0.7f) < 0.01f
               && std::abs (q.viewYaw.load() - 33.0f) < 0.01f, "the space and the turned room come back with the session");
        LeitnerProcessor s; dry (s);
        const auto a = run (s, 0.5, true, true, 2);
        check (a.getMagnitude (0, 0, a.getNumSamples()) > 0.05f && s.orderNow.load() == 0, "on a stereo track a left-right fold comes out");
    }
    {   // what a host sees: the same 21 parameters, by the same names, in the same order, with the same ranges and defaults
        struct Want { const char* id; float lo, hi, def; int steps; };
        const Want want[] = { { "spOrder", 0, 4, 0, 5 }, { "spFigure", 0, 14, 0, 15 }, { "spSeconds", 0.05f, 60, 4, 0 }, { "spRest", 0, 0.9f, 0, 0 }, { "spSize", 0.1f, 1, 0.9f, 0 },
                              { "spHeight", -1, 1, 0, 0 }, { "spTurn", -180, 180, 0, 0 }, { "spApart", 0, 90, 30, 0 }, { "spCounter", 0, 1, 0, 0 }, { "spPoints", 2, 33, 8, 32 },
                              { "spLegato", 0, 1, 0.5f, 0 }, { "spSwell", 0, 1, 0, 0 }, { "spLift", 0, 1, 0, 0 }, { "spChance", 0, 1, 0, 0 }, { "spWalk", 0, 1, 0, 0 },
                              { "spScatter", 0, 1, 0, 0 }, { "spSieve", 0, 4, 0, 5 }, { "spRoom", 0, 1, 0.15f, 0 }, { "spRoomDecay", 0.3f, 20, 3, 0 }, { "spLevel", 0, 1.5f, 1, 0 },
                              { "spHold", 0, 1, 0, 2 } };
        LeitnerProcessor p;
        bool ok = p.getParameters().size() == (int) std::size (want); String wrong;
        for (int i = 0; ok && i < (int) std::size (want); ++i)
        {
            auto* r = dynamic_cast<RangedAudioParameter*> (p.getParameters()[i]);
            const auto range = r->getNormalisableRange();
            const bool same = r->getParameterID() == want[i].id && r->getVersionHint() == 1 && std::abs (range.start - want[i].lo) < 1.0e-6f && std::abs (range.end - want[i].hi) < 1.0e-6f
                              && std::abs (r->convertFrom0to1 (r->getDefaultValue()) - want[i].def) < 1.0e-4f && (want[i].steps == 0 || r->getNumSteps() == want[i].steps);
            if (! same) { ok = false; wrong = want[i].id; }
        }
        check (ok, "the 21 parameters a host sees are the ones 0.4 had: ids, order, ranges, defaults " + wrong);
        check (p.getName() == "Leitner" && p.getNumPrograms() == 18 && LeitnerProcessor::presetExtension() == ".leitner", "the name, the 18 programs and the .leitner presets are as they were");

        // it opens the same every time: every parameter at its default, the first program, the saddle as your drawing, nothing by chance
        LeitnerProcessor q;
        bool atDefault = p.getCurrentProgram() == 0;
        for (auto* prm : p.getParameters()) atDefault = atDefault && std::abs (prm->getValue() - prm->getDefaultValue()) < 1.0e-6f;
        MemoryBlock a, b; p.getStateInformation (a); q.getStateInformation (b);
        check (atDefault && a == b, "a new Leitner opens at its defaults, and two new ones are the same to the byte");
        const auto one = run (p, 1.0, true, true, 16), two = run (q, 1.0, true, true, 16);
        bool sameSound = true;
        for (int c = 0; c < 16 && sameSound; ++c) for (int i = 0; i < one.getNumSamples(); i += 7) if (one.getSample (c, i) != two.getSample (c, i)) { sameSound = false; break; }
        check (sameSound, "and sound the same, sample for sample");

        // the state a session keeps: the same element, version and attributes as 0.4 wrote
        const auto x = AudioProcessor::getXmlFromBinary (a.getData(), (int) a.getSize());
        bool stateOk = x != nullptr && x->hasTagName ("LEITNER") && x->getIntAttribute ("version") == 4 && x->getChildByName ("DRAWING") != nullptr
                       && x->hasAttribute ("ltProgram") && x->hasAttribute ("viewYaw") && x->hasAttribute ("viewPitch") && x->getNumAttributes() == 21 + 4;
        for (auto& w : want) stateOk = stateOk && x->hasAttribute (w.id);
        check (stateOk, "the state is written as 0.4 wrote it (LEITNER, version 4, the 21 values, the program, the view, the drawing)");
    }
    {   // the window (in the faltung look): every parameter has its control, and the controls do what they did
        LeitnerProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (LeitnerEditor::designW, LeitnerEditor::designH);
        ed->setVisible (true);
        MessageManager::getInstance()->runDispatchLoopUntil (60);
        std::set<AudioProcessorParameter*> held;
        std::vector<fal::ui::ParamSlider*> sliders; std::vector<fal::ui::ChoiceBox*> choices; std::vector<fal::ui::ToggleBox*> toggles; std::vector<fal::ui::TextLink*> links;
        all (*ed, sliders); all (*ed, choices); all (*ed, toggles); all (*ed, links);
        for (auto* c : sliders) held.insert (&c->param);
        for (auto* c : choices) held.insert (&c->param);
        for (auto* c : toggles) held.insert (&c->param);
        bool every = true; String missing;
        for (auto* prm : p.getParameters()) if (held.count (prm) == 0) { every = false; missing << prm->getName (20) << " "; }
        check (every && sliders.size() == 17 && choices.size() == 3 && toggles.size() == 1,
               String::formatted ("every parameter has its control: %d rows, %d lists, %d switch ", (int) sliders.size(), (int) choices.size(), (int) toggles.size()) + missing);
        auto* room = find<lt::Room> (*ed); auto* strip = find<lt::FigureStrip> (*ed); auto* sheet = find<lt::DrawSheet> (*ed); auto* plan = find<lt::Plan> (*ed);
        auto* presets = find<lt::Presets> (*ed);
        check (room != nullptr && strip != nullptr && sheet != nullptr && plan != nullptr && find<lt::Chance> (*ed) != nullptr && presets != nullptr && links.size() == 8,
               String::formatted ("the room, Leitner's side, the drawing sheet, Xenakis's side, the strips, the programs and %d text buttons are there", (int) links.size()));

        // the strips: a click on the sixth is Kreis
        const float slot = (float) strip->getWidth() / 15.0f;
        click (*strip, { 5.5f * slot, 18.0f });
        check (p.figure->getIndex() == leitner::fgCircle, "a click on the sixth strip chooses Kreis");
        // a row: dragged to the right it rises; a mirrored one rises to the left; double-click puts it back
        auto sliderFor = [&] (AudioProcessorParameter* prm) { for (auto* c : sliders) if (&c->param == prm) return c; return (fal::ui::ParamSlider*) nullptr; };
        auto* rest = sliderFor (p.rest); auto* size = sliderFor (p.size);
        drag (*rest, { 120.0f, 10.0f }, { 180.0f, 10.0f });
        const float restUp = p.rest->get();
        setP (p.size, 0.5f);
        drag (*size, { 200.0f, 10.0f }, { 150.0f, 10.0f });
        const float sizeUp = p.size->get();
        rest->mouseDoubleClick (ev (*rest, { 120.0f, 10.0f }, { 120.0f, 10.0f }, ModifierKeys::leftButtonModifier, 2));
        check (restUp > 0.1f && sizeUp > 0.55f && p.rest->get() == 0.0f,
               String::formatted ("rows: dragged right, rest goes to %.0f %%; the mirrored size, dragged left, to %.0f %%; a double-click puts rest back", restUp * 100.0f, sizeUp * 100.0f));
        // the switch
        click (*toggles[0], { 8.0f, 10.0f });
        check (p.hold->get(), "the switch holds the lines");
        click (*toggles[0], { 8.0f, 10.0f });
        // the room turns, and a double-click faces front again
        drag (*room, { 300.0f, 200.0f }, { 240.0f, 150.0f });
        const float yaw = p.viewYaw.load(), pitch = p.viewPitch.load();
        room->mouseDoubleClick (ev (*room, { 300.0f, 200.0f }, { 300.0f, 200.0f }, ModifierKeys::leftButtonModifier, 2));
        check (std::abs (yaw - 27.0f) < 0.5f && std::abs (pitch + 20.0f) < 0.5f && p.viewYaw.load() == 0.0f && p.viewPitch.load() == 0.0f,
               String::formatted ("dragging the room turns it (to %.0f, %.0f degrees) and a double-click faces front again", yaw, pitch));
        // the programs: the arrows step
        click (*presets, { (float) presets->getWidth() - 6.0f, 10.0f });
        check (p.getCurrentProgram() == 1, "the arrow beside the program's name steps to the next: " + p.getProgramName (p.getCurrentProgram()));
        click (*presets, { 6.0f, 10.0f });
        // your own figure: the text button goes there and back; the sheet takes clicks, drags, deletes and undo; the room moves a point
        fal::ui::TextLink* own = nullptr;
        for (auto* l : links) if (l->text == "draw your own figure") own = l;
        check (own != nullptr, "the button to your own figure is there");
        if (own != nullptr)
        {
            // (the window follows on its timer: on a slow machine give it up to three seconds, not a fixed moment)
            auto settle = [] (std::function<bool()> done)
            {
                for (int t = 0; t < 150 && ! done(); ++t) MessageManager::getInstance()->runDispatchLoopUntil (20);
            };
            MessageManager::getInstance()->runDispatchLoopUntil (80);    // (the program chosen above lands first)
            setP (p.figure, (float) leitner::fgGate);
            MessageManager::getInstance()->runDispatchLoopUntil (80);
            settle ([&] { return p.figure->getIndex() == leitner::fgGate && own->text == "draw your own figure" && plan->isVisible(); });
            click (*own, { 10.0f, 10.0f });
            settle ([&] { return own->text == "back to ton-tor" && sheet->isVisible() && ! plan->isVisible(); });
            check (p.figure->getIndex() == leitner::fgDrawn && sheet->isVisible() && ! plan->isVisible() && own->text == "back to ton-tor", "it opens the drawing sheet, and reads: " + own->text);
            setP (p.apart, 0.0f); setP (p.size, 1.0f);
            const int before = p.drawing().count;
            click (*sheet, { 30.0f, 72.0f });                                    // a new point, from above, at the front left
            const auto added = p.drawing();
            drag (*sheet, { 30.0f, 72.0f }, { 60.0f, 102.0f });                  // and moved
            const auto moved = p.drawing();
            int at = -1; for (int k = 0; k < moved.count; ++k) if (std::abs (moved.pt[k].y - added.pt[k].y) > 0.1f) at = k;
            check (added.count == before + 1 && moved.count == before + 1 && at >= 0 && moved.pt[at].x < added.pt[at].x - 0.2f,
                   String::formatted ("a click on the sheet adds a point (%d to %d); dragged, it moves", before, added.count));
            sheet->mouseDoubleClick (ev (*sheet, { 60.0f, 102.0f }, { 60.0f, 102.0f }, ModifierKeys::leftButtonModifier, 2));
            check (p.drawing().count == before, "a double-click takes it away");
            sheet->keyPressed (KeyPress ('z', ModifierKeys::commandModifier, 0));
            check (p.drawing().count == before + 1, "cmd-Z brings it back");
            // the six beside the third view
            fal::ui::TextLink* closed = nullptr;
            for (auto* l : links) if (l->text == "a closed loop" || l->text == "open: there and back") closed = l;
            const bool was = p.drawing().closed;
            if (closed != nullptr) click (*closed, { 30.0f, 10.0f });
            check (closed != nullptr && p.drawing().closed != was, "the loop opens and closes");
            if (closed != nullptr) click (*closed, { 30.0f, 10.0f });
            // in the room: take the first point by its square and move it across the view
            p.viewYaw.store (0.0f); p.viewPitch.store (0.0f);
            const auto d0 = p.drawing();
            const float k = 0.31f * (float) room->getHeight() * (1.64f / (d0.pt[0].x + 2.64f));
            const Point<float> at0 ((float) room->getWidth() * 0.5f - d0.pt[0].y * k, (float) room->getHeight() * 0.5f + 4.0f - d0.pt[0].z * k);
            drag (*room, at0, at0 + Point<float> (-30.0f, -20.0f));
            check (p.drawing().pt[0].y > d0.pt[0].y + 0.1f && p.drawing().pt[0].z > d0.pt[0].z + 0.05f && std::abs (p.drawing().pt[0].x - d0.pt[0].x) < 0.01f,
                   "in the room a point is moved across the view, at its own depth");
            // shift-drag in the room: a new path on the plane through the middle
            const auto mods = ModifierKeys (ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier);
            room->mouseDown (ev (*room, { 150.0f, 120.0f }, { 150.0f, 120.0f }, mods));
            for (int i = 1; i <= 60; ++i)
            {
                const float t = (float) i / 60.0f * 6.2831853f;
                room->mouseDrag (ev (*room, { 280.0f + 130.0f * std::cos (t + 3.1416f), 230.0f + 110.0f * std::sin (t + 3.1416f) }, { 150.0f, 120.0f }, mods, 1, true));
            }
            room->mouseUp (ev (*room, { 150.0f, 230.0f }, { 150.0f, 120.0f }, mods, 1, true));
            float deepest = 0.0f; for (int i = 0; i < p.drawing().count; ++i) deepest = std::max (deepest, std::abs (p.drawing().pt[i].x));
            check (p.drawing().count >= 6 && deepest < 0.02f, String::formatted ("shift-drag in the room draws a new path of %d points on the middle plane", p.drawing().count));
            click (*own, { 10.0f, 10.0f });
            settle ([&] { return plan->isVisible(); });
            check (p.figure->getIndex() == leitner::fgGate && plan->isVisible(), "and back to the figure it came from");
        }
    }
    {   // pictures of the window: as it opens, from the front, turned, Xenakis's figures, your own, 7th order, a stereo track;
        // each whole and in its parts (the manual is made of these); and how nearly its two halves mirror
        struct Shot { int figure; float yaw, pitch, scatter; const char* name; int channels = 64; };
        const Shot shots[] = { { leitner::fgLine, 0.0f, 0.0f, 0.0f, "default", 16 }, { leitner::fgGate, 0.0f, 0.0f, 0.0f, "front" }, { leitner::fgSoundcube, 38.0f, 24.0f, 0.0f, "turned" },
                               { leitner::fgParaboloid, -30.0f, 18.0f, 0.0f, "paraboloid" }, { leitner::fgPolytope, 25.0f, 30.0f, 0.4f, "polytope" }, { leitner::fgCircle, 30.0f, 35.0f, 0.0f, "xenakis" },
                               { leitner::fgDrawn, 32.0f, 22.0f, 0.0f, "drawn" }, { leitner::fgSpiral, 150.0f, 12.0f, 0.0f, "seventh" }, { leitner::fgCube, -20.0f, 15.0f, 0.0f, "stereo", 2 } };
        for (auto& sh : shots)
        {
            LeitnerProcessor p;
            const String name (sh.name);
            if (name != "default")
            {
                setP (p.figure, (float) sh.figure); setP (p.seconds, 3.0f); setP (p.points, 12.0f); setP (p.scatter, sh.scatter);
            }
            if (sh.figure == leitner::fgGate) setP (p.counter, 1.0f);
            if (name == "seventh") { setP (p.order, 4.0f); setP (p.points, 24.0f); setP (p.lift, 0.4f); }
            if (name == "stereo") setP (p.points, 8.0f);
            if (sh.figure == leitner::fgDrawn)
            {
                // a knot of a path, picked at its fifth point
                leitner::Drawn d; d.count = 12;
                for (int k = 0; k < 12; ++k) { const float t = (float) k / 12.0f * 6.2831853f; d.pt[k] = { 0.55f * (std::sin (t) + 0.6f * std::sin (2.0f * t)), 0.55f * (std::cos (t) - 0.6f * std::cos (2.0f * t)), -0.55f * std::sin (3.0f * t) }; }
                p.setDrawing (d); setP (p.apart, 0.0f); setP (p.points, 24.0f);
            }
            const bool xen = name == "xenakis" || name == "polytope";
            if (xen) { setP (p.chance, 0.8f); setP (p.walk, 0.6f); setP (p.sieve, 2.0f); setP (p.points, 16.0f); setP (p.seconds, 0.25f); }
            p.viewYaw.store (sh.yaw); p.viewPitch.store (sh.pitch);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (LeitnerEditor::designW, LeitnerEditor::designH);
            ed->setVisible (true);
            if (sh.figure == leitner::fgDrawn)
            {
                // (pick the fifth point as a click on the sheet would)
                MessageManager::getInstance()->runDispatchLoopUntil (80);
                if (auto* sheet = find<lt::DrawSheet> (*ed))
                {
                    const auto v = p.drawing().pt[4];
                    const Point<float> at (100.0f - v.y * 90.0f, 142.0f - v.x * 90.0f);
                    sheet->mouseDown (ev (*sheet, at, at)); sheet->mouseUp (ev (*sheet, at, at));
                }
            }
            // let some passes go by (the Xenakis side draws the passes it has seen)
            if (xen)
            {
                // one run, many passes: the window watches between blocks
                p.setPlayConfigDetails (2, 64, SR, BLOCK); p.prepareToPlay (SR, BLOCK);
                AudioBuffer<float> buf (64, BLOCK); MidiBuffer midi; Random rnd (3);
                for (int k = 0; k < 40; ++k)
                {
                    for (int blk = 0; blk < 40; ++blk)
                    {
                        buf.clear();
                        for (int i = 0; i < BLOCK; ++i) { buf.setSample (0, i, 0.2f * (rnd.nextFloat() - 0.5f)); buf.setSample (1, i, 0.2f * (rnd.nextFloat() - 0.5f)); }
                        p.processBlock (buf, midi);
                    }
                    MessageManager::getInstance()->runDispatchLoopUntil (40);
                }
            }
            else run (p, 0.8, true, true, sh.channels);
            if (xen) setP (p.seconds, 3.0f);
            MessageManager::getInstance()->runDispatchLoopUntil (150);
            const float scale = 1.5f;
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            auto save = [&] (const Image& im, const String& part)
            {
                const auto f = outDir.getChildFile ("leitner_" + name + (part.isEmpty() ? "" : "_" + part) + ".png"); f.deleteFile();
                FileOutputStream os (f);
                PNGImageFormat().writeImageToStream (im, os);
            };
            save (img, {});
            auto part = [&] (Rectangle<int> r, const String& what) { save (img.getClippedImage ((r.toFloat() * scale).toNearestInt()), what); };
            part (lt::lay::stage(), "room");
            part (lt::lay::left().expanded (6, 4), "left");
            part (lt::lay::right().expanded (6, 4), "right");
            part (lt::lay::strips().expanded (4, 2), "strips");
            part (lt::lay::controls(), "controls");
            if (name == "front")
            {
                // ink on the left half against ink on the right half, in 30-pixel cells: text can't mirror letter for
                // letter, but where there is ink on one side there should be ink on the other
                const int W = img.getWidth(), H = img.getHeight(), cell = 30;
                const float ground = lt::paper.getBrightness();
                int same = 0, total = 0;
                for (int cy = 0; cy + cell <= H; cy += cell)
                    for (int cx = 0; cx + cell <= W / 2; cx += cell)
                    {
                        int l = 0, r = 0;
                        for (int y = cy; y < cy + cell; y += 2)
                            for (int x = cx; x < cx + cell; x += 2)
                            {
                                if (img.getPixelAt (x, y).getBrightness() < ground - 0.06f) ++l;
                                if (img.getPixelAt (W - 1 - x, y).getBrightness() < ground - 0.06f) ++r;
                            }
                        ++total; if ((l > 2) == (r > 2)) ++same;
                    }
                // (the sides mirror in their layout, not their drawings: Leitner's plan and elevation on
                // the left, Xenakis's chance, walk and sieve on the right)
                check (same > total * 70 / 100, String::formatted ("the window mirrors: %d of %d cells have ink on both sides or neither", same, total));
                // and it is in the faltung look, on Leitner's paper: the ground is the cream it has always had
                const auto corner = img.getPixelAt (8, 8), field = img.getPixelAt (roundToInt ((lt::lay::stageX + 6) * scale), roundToInt ((lt::lay::bandY + 60) * scale));
                check (corner == lt::paper && field == lt::desk, "the ground is Leitner's paper (" + corner.toDisplayString (false) + ") and the room its one darker field (" + field.toDisplayString (false) + ")");
            }
            if (name == "stereo") check (p.orderNow.load() == 0, "on a stereo track the window says so");
        }
    }
    std::printf ("\n%s (%d failed)\n", failures == 0 ? "all good" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
