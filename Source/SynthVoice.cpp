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

    const float velLevel = std::pow(velocity, velPow);
    const float velTone  = shapeVelocityForTone(velocity, velToneMid, velToneSharp);
    level = velLevel;
    buzzExcitation = velTone;

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
    const float drive     = params ? params->driveAmount.load(): 1.0f;

    while (--numSamples >= 0)
    {
        float carEnv = carrierEnv.getNextSample();
        float modEnv = modulatorEnv.getNextSample();

        float mod  = modEnv * modulationDepth * (float)std::sin(modulatorAngle);

        float carrierL = renderCarrierSample(carrierAngleL, mod, h2, h3, h5);
        float carrierR = renderCarrierSample(carrierAngleR, mod, h2, h3, h5);

        const float buzzMix = juce::jlimit(0.0f, 1.0f,
                                           buzzExcitation * (0.35f + modEnv * 0.9f));
        carrierL = applyReedBuzz(carrierL, buzzDrive, buzzMix);
        carrierR = applyReedBuzz(carrierR, buzzDrive, buzzMix);

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
        ++startSample;

        if (!carrierEnv.isActive())
        {
            clearCurrentNote();
            carrierDeltaL = carrierDeltaR = 0.0;
            modulatorDelta = 0.0;
            baseFrequencyHz = 0.0;
            pitchBendRatio = 1.0f;
            break;
        }
    }
}
