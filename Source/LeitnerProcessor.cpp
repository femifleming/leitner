#include "LeitnerProcessor.h"
#include "LeitnerEditor.h"

using namespace juce;

LeitnerProcessor::LeitnerProcessor() : AudioProcessor (buses())
{
    auto secs = [] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };
    auto pct  = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto deg  = [] (float v, int) { return String (roundToInt (v)) + String::fromUTF8 ("\xc2\xb0"); };
    using A = AudioParameterFloatAttributes;
    auto f = [this] (AudioParameterFloat*& slot, const char* id, const char* name, NormalisableRange<float> r, float def, A attr)
    {
        auto p = std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr);
        slot = p.get(); spaceParams.push_back (slot); addParameter (p.release());
    };
    auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    {
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spOrder", 1 }, "Ambisonic Order", StringArray { "3rd", "4th", "5th", "6th", "7th" }, 0);
        order = p.get(); spaceParams.push_back (order); addParameter (p.release());
    }
    {
        StringArray names;
        for (int i = 0; i < leitner::numFigures; ++i) names.add (String::fromUTF8 (leitner::figureNames()[i]));
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spFigure", 1 }, "Figure", names, 0);
        figure = p.get(); spaceParams.push_back (figure); addParameter (p.release());
    }
    f (seconds, "spSeconds", "Pass Time", skew (0.05f, 60.0f, 4.0f), 4.0f, A().withStringFromValueFunction (secs));
    f (rest,    "spRest",    "Rest",     { 0.0f, 0.9f }, 0.0f, A().withStringFromValueFunction (pct));
    f (size,    "spSize",    "Size",     { 0.1f, 1.0f }, 0.9f, A().withStringFromValueFunction (pct));
    f (height,  "spHeight",  "Height",   { -1.0f, 1.0f }, 0.0f, A().withStringFromValueFunction ([] (float v, int) { return (v > 0.004f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; }));
    f (turn,    "spTurn",    "Turn",     { -180.0f, 180.0f }, 0.0f, A().withStringFromValueFunction (deg));
    f (apart,   "spApart",   "Apart",    { 0.0f, 90.0f }, 30.0f, A().withStringFromValueFunction (deg));
    f (counter, "spCounter", "Counter",  { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    {
        auto p = std::make_unique<AudioParameterInt> (ParameterID { "spPoints", 1 }, "Points", 2, leitner::kMaxPoints, 8,
                     AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v >= leitner::kMaxPoints ? String ("continuous") : String (v); }));
        points = p.get(); spaceParams.push_back (points); addParameter (p.release());
    }
    f (legato,  "spLegato",  "Legato",   { 0.0f, 1.0f }, 0.5f, A().withStringFromValueFunction (pct));
    f (swell,   "spSwell",   "Swell",    { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (lift,    "spLift",    "Lift",     { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (chance,  "spChance",  "Chance",   { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (walk,    "spWalk",    "Walk",     { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    f (scatter, "spScatter", "Scatter",  { 0.0f, 1.0f }, 0.0f, A().withStringFromValueFunction (pct));
    {
        StringArray names;
        for (int i = 0; i < leitner::kNumSieves; ++i) names.add (leitner::sieveNames()[i]);
        auto p = std::make_unique<AudioParameterChoice> (ParameterID { "spSieve", 1 }, "Sieve", names, 0);
        sieve = p.get(); spaceParams.push_back (sieve); addParameter (p.release());
    }
    f (room,    "spRoom",    "Room",     { 0.0f, 1.0f }, 0.15f, A().withStringFromValueFunction (pct));
    f (roomDecay, "spRoomDecay", "Room Decay", skew (0.3f, 20.0f, 3.0f), 3.0f, A().withStringFromValueFunction (secs));
    f (level,   "spLevel",   "Level",    { 0.0f, 1.5f }, 1.0f, A().withStringFromValueFunction (pct));
    {
        auto p = std::make_unique<AudioParameterBool> (ParameterID { "spHold", 1 }, "Hold", false);
        hold = p.get(); spaceParams.push_back (hold); addParameter (p.release());
    }
}

//==============================================================================
namespace
{
struct SpaceValue { const char* id; float v; };
struct SpaceProgram { const char* name; const char* group; std::vector<SpaceValue> space; };

// figures by number, as in leitner::Figure
enum { Line = 0, Swing, Liege, Gate, Vertical, Circle, Soundcube, Spiral, Cube, Expand, Vault, Paraboloid, Terretektorh, Polytope };

const std::vector<SpaceProgram>& spacePrograms()
{
    static const std::vector<SpaceProgram> list {
        // after Leitner
        { "Ton-Linie",           "Leitner", {} },
        { "Through the Body",    "Leitner", { { "spFigure", Line }, { "spHeight", 0.0f }, { "spApart", 0.0f }, { "spSwell", 0.8f }, { "spSeconds", 3.0f }, { "spPoints", 12 }, { "spLegato", 0.7f } } },
        { "Ton-Tor",             "Leitner", { { "spFigure", Gate }, { "spCounter", 1.0f }, { "spLift", 0.7f }, { "spSeconds", 5.0f }, { "spPoints", 19 }, { "spLegato", 0.3f }, { "spApart", 20.0f } } },
        { "Schwingen",           "Leitner", { { "spFigure", Swing }, { "spPoints", 33 }, { "spSeconds", 5.0f }, { "spRoom", 0.3f }, { "spApart", 15.0f } } },
        { "Ton-Liege",           "Leitner", { { "spFigure", Liege }, { "spPoints", 4 }, { "spLegato", 1.0f }, { "spSeconds", 9.0f }, { "spApart", 10.0f }, { "spSwell", 0.4f } } },
        { "Soundcube 1969",      "Leitner", { { "spFigure", Soundcube }, { "spPoints", 32 }, { "spLegato", 0.2f }, { "spSeconds", 14.0f }, { "spRoom", 0.1f } } },
        { "Vertical Space",      "Leitner", { { "spFigure", Vertical }, { "spCounter", 1.0f }, { "spLift", 0.6f }, { "spPoints", 6 }, { "spLegato", 0.1f }, { "spSeconds", 2.5f }, { "spApart", 45.0f } } },
        { "Spiral Space",        "Leitner", { { "spFigure", Spiral }, { "spPoints", 24 }, { "spLegato", 0.5f }, { "spSeconds", 10.0f }, { "spLift", 0.4f } } },
        { "Ton-W\xc3\xbcrfel",   "Leitner", { { "spFigure", Cube }, { "spPoints", 8 }, { "spLegato", 0.0f }, { "spSeconds", 6.0f }, { "spCounter", 0.5f } } },
        { "Widening, Narrowing", "Leitner", { { "spFigure", Expand }, { "spApart", 90.0f }, { "spPoints", 33 }, { "spSeconds", 7.0f }, { "spSwell", 0.3f }, { "spLevel", 0.8f } } },
        { "Vault",               "Leitner", { { "spFigure", Vault }, { "spPoints", 33 }, { "spSeconds", 20.0f }, { "spRoom", 0.4f }, { "spRoomDecay", 8.0f } } },
        // after Xenakis
        { "Ruled Surface",       "Xenakis", { { "spFigure", Paraboloid }, { "spPoints", 33 }, { "spCounter", 1.0f }, { "spSeconds", 8.0f }, { "spSize", 1.0f } } },
        { "Terretektorh",        "Xenakis", { { "spFigure", Terretektorh }, { "spPoints", 24 }, { "spLegato", 0.6f }, { "spSeconds", 6.0f }, { "spCounter", 0.5f } } },
        { "Polytope",            "Xenakis", { { "spFigure", Polytope }, { "spChance", 1.0f }, { "spScatter", 0.4f }, { "spRest", 0.2f }, { "spPoints", 8 }, { "spLegato", 1.0f }, { "spSeconds", 4.0f } } },
        // Persephassa (1969): six percussionists round the audience, the sound circling and speeding up
        { "Persephassa",         "Xenakis", { { "spFigure", Circle }, { "spPoints", 6 }, { "spLegato", 0.25f }, { "spSeconds", 1.6f }, { "spCounter", 1.0f }, { "spChance", 0.3f }, { "spRoom", 0.2f } } },
        // Nomos Gamma (1968): the orchestra scattered among the listeners
        { "Nomos Gamma",         "Xenakis", { { "spFigure", Polytope }, { "spPoints", 16 }, { "spLegato", 0.4f }, { "spScatter", 0.7f }, { "spChance", 0.6f }, { "spWalk", 0.3f }, { "spSeconds", 5.0f }, { "spRoom", 0.25f } } },
        { "Sieve Ring",          "Xenakis", { { "spFigure", Circle }, { "spPoints", 16 }, { "spLegato", 0.0f }, { "spSieve", 2 }, { "spChance", 0.6f }, { "spSeconds", 3.0f } } },
        { "Brownian Line",       "Xenakis", { { "spFigure", Line }, { "spWalk", 0.7f }, { "spSeconds", 6.0f }, { "spPoints", 33 }, { "spSwell", 0.5f } } },
    };
    return list;
}
}

int LeitnerProcessor::getNumPrograms() { return (int) spacePrograms().size(); }

const String LeitnerProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, getNumPrograms()) ? String::fromUTF8 (spacePrograms()[(size_t) i].name) : String();
}

String LeitnerProcessor::getProgramGroup (int i)
{
    return isPositiveAndBelow (i, getNumPrograms()) ? String (spacePrograms()[(size_t) i].group) : String();
}

void LeitnerProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    const auto& pr = spacePrograms()[(size_t) index];
    const float keepOrder = static_cast<RangedAudioParameter*> (order)->getValue();   // (the order belongs to the track, not the program)
    for (auto* prm : spaceParams) prm->setValueNotifyingHost (prm->getDefaultValue());
    for (auto& v : pr.space)
        for (auto* prm : spaceParams)
            if (prm->getParameterID() == v.id) prm->setValueNotifyingHost (prm->convertTo0to1 (v.v));
    order->setValueNotifyingHost (keepOrder);
    program.store (index);
}

// a random space: any figure, a raster, and Xenakis's chance in some measure. The order, the level and
// 'hold' are left as they are.
void LeitnerProcessor::randomize()
{
    Random r;
    auto set = [] (RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (figure, (float) r.nextInt (leitner::fgDrawn));   // (any but your own)
    set (seconds, 1.0f + 14.0f * r.nextFloat() * r.nextFloat());
    set (rest, r.nextBool() ? 0.0f : 0.4f * r.nextFloat());
    const int pts[] = { 4, 6, 8, 12, 16, 24, leitner::kMaxPoints };
    set (points, (float) pts[r.nextInt (7)]);
    set (legato, r.nextFloat());
    set (size, 0.6f + 0.4f * r.nextFloat());
    set (height, 0.8f * r.nextFloat() - 0.4f);
    set (turn, 360.0f * r.nextFloat() - 180.0f);
    set (apart, 60.0f * r.nextFloat());
    set (counter, (float) r.nextInt (3) * 0.5f);
    set (swell, 0.6f * r.nextFloat());
    set (lift, 0.6f * r.nextFloat());
    set (chance, r.nextBool() ? 0.0f : r.nextFloat());
    set (walk, r.nextBool() ? 0.0f : 0.8f * r.nextFloat());
    set (scatter, r.nextBool() ? 0.0f : 0.6f * r.nextFloat());
    set (sieve, (float) r.nextInt (leitner::kNumSieves));
    set (room, 0.4f * r.nextFloat());
    set (roomDecay, 1.0f + 7.0f * r.nextFloat());
}

// Stereo in; out, Ambisonics. As an Audio Unit and in the desktop app 3rd order (16 channels) is
// published; everywhere else up to 7th (64), so a track with that many has room for it. The order
// setting says how many are used.
AudioProcessor::BusesProperties LeitnerProcessor::buses()
{
    const auto t = PluginHostType::getPluginLoadedAs();
    const bool third = t == AudioProcessor::wrapperType_AudioUnit || t == AudioProcessor::wrapperType_Standalone;
    return BusesProperties()
        .withInput  ("Input",  AudioChannelSet::stereo(), true)
        .withOutput ("Output", AudioChannelSet::ambisonic (third ? 3 : 7), true);
}

bool LeitnerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    // any order from 3rd to 7th (and the lower ones, for a host that insists: the highest that fits comes out),
    // the same counts of plain channels, or stereo (a plain left/right fold of the scene, for checking)
    bool ok = out == AudioChannelSet::stereo();
    for (int o = 1; o <= fp::ambi::kMaxOrder; ++o)
    {
        ok = ok || out == AudioChannelSet::ambisonic (o);
        ok = ok || (out == AudioChannelSet::discreteChannels (fp::ambi::chFor (o)) && wrapperType != wrapperType_AudioUnit);
    }
    if (! ok) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

leitner::Params LeitnerProcessor::readSpace() const
{
    leitner::Params p;
    p.order = 3 + order->getIndex();
    p.figure = figure->getIndex();
    p.seconds = seconds->get(); p.rest = rest->get(); p.size = size->get(); p.height = height->get();
    p.turn = turn->get(); p.apart = apart->get(); p.counter = counter->get();
    p.points = points->get(); p.legato = legato->get(); p.swell = swell->get(); p.lift = lift->get();
    p.chance = chance->get(); p.walk = walk->get(); p.scatter = scatter->get(); p.sieve = sieve->getIndex();
    p.room = room->get(); p.roomDecay = roomDecay->get(); p.level = level->get();
    p.hold = hold->get();
    p.drawn = &drawn;
    return p;
}

void LeitnerProcessor::setDrawing (const leitner::Drawn& d)
{
    drawn = d;
    drawn.prepare();
    {
        const SpinLock::ScopedLockType l (drawnLock);
        drawnShared = drawn;
    }
    ++drawnVersion;
}

void LeitnerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    space.prepare (sampleRate);
    {
        const SpinLock::ScopedLockType l (drawnLock);
        drawnAudio = drawnShared; drawnSeen = drawnVersion.load();
    }
    stereo.setSize (2, jmax (1, samplesPerBlock), false, true, true);
}

void LeitnerProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = buffer.getNumChannels();
    if (n == 0 || numOut == 0) return;
    if (stereo.getNumSamples() < n) stereo.setSize (2, n, false, true, true);

    // the input, untouched: the left channel is line 1, the right line 2 (a mono input: both)
    stereo.copyFrom (0, 0, buffer, 0, 0, n);
    stereo.copyFrom (1, 0, buffer, numIn > 1 && buffer.getNumChannels() > 1 ? 1 : 0, 0, n);

    auto p = readSpace();
    // your drawing, if it has changed (and the window isn't handing it over this very moment)
    if (const int v = drawnVersion.load(); v != drawnSeen)
        if (const SpinLock::ScopedTryLockType l (drawnLock); l.isLocked()) { drawnAudio = drawnShared; drawnSeen = v; }
    p.drawn = &drawnAudio;

    float* outs[fp::ambi::kCh];
    if (numOut >= 4)
    {
        const int nch = jmin (numOut, fp::ambi::kCh);
        for (int c = 0; c < nch; ++c) outs[c] = buffer.getWritePointer (c);
        for (int c = nch; c < numOut; ++c) buffer.clear (c, 0, n);
        space.process (stereo.getReadPointer (0), stereo.getReadPointer (1), outs, nch, n, p);
        const int o = jmin (p.order, fp::ambi::orderFor (nch));
        for (int c = fp::ambi::chFor (o); c < nch; ++c) buffer.clear (c, 0, n);
        orderNow.store (o); channelsNow.store (fp::ambi::chFor (o));
    }
    else
    {
        // a stereo (or mono) output: encode at 1st order and fold it to left and right, W + Y and W - Y,
        // so there's something to hear without a decoder
        static thread_local std::vector<float> scratch[4];
        for (auto& s : scratch) if ((int) s.size() < n) s.resize ((size_t) n);
        for (int c = 0; c < 4; ++c) outs[c] = scratch[c].data();
        p.order = 1;
        space.process (stereo.getReadPointer (0), stereo.getReadPointer (1), outs, 4, n, p);
        float* L = buffer.getWritePointer (0);
        float* R = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float w = outs[0][i], y = outs[1][i];
            if (R != nullptr) { L[i] = 0.7071f * (w + y); R[i] = 0.7071f * (w - y); } else L[i] = w;
        }
        orderNow.store (0); channelsNow.store (numOut);
    }
    outLevel.store (buffer.getMagnitude (0, 0, n));
}

AudioProcessorEditor* LeitnerProcessor::createEditor() { return new LeitnerEditor (*this); }

// The state: every parameter as an attribute (its 0..1 value), the program, how the room is turned, and
// your drawing. Sessions and presets from Leitner 0.2 (which kept these same attributes on Cloudfin's
// state) load too. Before version 4 there were 14 figures, not 15, so the figure's 0..1 is moved over.
void LeitnerProcessor::getStateInformation (MemoryBlock& dest)
{
    XmlElement x ("LEITNER");
    x.setAttribute ("version", 4);
    {
        auto* d = x.createNewChildElement ("DRAWING");
        d->setAttribute ("closed", drawn.closed); d->setAttribute ("curved", drawn.curved); d->setAttribute ("atPoints", drawn.atPoints);
        String pts;
        for (int i = 0; i < drawn.count; ++i)
            pts << String (drawn.pt[i].x, 4) << " " << String (drawn.pt[i].y, 4) << " " << String (drawn.pt[i].z, 4) << (i + 1 < drawn.count ? ";" : "");
        d->setAttribute ("points", pts);
    }
    for (auto* prm : spaceParams) x.setAttribute (prm->getParameterID(), prm->getValue());
    x.setAttribute ("ltProgram", program.load());
    x.setAttribute ("viewYaw", viewYaw.load());
    x.setAttribute ("viewPitch", viewPitch.load());
    copyXmlToBinary (x, dest);
}

void LeitnerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto x = getXmlFromBinary (data, sizeInBytes);
    if (x == nullptr) return;
    const bool before4 = x->getIntAttribute ("version", 2) < 4;
    for (auto* prm : spaceParams)
        if (x->hasAttribute (prm->getParameterID()))
        {
            float v = (float) x->getDoubleAttribute (prm->getParameterID());
            if (before4 && prm == figure) v = figure->convertTo0to1 ((float) roundToInt (v * (float) (leitner::numFigures - 2)));
            prm->setValueNotifyingHost (v);
        }
    if (auto* d = x->getChildByName ("DRAWING"))
    {
        leitner::Drawn nd;
        nd.closed = d->getBoolAttribute ("closed", true); nd.curved = d->getBoolAttribute ("curved", true); nd.atPoints = d->getBoolAttribute ("atPoints", false);
        for (auto& t : StringArray::fromTokens (d->getStringAttribute ("points"), ";", ""))
        {
            const auto c = StringArray::fromTokens (t, " ", "");
            if (c.size() == 3 && nd.count < leitner::kMaxDrawn)
                nd.pt[nd.count++] = { jlimit (-1.0f, 1.0f, c[0].getFloatValue()), jlimit (-1.0f, 1.0f, c[1].getFloatValue()), jlimit (-1.0f, 1.0f, c[2].getFloatValue()) };
        }
        if (nd.count >= 2) setDrawing (nd);
    }
    program.store (jlimit (0, getNumPrograms() - 1, x->getIntAttribute ("ltProgram", 0)));
    if (x->hasAttribute ("viewYaw")) { viewYaw.store ((float) x->getDoubleAttribute ("viewYaw")); viewPitch.store ((float) x->getDoubleAttribute ("viewPitch")); }
}

File LeitnerProcessor::userPresetFolder() const
{
   #if JUCE_MAC
    const auto base = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    const auto base = File::getSpecialLocation (File::userApplicationDataDirectory);   // AppData\Roaming on Windows, ~/.config on Linux
   #endif
    return base.getChildFile ("FreakiePie").getChildFile ("Leitner").getChildFile ("Presets");
}

Array<File> LeitnerProcessor::userPresets() const
{
    auto files = userPresetFolder().findChildFiles (File::findFiles, false, "*" + presetExtension());
    files.sort();
    return files;
}

bool LeitnerProcessor::saveUserPreset (const File& file)
{
    MemoryBlock mb;
    getStateInformation (mb);
    const auto target = file.withFileExtension (presetExtension());
    return target.getParentDirectory().createDirectory().wasOk() && target.replaceWithData (mb.getData(), mb.getSize());
}

bool LeitnerProcessor::loadUserPreset (const File& file)
{
    MemoryBlock mb;
    if (! file.existsAsFile() || ! file.loadFileAsData (mb) || mb.getSize() == 0) return false;
    setStateInformation (mb.getData(), (int) mb.getSize());
    return true;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LeitnerProcessor(); }
