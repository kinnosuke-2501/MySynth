#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "VintageLookAndFeel.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,   // public に変更: キャストを確実に通す
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // MidiInputCallback (public override)
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override;

private:
    enum class Preset { Wurlitzer, Rhodes };

    void timerCallback() override;
    void openAllMidiInputs();
    void updateMidiStatus();
    void applyPreset(Preset p);

    juce::Synthesiser          synthesiser;
    juce::MidiMessageCollector midiCollector;

    // Shared synth voice parameters (UI thread writes, audio thread reads)
    SynthParams synthParams;

    // 2× oversampling of the voice generation only. FM + 3rd/5th harmonic
    // injection + asymmetric tanh + reed buzz + saturation all alias badly at
    // base rate; the synth renders at 2×fs and a half-band polyphase IIR
    // (low latency) decimation filter removes the folded partials. FX run at
    // base rate post-decimation (they are not aliasing sources).
    static constexpr int kOversampleFactorLog2 = 1;   // 1 → 2×
    juce::dsp::Oversampling<float> oversampling
        { 2, kOversampleFactorLog2,
          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    // Effects chain: Chorus → Tremolo → Reverb
    juce::dsp::Chorus<float>  fxChorus;
    juce::dsp::Reverb         fxReverb;

    // Tremolo LFO state (Wurlitzer 145 amp character)
    double tremoloPhase      = 0.0;
    double tremoloPhaseDelta = 0.0;
    double currentSampleRate = 44100.0;  // set in prepareToPlay, read in getNextAudioBlock

    // Atomics for audio-thread-readable effect parameters
    std::atomic<float> tremoloRateHz   { 5.0f };
    std::atomic<float> tremoloDepthAmt { 0.25f };
    std::atomic<float> reverbWetAmt    { 0.35f };
    std::atomic<bool>  reverbDirty     { false };
    std::atomic<float> chorusMixAmt    { 0.08f };  // updated per preset
    std::atomic<int>   pendingPedalNoise { 0 };    // +1 pedal down, -1 pedal up
    float              lastChorusMix   { -1.0f };  // tracks last applied value

    float pedalNoiseEnv        { 0.0f };
    float pedalNoiseDecay      { 0.0f };
    float pedalNoiseFilter     { 0.0f };
    float pedalNoiseDirection  { 0.0f };
    double pedalNoiseThumpPhase{ 0.0 };
    double pedalNoiseThumpDelta{ 0.0 };
    juce::Random pedalNoiseRng;

    // DC blocker (one-pole HPF, ~20 Hz). Asymmetric reed shaping injects a
    // small offset; strip it before the chorus/reverb so DC can't accumulate
    // in the tail. State is touched only on the audio thread.
    float dcBlockR  = 0.9975f;          // recomputed from sample rate
    float dcBlockX1[2] { 0.0f, 0.0f };
    float dcBlockY1[2] { 0.0f, 0.0f };

    // MidiInput::openDevice で直接開いたデバイスを保持
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;

    juce::Atomic<int> activeNoteCount  { 0 };
    juce::Atomic<int> sustainPedalDown { 0 };
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboardComponent { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    juce::Label       titleLabel;
    juce::Label       statusLabel;
    juce::Label       hintLabel;
    juce::TextButton  settingsButton   { "Settings..." };
    juce::TextButton  panicButton      { "Panic" };
    juce::TextButton  wurlitzerButton  { "WURLI" };
    juce::TextButton  rhodesButton     { "ROADS" };
    Preset            currentPreset    { Preset::Wurlitzer };

    // Knob controls
    juce::Slider sliderTremoloRate, sliderTremoloDepth;
    juce::Slider sliderReverbWet,   sliderFMDepth;
    juce::Slider sliderAttack,      sliderRelease,   sliderDrive;
    juce::Slider sliderChorus;

    juce::Label  labelTremoloRate,  labelTremoloDepth;
    juce::Label  labelReverbWet,    labelFMDepth;
    juce::Label  labelAttack,       labelRelease,    labelDrive;
    juce::Label  labelChorus;

    // VU meter level (RMS, written from audio thread)
    std::atomic<float> vuLevel { 0.0f };

    VintageLookAndFeel vintageTheme;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
