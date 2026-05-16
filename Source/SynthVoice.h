#pragma once
#include <JuceHeader.h>

// Shared parameters between UI thread (writer) and audio thread (reader).
// All fields are atomic so no lock is needed across thread boundary.
//
// Split into two groups:
//   Preset params  — set by preset buttons, define the instrument character.
//   User params    — knob-controlled fine-tuning on top of the preset.
struct SynthParams
{
    // ---- Preset params (instrument character) ----
    std::atomic<float> modRatio       { 2.0f };   // FM modulator freq ratio (1:1 Rhodes / 2:1 Wurlitzer)
    std::atomic<float> detuneCents    { 3.0f };   // L/R stereo spread in cents
    std::atomic<float> carrierDecay   { 0.5f };   // carrier ADSR decay (s)
    std::atomic<float> carrierSustain { 0.45f };  // carrier ADSR sustain level (0–1)
    std::atomic<float> modDecay       { 0.12f };  // modulator ADSR decay (s)
    std::atomic<float> modSustain     { 0.0f };   // modulator ADSR sustain (0 = full decay)
    std::atomic<float> modRelease     { 0.06f };  // modulator ADSR release (s)
    std::atomic<float> fmDepthLow     { 0.8f };   // FM depth at velocity=0
    std::atomic<float> fmDepthRange   { 5.2f };   // FM depth velocity range (× velocity)
    std::atomic<float> keyScaleCoeff  { 0.003f }; // brightness scaling per semitone from C4
    std::atomic<float> noiseBurstAmt  { 0.20f };  // hammer impact transient level (vel-scaled)

    // Waveform shaping — applied per-block in renderNextBlock
    std::atomic<float> carrierH2 { 0.08f };  // 2nd harmonic added to carrier (0=pure sine)
    std::atomic<float> carrierH3 { 0.10f };  // odd harmonic content for reed/tine bite
    std::atomic<float> carrierH5 { 0.04f };  // upper odd harmonic sheen
    std::atomic<float> satDrive  { 1.0f };   // soft saturation: 0=off, 1=mild, 3=heavy
    std::atomic<float> reedAsymmetry { 0.0f }; // Wurlitzer-style asymmetric reed shaping
    std::atomic<float> barkBloom     { 0.0f }; // transient harmonic bloom on note attack
    std::atomic<float> velPow    { 2.0f };   // velocity power curve for output level
    std::atomic<float> velToneMidpoint  { 0.58f }; // midpoint for tone-brightening S-curve
    std::atomic<float> velToneSharpness { 9.0f };  // steepness for tone-brightening S-curve
    std::atomic<float> pitchBendRange   { 2.0f };  // pitch bend range in semitones
    std::atomic<float> afterglowAmount  { 0.0f };  // note-off bell tail level (Rhodes only)
    std::atomic<float> afterglowDecay   { 0.18f }; // note-off bell tail decay time in seconds
    std::atomic<float> afterglowHarmonic{ 4.0f };  // bell tail harmonic multiplier
    std::atomic<float> resonanceAmount  { 0.0f };  // sustain-pedal sympathetic resonance level
    std::atomic<float> resonanceHarmonic{ 3.0f };  // sustain-pedal resonance harmonic multiplier

    // ---- User params (knob-controlled) ----
    std::atomic<float> fmDepthScale { 1.0f };   // 0.5–2.0: multiplier on velocity→FM depth
    std::atomic<float> attackTime   { 0.001f };  // carrier attack  (seconds, 1ms–200ms)
    std::atomic<float> releaseTime  { 0.4f };    // carrier release (seconds, 50ms–2000ms)
    std::atomic<float> driveAmount  { 1.0f };    // user-facing Drive knob (mirrors satDrive)
};

struct ElectricPianoSound : public juce::SynthesiserSound
{
    ElectricPianoSound() {}
    bool appliesToNote(int) override    { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Wurlitzer 200A electric piano emulation via 2-operator FM synthesis.
//
// What makes Wurlitzer sound different from Rhodes:
//   - Modulator runs at 2× carrier freq (ratio 2:1) → nasal, reedy harmonic profile
//   - Very wide velocity sensitivity (dark/mellow → bright/aggressive)
//   - Short modulator decay (~120ms) → crisp percussive "bark", then settles
//   - Noise burst on key-on → simulates physical hammer-on-reed impact ("clack")
//   - Key scaling → low register is brighter, high register mellower (reed physics)
//   - Tight L/R detune (±3 cents) — Wurlitzer is more focused than Rhodes
class ElectricPianoVoice : public juce::SynthesiserVoice
{
public:
    void setParams(SynthParams* p) noexcept { params = p; }

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound*, int currentPitchWheelPosition) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override;

private:
    void updateOscillatorDeltas() noexcept;

    SynthParams* params = nullptr;

    juce::ADSR carrierEnv;
    juce::ADSR modulatorEnv;

    // Carrier: L and R slightly detuned (±3 cents)
    double carrierAngleL   = 0.0;
    double carrierAngleR   = 0.0;
    double carrierDeltaL   = 0.0;
    double carrierDeltaR   = 0.0;

    double modulatorAngle  = 0.0;
    double modulatorDelta  = 0.0;   // 2× carrier freq
    double baseFrequencyHz = 0.0;
    float  currentModRatio = 2.0f;
    float  currentDetuneCents = 3.0f;
    float  pitchBendRatio = 1.0f;

    float modulationDepth  = 0.0f;
    float level            = 0.0f;
    float buzzExcitation   = 0.0f;
    float afterglowLevel   = 0.0f;
    float afterglowDecay   = 0.0f;
    float currentAfterglowAmount = 0.0f;
    float currentAfterglowHarmonic = 4.0f;
    float sympatheticResonance = 0.0f;
    float currentResonanceAmount = 0.0f;
    float currentResonanceHarmonic = 3.0f;

    // Noise burst: physical hammer-on-reed "clack" transient
    float noiseBurstLevel  = 0.0f;
    float noiseBurstDecay  = 0.0f;  // multiplicative per-sample decay
    juce::Random rng;
};
