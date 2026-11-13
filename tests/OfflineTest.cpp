// Offline checks for Leitner (not shipped): the Ambisonic encoding (direction, mirror image, the
// raster's steps, every order), the state, CPU, and a picture of the window.
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

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
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
        old.setAttribute ("spFigure", p.figure->convertTo0to1 ((float) leitner::fgSpiral));
        old.setAttribute ("spPoints", p.points->convertTo0to1 (12.0f));
        old.setAttribute ("dlMix", 0.5);
        MemoryBlock mb; AudioProcessor::copyXmlToBinary (old, mb);
        LeitnerProcessor r; r.setStateInformation (mb.getData(), (int) mb.getSize());
        check (r.figure->getIndex() == leitner::fgSpiral && r.points->get() == 12, "a session saved by Leitner 0.2 opens with its space");
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
    {   // the window: from the front, turned, and one of Xenakis's figures; and how nearly its two halves mirror
        struct Shot { int figure; float yaw, pitch, scatter; const char* name; };
        const Shot shots[] = { { leitner::fgGate, 0.0f, 0.0f, 0.0f, "front" }, { leitner::fgSoundcube, 38.0f, 24.0f, 0.0f, "turned" },
                               { leitner::fgParaboloid, -30.0f, 18.0f, 0.0f, "paraboloid" }, { leitner::fgPolytope, 25.0f, 30.0f, 0.4f, "polytope" }, { leitner::fgCircle, 30.0f, 35.0f, 0.0f, "xenakis" } };
        for (auto& sh : shots)
        {
            LeitnerProcessor p;
            setP (p.figure, (float) sh.figure); setP (p.seconds, 3.0f); setP (p.points, 12.0f); setP (p.scatter, sh.scatter);
            if (sh.figure == leitner::fgGate) setP (p.counter, 1.0f);
            const bool xen = String (sh.name) == "xenakis" || String (sh.name) == "polytope";
            if (xen) { setP (p.chance, 0.8f); setP (p.walk, 0.6f); setP (p.sieve, 2.0f); setP (p.points, 16.0f); setP (p.seconds, 0.25f); }
            p.viewYaw.store (sh.yaw); p.viewPitch.store (sh.pitch);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            ed->setSize (1500, 790);
            ed->setVisible (true);
            // let some passes go by (the Xenakis sheet draws the passes it has seen)
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
            else run (p, 0.8, true, true);
            if (xen) setP (p.seconds, 3.0f);
            MessageManager::getInstance()->runDispatchLoopUntil (150);
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            const auto f = outDir.getChildFile ("leitner_" + String (sh.name) + ".png"); f.deleteFile();
            FileOutputStream os (f);
            PNGImageFormat().writeImageToStream (img, os);
            if (sh.yaw == 0.0f)
            {
                // ink on the left half against ink on the right half, in 30-pixel cells: text can't mirror letter for
                // letter, but where there is ink on one side there should be ink on the other
                const int W = img.getWidth(), H = img.getHeight(), cell = 30;
                int same = 0, total = 0;
                for (int cy = 0; cy + cell <= H; cy += cell)
                    for (int cx = 0; cx + cell <= W / 2; cx += cell)
                    {
                        int l = 0, r = 0;
                        for (int y = cy; y < cy + cell; y += 2)
                            for (int x = cx; x < cx + cell; x += 2)
                            {
                                if (img.getPixelAt (x, y).getBrightness() < 0.6f) ++l;
                                if (img.getPixelAt (W - 1 - x, y).getBrightness() < 0.6f) ++r;
                            }
                        ++total; if ((l > 2) == (r > 2)) ++same;
                    }
                // (the side sheets mirror in their layout, not their drawings: Leitner's plan and elevation on
                // the left, Xenakis's sieve, chance and walk on the right)
                check (same > total * 70 / 100, String::formatted ("the window mirrors: %d of %d cells have ink on both sides or neither", same, total));
            }
        }
    }
    std::printf ("\n%s (%d failed)\n", failures == 0 ? "all good" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
