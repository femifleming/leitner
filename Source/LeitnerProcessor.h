#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Spatial.h"

//==============================================================================
// Leitner: a spatialization plug-in after Bernhard Leitner and Iannis Xenakis. The two channels you
// give it become two lines of sound that travel through the room round the listener (see
// dsp/Spatial.h), encoded to Ambisonics, ACN/SN3D, 3rd to 7th order. It changes nothing in the sound
// itself: it only says where it is. It is an encoder: route its channels to a decoder of the same order.
class LeitnerProcessor : public juce::AudioProcessor
{
public:
    LeitnerProcessor();

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Leitner"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    // the programs: spaces after Leitner, and after Xenakis
    int getNumPrograms() override;
    int getCurrentProgram() override { return program.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    juce::String getProgramGroup (int);

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // your own presets (a folder of .leitner files)
    juce::File userPresetFolder() const;
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    static juce::String presetExtension() { return ".leitner"; }
    void randomize();   // a random space (message thread)

    // your own figure: the path you draw (message thread). setDrawing measures it and hands it to the audio.
    const leitner::Drawn& drawing() const { return drawn; }
    void setDrawing (const leitner::Drawn&);

    juce::AudioParameterChoice* order = nullptr;
    juce::AudioParameterChoice* figure = nullptr;
    juce::AudioParameterChoice* sieve = nullptr;
    juce::AudioParameterFloat *seconds = nullptr, *rest = nullptr, *size = nullptr, *height = nullptr, *turn = nullptr, *apart = nullptr,
                              *counter = nullptr, *legato = nullptr, *swell = nullptr, *lift = nullptr, *chance = nullptr, *walk = nullptr, *scatter = nullptr,
                              *room = nullptr, *roomDecay = nullptr, *level = nullptr;
    juce::AudioParameterInt* points = nullptr;
    juce::AudioParameterBool* hold = nullptr;
    std::vector<juce::RangedAudioParameter*> spaceParams;   // all of them, in order

    leitner::Params readSpace() const;
    leitner::Space space;
    std::atomic<int> orderNow { 3 }, channelsNow { 16 };   // what's actually coming out (a track with too few channels gets a lower order)
    std::atomic<float> outLevel { 0.0f };
    std::atomic<int> program { 0 };
    std::atomic<float> viewYaw { 0.0f }, viewPitch { 0.0f };   // how the drawing of the room is turned (kept while the window is closed)

private:
    static BusesProperties buses();
    leitner::Drawn drawn = leitner::Drawn::saddle();     // the message thread's
    leitner::Drawn drawnShared = drawn, drawnAudio = drawn;   // handed over under the lock; the audio's own
    juce::SpinLock drawnLock;
    std::atomic<int> drawnVersion { 0 };
    int drawnSeen = 0;
    juce::AudioBuffer<float> stereo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LeitnerProcessor)
};
