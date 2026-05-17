#include "SynthVoice.h"

static constexpr float kGlobalLevel = 0.06f;

namespace
{
float shapeVelocityForTone(float velocity, float midpoint, float sharpness)
{
    velocity  = juce::jlimit(0.0f, 1.0f, velocity);
    midpoint  = juce::jlimit(0.1f, 0.9f, midpoint);
    sharpness = juce::jmax(1.0f, sharpness);

    const float logistic = 1.0f / (1.0f + std::exp(-sharpness * (velocity - midpoint)));
    const float minValue = 1.0f / (1.0f + std::exp( sharpness * midpoint));
    const float maxValue = 1.0f / (1.0f + std::exp(-sharpness * (1.0f - midpoint)));
    return juce::jlimit(0.0f, 1.0f, (logistic - minValue) / (maxValue - minValue));
}

float renderCarrierSample(double angle, float modulation,
                          float h2, float h3, float h5)
{
    const float phase = (float) angle + modulation;
    const float harmonicSum = std::sin(phase)
                              + h2 * std::sin(2.0f * phase)
                              + h3 * std::sin(3.0f * phase)
                              + h5 * std::sin(5.0f * phase);
    const float norm = 1.0f + std::abs(h2) + std::abs(h3) + std::abs(h5);
    return harmonicSum / norm;
}

float applyReedBuzz(float sample, float driveAmount, float mix)
{
    mix = juce::jlimit(0.0f, 1.0f, mix);
    if (mix <= 0.0f || driveAmount <= 0.0f)
        return sample;

    const float drive = 1.0f + driveAmount * 2.0f;
    const float clipped = std::tanh(sample * drive) / std::tanh(drive);
    const float oddEmphasis = 0.82f * clipped + 0.18f * clipped * clipped * clipped;
    return juce::jmap(mix, sample, oddEmphasis);
}

float applyAsymmetricReedShape(float sample, float asymmetry, float bark)
{
    asymmetry = juce::jlimit(0.0f, 1.0f, asymmetry);
    bark = juce::jlimit(0.0f, 1.0f, bark);

    if (asymmetry <= 0.0001f && bark <= 0.0001f)
        return sample;

    const float posDrive = 1.0f + asymmetry * 2.4f + bark * 1.1f;
    const float negDrive = 1.0f + asymmetry * 0.9f;
    const float shaped = sample >= 0.0f
                             ? std::tanh(sample * posDrive) / std::tanh(posDrive)
                             : std::tanh(sample * negDrive) / std::tanh(negDrive);
    const float evenTilt = asymmetry * 0.12f * (sample * sample - 0.35f * std::abs(sample));
    return juce::jlimit(-1.0f, 1.0f, shaped + evenTilt);
}

float renderAfterglowSample(double angle, float modulation, float harmonic)
{
    const float harmonicRatio = juce::jlimit(2.0f, 5.0f, harmonic);
    const float phase = (float) angle + modulation * 0.15f;
    const float bright = (float) std::sin(phase * harmonicRatio);
    return ((float) std::sin(phase) + 0.40f * bright) / 1.40f;
}

float renderResonanceSample(double angle, float modulation, float harmonic)
{
    const float harmonicRatio = juce::jlimit(2.0f, 4.0f, harmonic);
    const float phase = (float) angle + modulation * 0.08f;
    const float resonant = (float) std::sin(phase * harmonicRatio);
    const float upper = (float) std::sin(phase * (harmonicRatio + 0.8f));
    return (resonant + 0.22f * upper) / 1.22f;
}
}

bool ElectricPianoVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<ElectricPianoSound*>(sound) != nullptr;
}

void ElectricPianoVoice::updateOscillatorDeltas() noexcept
{
    if (baseFrequencyHz <= 0.0 || getSampleRate() <= 0.0)
        return;

    const double freq = baseFrequencyHz * (double) pitchBendRatio;
    const double baseDelta = freq / getSampleRate() * juce::MathConstants<double>::twoPi;
    const double detuneRatio = std::pow(2.0, (double) currentDetuneCents / 1200.0);

    carrierDeltaL  = baseDelta / detuneRatio;
    carrierDeltaR  = baseDelta * detuneRatio;
    modulatorDelta = baseDelta * (double) currentModRatio;
}

void ElectricPianoVoice::startNote(int midiNoteNumber, float velocity,
                                    juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    carrierAngleL  = 0.0;
    carrierAngleR  = 0.0;
    modulatorAngle = 0.0;
    pitchBendRatio = 1.0f;
    level = velocity;
    afterglowLevel = 0.0f;
    afterglowDecay = 0.0f;
    sympatheticResonance = 0.0f;

    // Read all preset + user params from the shared atomic struct.
    // Falls back to Wurlitzer 200A defaults when params is null.
    const float modRatio     = params ? params->modRatio.load()       : 2.0f;
    const float detuneCents  = params ? params->detuneCents.load()    : 3.0f;
    const float cDecay       = params ? params->carrierDecay.load()   : 0.35f;
    const float cSustain     = params ? params->carrierSustain.load() : 0.40f;
    const float mDecay       = params ? params->modDecay.load()       : 0.08f;
    const float mSustain     = params ? params->modSustain.load()     : 0.0f;
    const float mRelease     = params ? params->modRelease.load()     : 0.04f;
    const float fmLow        = params ? params->fmDepthLow.load()     : 0.10f;
    const float fmRange      = params ? params->fmDepthRange.load()   : 5.5f;
    const float ksCoeff      = params ? params->keyScaleCoeff.load()  : 0.004f;
    const float noiseAmt     = params ? params->noiseBurstAmt.load()  : 0.25f;
    const float depthScale   = params ? params->fmDepthScale.load()   : 1.0f;
    const float attackSec    = params ? params->attackTime.load()     : 0.001f;
    const float releaseSec   = params ? params->releaseTime.load()    : 0.4f;
    const float velPow       = params ? params->velPow.load()         : 2.0f;
    const float velToneMid   = params ? params->velToneMidpoint.load(): 0.58f;
    const float velToneSharp = params ? params->velToneSharpness.load(): 9.0f;
    const float afterglowAmt = params ? params->afterglowAmount.load(): 0.0f;
    const float afterglowSec = params ? params->afterglowDecay.load() : 0.18f;
    const float afterglowHarmonic = params ? params->afterglowHarmonic.load() : 4.0f;
    const float resonanceAmount = params ? params->resonanceAmount.load() : 0.0f;
    const float resonanceHarmonic = params ? params->resonanceHarmonic.load() : 3.0f;

    const float velLevel = std::pow(velocity, velPow);
    const float velTone  = shapeVelocityForTone(velocity, velToneMid, velToneSharp);
    level = velLevel;
    buzzExcitation = velTone;
    currentAfterglowAmount = afterglowAmt;
    currentAfterglowHarmonic = afterglowHarmonic;
    currentResonanceAmount = resonanceAmount;
    currentResonanceHarmonic = resonanceHarmonic;
    afterglowDecay = (float) std::pow(0.001, 1.0 / (getSampleRate() * juce::jmax(0.01f, afterglowSec)));

    const float highNoteFactor = juce::jlimit(0.0f, 1.0f, ((float) midiNoteNumber - 72.0f) / 24.0f);
    const float transientDecay = juce::jmap(highNoteFactor, mDecay, mDecay * 0.6f);
    const float transientBoost = juce::jmap(highNoteFactor, 1.0f, 1.18f);

    // Velocity → FM depth with key scaling and user multiplier.
    float keyScale    = juce::jlimit(0.6f, 1.4f, 1.0f - ksCoeff * (float)(midiNoteNumber - 60));
    modulationDepth   = (fmLow + velTone * fmRange) * depthScale * keyScale * transientBoost;

    baseFrequencyHz    = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    currentModRatio    = modRatio;
    currentDetuneCents = detuneCents;
    pitchWheelMoved(currentPitchWheelPosition);

    carrierEnv.setSampleRate(getSampleRate());
    modulatorEnv.setSampleRate(getSampleRate());
    carrierEnv.setParameters  ({attackSec, cDecay, cSustain, releaseSec});
    modulatorEnv.setParameters({0.0005f, transientDecay, mSustain, mRelease});
    carrierEnv.noteOn();
    modulatorEnv.noteOn();

    // Noise burst: hammer-on-reed/tine physical impact transient (~20ms decay).
    noiseBurstDecay = (float)std::pow(0.001, 1.0 / (getSampleRate() * 0.020));
    noiseBurstLevel = velTone * noiseAmt * juce::jmap(highNoteFactor, 1.0f, 1.25f);
}

void ElectricPianoVoice::pitchWheelMoved(int newPitchWheelValue)
{
    const float bendRangeSemitones = params ? params->pitchBendRange.load() : 2.0f;
    const double normalized = juce::jlimit(-1.0, 1.0,
                                           ((double) newPitchWheelValue - 8192.0) / 8192.0);
    const double semitones = normalized * (double) bendRangeSemitones;
    pitchBendRatio = (float) std::pow(2.0, semitones / 12.0);
    updateOscillatorDeltas();
}

void ElectricPianoVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        carrierEnv.noteOff();
        modulatorEnv.noteOff();
        afterglowLevel = juce::jmax(afterglowLevel, level * currentAfterglowAmount);
    }
    else
    {
        carrierEnv.reset();
        modulatorEnv.reset();
        clearCurrentNote();
        carrierDeltaL = carrierDeltaR = 0.0;
        modulatorDelta = 0.0;
        baseFrequencyHz = 0.0;
        pitchBendRatio = 1.0f;
        afterglowLevel = 0.0f;
        sympatheticResonance = 0.0f;
    }
    noiseBurstLevel = 0.0f;
    buzzExcitation = 0.0f;
}

void ElectricPianoVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                          int startSample, int numSamples)
{
    if (!carrierEnv.isActive())
        return;

    const bool  hasStereo = outputBuffer.getNumChannels() >= 2;

    // Load per-block params (atomic reads once per callback, not per sample).
    const float h2    = params ? params->carrierH2.load()    : 0.08f;
    const float h3    = params ? params->carrierH3.load()    : 0.10f;
    const float h5    = params ? params->carrierH5.load()    : 0.04f;
    const float buzzDrive = params ? params->satDrive.load()   : 1.0f;
    const float reedAsymmetry = params ? params->reedAsymmetry.load() : 0.0f;
    const float barkBloom = params ? params->barkBloom.load() : 0.0f;
    const float drive     = params ? params->driveAmount.load(): 1.0f;

    while (--numSamples >= 0)
    {
        float carEnv = carrierEnv.getNextSample();
        float modEnv = modulatorEnv.getNextSample();

        float mod  = modEnv * modulationDepth * (float)std::sin(modulatorAngle);

        const float barkAmount = juce::jlimit(0.0f, 1.0f,
                                              barkBloom * buzzExcitation * (0.25f + modEnv * 1.25f));
        const float dynamicH2 = h2 + reedAsymmetry * barkAmount * 0.05f;
        const float dynamicH3 = h3 + barkAmount * 0.16f;
        const float dynamicH5 = h5 + barkAmount * 0.09f;

        float carrierL = renderCarrierSample(carrierAngleL, mod, dynamicH2, dynamicH3, dynamicH5);
        float carrierR = renderCarrierSample(carrierAngleR, mod, dynamicH2, dynamicH3, dynamicH5);

        carrierL = applyAsymmetricReedShape(carrierL, reedAsymmetry, barkAmount);
        carrierR = applyAsymmetricReedShape(carrierR, reedAsymmetry, barkAmount);

        const float buzzMix = juce::jlimit(0.0f, 1.0f,
                                           buzzExcitation * (0.35f + modEnv * 0.9f));
        carrierL = applyReedBuzz(carrierL, buzzDrive, buzzMix);
        carrierR = applyReedBuzz(carrierR, buzzDrive, buzzMix);

        const float resonanceTarget = isSustainPedalDown() ? currentResonanceAmount : 0.0f;
        sympatheticResonance += (resonanceTarget - sympatheticResonance) * 0.0025f;

        if (sympatheticResonance > 0.00005f)
        {
            const float releaseBlend = isKeyDown()
                                           ? juce::jlimit(0.0f, 0.25f, (1.0f - carEnv) * 0.20f)
                                           : juce::jlimit(0.12f, 0.40f, 0.12f + (1.0f - carEnv) * 0.28f);
            const float resL = renderResonanceSample(carrierAngleL, mod, currentResonanceHarmonic);
            const float resR = renderResonanceSample(carrierAngleR, mod, currentResonanceHarmonic);
            carrierL += resL * sympatheticResonance * releaseBlend;
            carrierR += resR * sympatheticResonance * releaseBlend;
        }

        if (afterglowLevel > 0.00005f)
        {
            const float afterglowMix = juce::jlimit(0.0f, 0.30f, afterglowLevel);
            const float bellL = renderAfterglowSample(carrierAngleL, mod, currentAfterglowHarmonic);
            const float bellR = renderAfterglowSample(carrierAngleR, mod, currentAfterglowHarmonic);
            carrierL = juce::jmap(afterglowMix, carrierL, bellL);
            carrierR = juce::jmap(afterglowMix, carrierR, bellR);
            afterglowLevel *= afterglowDecay;
        }

        // Soft saturation: f(x) = x / (1 + |x|*drive) * (1+drive)
        // Clamps peaks while boosting mid-signal → warm, organic texture.
        float satL = (drive > 0.001f) ? carrierL / (1.0f + std::abs(carrierL) * drive) * (1.0f + drive)
                                      : carrierL;
        float satR = (drive > 0.001f) ? carrierR / (1.0f + std::abs(carrierR) * drive) * (1.0f + drive)
                                      : carrierR;

        float sampleL = carEnv * level * satL * kGlobalLevel;
        float sampleR = carEnv * level * satR * kGlobalLevel;

        if (noiseBurstLevel > 0.0002f)
        {
            float noise = (rng.nextFloat() * 2.0f - 1.0f) * noiseBurstLevel * kGlobalLevel * level;
            sampleL += noise;
            sampleR += noise;
            noiseBurstLevel *= noiseBurstDecay;
        }

        outputBuffer.addSample(0, startSample, sampleL);
        if (hasStereo)
            outputBuffer.addSample(1, startSample, sampleR);

        carrierAngleL  += carrierDeltaL;
        carrierAngleR  += carrierDeltaR;
        modulatorAngle += modulatorDelta;

        // Wrap phase into [0, 2π). Without this the double angle grows
        // unboundedly; once cast to float in renderCarrierSample the phase
        // resolution degrades on long-held notes (audible detune / FM noise).
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        while (carrierAngleL  >= twoPi) carrierAngleL  -= twoPi;
        while (carrierAngleR  >= twoPi) carrierAngleR  -= twoPi;
        while (modulatorAngle >= twoPi) modulatorAngle -= twoPi;

        ++startSample;

        if (!carrierEnv.isActive())
        {
            clearCurrentNote();
            carrierDeltaL = carrierDeltaR = 0.0;
            modulatorDelta = 0.0;
            baseFrequencyHz = 0.0;
            pitchBendRatio = 1.0f;
            afterglowLevel = 0.0f;
            sympatheticResonance = 0.0f;
            break;
        }
    }
}
