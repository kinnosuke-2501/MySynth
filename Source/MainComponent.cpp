#include "MainComponent.h"

static constexpr int kNumVoices = 16;

// ---- Preset data --------------------------------------------------------
// Each preset defines the full FM voice character plus default knob positions.
struct PresetData
{
    // Voice character (FM synthesis + waveform shaping)
    float modRatio, detuneCents;
    float carrierDecay, carrierSustain;
    float modDecay, modSustain, modRelease;
    float fmDepthLow, fmDepthRange, keyScaleCoeff, noiseBurstAmt;
    float carrierH2, carrierH3, carrierH5;
    float satDrive;      // soft saturation amount
    float reedAsymmetry, barkBloom;
    float velPow;        // velocity power curve exponent
    float velToneMidpoint, velToneSharpness;
    float afterglowAmount, afterglowDecay, afterglowHarmonic;
    float resonanceAmount, resonanceHarmonic;
    // Knob defaults (display units matching sliders)
    float attackMs, releaseMs, fmDepthScaleX, driveKnob;
    float tremoloRateHz, tremoloDepthPct, reverbWetPct;
    float chorusMixPct;  // chorus wet level (0–100%)
};

static const PresetData kPresetWurlitzer {
    // Wurlitzer 200A: metal reed driven by electromagnetic pickup.
    // Character: nasal/reedy from 2:1 FM ratio; very velocity-sensitive;
    // short bark that collapses to a darker sustain tone.
    2.0f,  3.0f,               // modRatio 2:1, tight ±3-cent stereo
    0.35f, 0.40f,              // carrier: fast decay to 40% sustain (percussive)
    0.08f, 0.0f, 0.04f,       // mod: crisp 80ms bark, no sustain (pure tone after bark)
    0.10f, 5.5f, 0.004f, 0.25f, // FM range starts dark at pp; key scale; loud clack
    0.04f, 0.07f, 0.02f,       // sustain harmonics stay lean; attack bloom adds the bark
    1.35f,                     // satDrive: stronger pickup bite than Rhodes
    0.72f, 0.62f,              // asymmetric reed shaping + transient bark bloom
    2.0f,                      // velPow: quadratic — very touch-sensitive
    0.58f, 9.5f,               // tone S-curve: soft lows, bright mid-velocity bark
    0.0f, 0.18f, 4.0f,         // no bell afterglow on Wurlitzer
    0.0f, 3.0f,                // no pedal resonance on Wurlitzer
    1.0f, 400.0f, 1.0f, 1.2f, // attack, release, FM scale, drive knob
    5.0f, 28.0f, 18.0f,        // tremolo 5Hz/28% (signature Wurlitzer tremolo); drier room
    2.0f,                      // chorusMix: nearly dry to avoid dated soft-synth sheen
};

static const PresetData kPresetRhodes {
    // Rhodes Mark II: metal tine + tonebar resonator, felt hammer.
    // Character: bell-like from 1:1 FM ratio; long singing sustain;
    // gentler velocity response than Wurlitzer.
    1.0f,  5.0f,               // modRatio 1:1 (bell), wider ±5-cent shimmer
    1.5f,  0.80f,              // carrier: slow 1.5s decay to 80% sustain (rings long)
    0.20f, 0.15f, 0.15f,      // mod: 200ms bark with 15% sustain (brightness lingers)
    0.20f, 2.2f, 0.0015f, 0.12f, // narrower FM range; minimal key scale; soft clack
    0.03f, 0.05f, 0.00f,       // harmonic mix: light upper partials, near-sine tine core
    0.30f,                     // satDrive: very mild (felt hammer, clean pickup)
    0.0f, 0.0f,                // no Wurlitzer-style reed asymmetry or bark bloom
    1.3f,                      // velPow: gentler curve (Rhodes is more forgiving)
    0.62f, 6.5f,               // tone S-curve: smoother brightening than Wurlitzer
    0.22f, 0.24f, 4.0f,        // note-off bell afterglow for tine/tonebar shimmer
    0.10f, 2.7f,               // sustain-pedal sympathetic resonance for Rhodes body/tonebar bloom
    1.0f, 800.0f, 1.0f, 0.3f, // attack, release, FM scale, drive knob
    4.0f, 12.0f, 48.0f,        // tremolo 4Hz/12% (subtle); more reverb for spaciousness
    28.0f,                     // chorusMix: prominent (Rhodes shimmer is iconic)
};

MainComponent::MainComponent()
{
    synthesiser.addSound(new ElectricPianoSound());
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* voice = new ElectricPianoVoice();
        voice->setParams(&synthParams);
        synthesiser.addVoice(voice);
    }

    titleLabel.setText("Electric Piano", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(36.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    statusLabel.setText("Audio: starting...  |  MIDI: scanning...", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00cc88));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    hintLabel.setText("Play with a MIDI keyboard  |  Sustain pedal (CC#64) supported  |  FX: Chorus + Tremolo + Reverb", juce::dontSendNotification);
    hintLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f)));
    hintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    hintLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(hintLabel);

    settingsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a4a));
    settingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    settingsButton.onClick = [this]
    {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 0, 0, 2, true, false, false, false);
        selector->setSize(450, 300);
        juce::DialogWindow::showDialog("Audio & MIDI Settings", selector, nullptr,
                                       juce::Colours::darkgrey, true);
    };
    addAndMakeVisible(settingsButton);

    // 全音停止ボタン: 音が止まらなくなったときに使う
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff660000));
    panicButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.onClick = [this]
    {
        synthesiser.allNotesOff(0, false);  // 全チャンネル、テールオフなしで即停止
        activeNoteCount.set(0);
    };
    addAndMakeVisible(panicButton);

    // ---- Preset buttons ----
    auto stylePresetBtn = [&](juce::TextButton& btn, bool active)
    {
        btn.setColour(juce::TextButton::buttonColourId,
                      active ? juce::Colour(0xff006644) : juce::Colour(0xff2a2a4a));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    };
    stylePresetBtn(wurlitzerButton, true);
    stylePresetBtn(rhodesButton,    false);
    addAndMakeVisible(wurlitzerButton);
    addAndMakeVisible(rhodesButton);

    wurlitzerButton.onClick = [this] { applyPreset(Preset::Wurlitzer); };
    rhodesButton.onClick    = [this] { applyPreset(Preset::Rhodes);    };

    // ---- Knob setup ----
    auto setupKnob = [&](juce::Slider& s, double lo, double hi, double def, int decimals)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
        s.setRange(lo, hi);
        s.setValue(def, juce::dontSendNotification);
        s.setNumDecimalPlacesToDisplay(decimals);
        s.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(0xff00cc88));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2a3a5a));
        s.setColour(juce::Slider::textBoxTextColourId,         juce::Colour(0xffaaaacc));
        s.setColour(juce::Slider::textBoxBackgroundColourId,   juce::Colour(0xff111122));
        s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colour(0xff2a2a4a));
        addAndMakeVisible(s);
    };
    auto setupLabel = [&](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        l.setColour(juce::Label::textColourId, juce::Colour(0xff9999bb));
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };

    // Ranges use display-friendly units; callbacks convert to internal values.
    setupKnob(sliderTremoloRate,  1.0,   8.0,   5.0,  1);
    setupKnob(sliderTremoloDepth, 0.0,  50.0,  25.0,  0);
    setupKnob(sliderReverbWet,    0.0,  60.0,  35.0,  0);
    setupKnob(sliderFMDepth,      0.5,   2.0,   1.0,  2);
    setupKnob(sliderAttack,       1.0, 200.0,   1.0,  0);
    setupKnob(sliderRelease,     50.0, 2000.0, 400.0, 0);
    setupKnob(sliderDrive,        0.0,   3.0,   1.2,  2);

    sliderTremoloRate .setTextValueSuffix(" Hz");
    sliderTremoloDepth.setTextValueSuffix("%");
    sliderReverbWet   .setTextValueSuffix("%");
    sliderFMDepth     .setTextValueSuffix("x");
    sliderAttack      .setTextValueSuffix("ms");
    sliderRelease     .setTextValueSuffix("ms");

    setupLabel(labelTremoloRate,  "TREMOLO RATE");
    setupLabel(labelTremoloDepth, "TREMOLO DEPTH");
    setupLabel(labelReverbWet,    "REVERB WET");
    setupLabel(labelFMDepth,      "FM DEPTH");
    setupLabel(labelAttack,       "ATTACK");
    setupLabel(labelRelease,      "RELEASE");
    setupLabel(labelDrive,        "DRIVE");

    sliderTremoloRate.onValueChange  = [this] { tremoloRateHz   = (float)sliderTremoloRate.getValue(); };
    sliderTremoloDepth.onValueChange = [this] { tremoloDepthAmt = (float)sliderTremoloDepth.getValue() / 100.0f; };
    sliderReverbWet.onValueChange    = [this] { reverbWetAmt    = (float)sliderReverbWet.getValue()    / 100.0f;
                                                reverbDirty     = true; };
    sliderFMDepth.onValueChange      = [this] { synthParams.fmDepthScale = (float)sliderFMDepth.getValue(); };
    sliderAttack.onValueChange       = [this] { synthParams.attackTime   = (float)sliderAttack.getValue()  / 1000.0f; };
    sliderRelease.onValueChange      = [this] { synthParams.releaseTime  = (float)sliderRelease.getValue() / 1000.0f; };
    sliderDrive.onValueChange        = [this] { synthParams.driveAmount  = (float)sliderDrive.getValue(); };

    setSize(700, 420);
    setAudioChannels(0, 2);
    openAllMidiInputs();
    startTimer(200);
}

MainComponent::~MainComponent()
{
    stopTimer();
    for (auto& m : midiInputs)
        m->stop();
    midiInputs.clear();
    shutdownAudio();
}

void MainComponent::openAllMidiInputs()
{
    // AudioDeviceManager 経由ではなく MidiInput::openDevice で直接開く。
    // こうすることで callback の private 継承問題を回避し、確実に受信できる。
    midiInputs.clear();
    for (auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (auto m = juce::MidiInput::openDevice(info.identifier, this))
        {
            m->start();
            midiInputs.push_back(std::move(m));
        }
    }
}

void MainComponent::updateMidiStatus()
{
    int deviceCount = (int)midiInputs.size();

    auto* dev = deviceManager.getCurrentAudioDevice();
    auto audioState = dev != nullptr
                      ? juce::String("Audio: ") + dev->getName()
                        + " (" + juce::String((int)dev->getCurrentBufferSizeSamples()) + " smp)"
                      : juce::String("Audio: no device");

    auto midiState = deviceCount > 0
                     ? juce::String("MIDI: ") + juce::String(deviceCount) + " device(s)"
                     : juce::String("MIDI: no devices found");

    statusLabel.setText(audioState + "  |  " + midiState, juce::dontSendNotification);
}

void MainComponent::timerCallback()
{
    updateMidiStatus();
    repaint();
}

void MainComponent::applyPreset(Preset p)
{
    currentPreset = p;
    const PresetData& d = (p == Preset::Wurlitzer) ? kPresetWurlitzer : kPresetRhodes;

    // Update voice parameters (audio thread reads these atomically on next note-on).
    synthParams.modRatio       = d.modRatio;
    synthParams.detuneCents    = d.detuneCents;
    synthParams.carrierDecay   = d.carrierDecay;
    synthParams.carrierSustain = d.carrierSustain;
    synthParams.modDecay       = d.modDecay;
    synthParams.modSustain     = d.modSustain;
    synthParams.modRelease     = d.modRelease;
    synthParams.fmDepthLow     = d.fmDepthLow;
    synthParams.fmDepthRange   = d.fmDepthRange;
    synthParams.keyScaleCoeff  = d.keyScaleCoeff;
    synthParams.noiseBurstAmt  = d.noiseBurstAmt;
    synthParams.carrierH2      = d.carrierH2;
    synthParams.carrierH3      = d.carrierH3;
    synthParams.carrierH5      = d.carrierH5;
    synthParams.satDrive       = d.satDrive;
    synthParams.reedAsymmetry  = d.reedAsymmetry;
    synthParams.barkBloom      = d.barkBloom;
    synthParams.velPow         = d.velPow;
    synthParams.velToneMidpoint  = d.velToneMidpoint;
    synthParams.velToneSharpness = d.velToneSharpness;
    synthParams.afterglowAmount  = d.afterglowAmount;
    synthParams.afterglowDecay   = d.afterglowDecay;
    synthParams.afterglowHarmonic = d.afterglowHarmonic;
    synthParams.resonanceAmount  = d.resonanceAmount;
    synthParams.resonanceHarmonic = d.resonanceHarmonic;
    synthParams.fmDepthScale   = d.fmDepthScaleX;
    synthParams.attackTime     = d.attackMs  / 1000.0f;
    synthParams.releaseTime    = d.releaseMs / 1000.0f;
    synthParams.driveAmount    = d.driveKnob;

    // Chorus: update target (audio thread applies next block).
    chorusMixAmt = d.chorusMixPct / 100.0f;

    // Snap knobs to preset defaults (triggers onValueChange → atomics update).
    sliderAttack      .setValue(d.attackMs,        juce::sendNotification);
    sliderRelease     .setValue(d.releaseMs,       juce::sendNotification);
    sliderFMDepth     .setValue(d.fmDepthScaleX,   juce::sendNotification);
    sliderDrive       .setValue(d.driveKnob,       juce::sendNotification);
    sliderTremoloRate .setValue(d.tremoloRateHz,   juce::sendNotification);
    sliderTremoloDepth.setValue(d.tremoloDepthPct, juce::sendNotification);
    sliderReverbWet   .setValue(d.reverbWetPct,    juce::sendNotification);

    // Update button highlight.
    bool isWurl = (p == Preset::Wurlitzer);
    wurlitzerButton.setColour(juce::TextButton::buttonColourId,
                              isWurl ? juce::Colour(0xff006644) : juce::Colour(0xff2a2a4a));
    rhodesButton.setColour(juce::TextButton::buttonColourId,
                           isWurl ? juce::Colour(0xff2a2a4a) : juce::Colour(0xff006644));

    // Stop any hanging notes so changes take effect cleanly.
    synthesiser.allNotesOff(0, true);
}

// ---- AudioAppComponent ----

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    synthesiser.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate);
    pedalNoiseEnv = 0.0f;
    pedalNoiseDecay = 0.0f;
    pedalNoiseFilter = 0.0f;
    pedalNoiseDirection = 0.0f;
    pedalNoiseThumpPhase = 0.0;
    pedalNoiseThumpDelta = 0.0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlockExpected;
    spec.numChannels      = 2;

    // Chorus: subtle width/shimmer. Wurlitzer itself doesn't use chorus;
    // we keep it light so the nasal FM character stays intact.
    fxChorus.prepare(spec);
    fxChorus.setRate(0.6f);           // 0.6 Hz — slow shimmer
    fxChorus.setDepth(0.15f);         // light depth — don't blur the reed attack
    fxChorus.setCentreDelay(6.0f);    // 6 ms base delay
    fxChorus.setFeedback(0.05f);      // minimal feedback
    fxChorus.setMix(0.25f);           // 25% wet — subtle (was 50% for Rhodes style)

    // Tremolo: iconic Wurlitzer 145 amplifier effect.
    // Placed BEFORE reverb so the decay tail isn't chopped by the LFO.
    tremoloPhaseDelta = (double)tremoloRateHz.load() / sampleRate * juce::MathConstants<double>::twoPi;
    tremoloPhase = 0.0;

    // Reverb: spacious room for dreampop ambience (applied last)
    fxReverb.prepare(spec);
    juce::dsp::Reverb::Parameters rvp;
    rvp.roomSize   = 0.75f;
    rvp.damping    = 0.45f;
    rvp.wetLevel   = 0.35f;
    rvp.dryLevel   = 0.65f;
    rvp.width      = 1.0f;
    rvp.freezeMode = 0.0f;
    fxReverb.setParameters(rvp);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Apply chorus mix change (preset switch or future knob).
    {
        float newMix = chorusMixAmt.load();
        if (newMix != lastChorusMix)
        {
            fxChorus.setMix(newMix);
            lastChorusMix = newMix;
        }
    }

    // Apply reverb parameter changes requested from UI thread.
    if (reverbDirty.exchange(false))
    {
        float wet = reverbWetAmt.load();
        juce::dsp::Reverb::Parameters rvp;
        rvp.roomSize   = 0.75f;
        rvp.damping    = 0.45f;
        rvp.wetLevel   = wet;
        rvp.dryLevel   = 1.0f - wet;
        rvp.width      = 1.0f;
        rvp.freezeMode = 0.0f;
        fxReverb.setParameters(rvp);
    }

    bufferToFill.clearActiveBufferRegion();

    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);

    synthesiser.renderNextBlock(*bufferToFill.buffer, incomingMidi,
                                bufferToFill.startSample, bufferToFill.numSamples);

    // Effects chain: Chorus → Tremolo → Reverb
    {
        auto block = juce::dsp::AudioBlock<float>(*bufferToFill.buffer)
                         .getSubBlock((size_t)bufferToFill.startSample,
                                      (size_t)bufferToFill.numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxChorus.process(ctx);
    }

    // Tremolo: amplitude modulation via sine LFO (Wurlitzer 145 amp character).
    // Applied sample-by-sample after chorus, before reverb, so the reverb tail
    // is smooth rather than pulsing with the LFO.
    {
        const float  depth = tremoloDepthAmt.load();
        tremoloPhaseDelta  = (double)tremoloRateHz.load() / currentSampleRate
                             * juce::MathConstants<double>::twoPi;

        auto* L = bufferToFill.buffer->getWritePointer(0);
        auto* R = (bufferToFill.buffer->getNumChannels() >= 2)
                  ? bufferToFill.buffer->getWritePointer(1) : nullptr;
        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            float trem = 1.0f - depth * (float)((1.0 - std::cos(tremoloPhase)) * 0.5);
            L[bufferToFill.startSample + i] *= trem;
            if (R) R[bufferToFill.startSample + i] *= trem;
            tremoloPhase += tremoloPhaseDelta;
        }
        tremoloPhase = std::fmod(tremoloPhase, juce::MathConstants<double>::twoPi);
    }

    // Pedal noise: short mechanical thunk/click when sustain pedal changes state.
    // Added after tremolo so the effect doesn't wobble with the amp modulation.
    {
        const int pedalNoiseEvent = pendingPedalNoise.exchange(0);
        if (pedalNoiseEvent != 0 && currentSampleRate > 0.0)
        {
            const bool pedalDownEvent = pedalNoiseEvent > 0;
            pedalNoiseDirection = pedalDownEvent ? 1.0f : -1.0f;
            pedalNoiseEnv = pedalDownEvent ? 0.020f : 0.014f;

            const float decaySeconds = pedalDownEvent ? 0.055f : 0.032f;
            pedalNoiseDecay = (float) std::pow(0.001f, 1.0f / ((float) currentSampleRate * decaySeconds));

            const float thunkFrequency = pedalDownEvent ? 180.0f : 320.0f;
            pedalNoiseThumpDelta = (double) thunkFrequency / currentSampleRate
                                   * juce::MathConstants<double>::twoPi;
        }

        if (pedalNoiseEnv > 0.00005f)
        {
            auto* L = bufferToFill.buffer->getWritePointer(0);
            auto* R = (bufferToFill.buffer->getNumChannels() >= 2)
                      ? bufferToFill.buffer->getWritePointer(1) : nullptr;

            for (int i = 0; i < bufferToFill.numSamples; ++i)
            {
                const int sampleIndex = bufferToFill.startSample + i;
                const float noise = pedalNoiseRng.nextFloat() * 2.0f - 1.0f;
                pedalNoiseFilter = 0.84f * pedalNoiseFilter + 0.16f * noise;

                const float thunk = (float) std::sin(pedalNoiseThumpPhase);
                const float click = noise - pedalNoiseFilter;
                const float mechanical = pedalNoiseDirection > 0.0f
                                             ? 0.60f * thunk + 0.40f * pedalNoiseFilter
                                             : 0.22f * thunk + 0.78f * click;
                const float pedalSample = pedalNoiseEnv * mechanical;

                L[sampleIndex] += pedalSample;
                if (R != nullptr)
                    R[sampleIndex] += pedalSample * 0.96f;

                pedalNoiseEnv *= pedalNoiseDecay;
                pedalNoiseThumpPhase += pedalNoiseThumpDelta;
            }

            pedalNoiseThumpPhase = std::fmod(pedalNoiseThumpPhase, juce::MathConstants<double>::twoPi);
        }
    }

    {
        auto block = juce::dsp::AudioBlock<float>(*bufferToFill.buffer)
                         .getSubBlock((size_t)bufferToFill.startSample,
                                      (size_t)bufferToFill.numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        fxReverb.process(ctx);
    }
}

void MainComponent::releaseResources()
{
    fxChorus.reset();
    fxReverb.reset();
}

// ---- MidiInputCallback (MIDI スレッドから呼ばれる) ----

void MainComponent::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    midiCollector.addMessageToQueue(message);

    // UI indicator updates (no effect on audio processing).
    if (message.isNoteOn())
        activeNoteCount.set(activeNoteCount.get() + 1);
    else if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0))
        activeNoteCount.set(juce::jmax(0, activeNoteCount.get() - 1));
    else if (message.isController() && message.getControllerNumber() == 64)
    {
        const int newPedalState = message.getControllerValue() >= 64 ? 1 : 0;
        const int previousPedalState = sustainPedalDown.get();
        sustainPedalDown.set(newPedalState);

        if (newPedalState != previousPedalState)
            pendingPedalNoise = newPedalState != 0 ? 1 : -1;
    }
}

// ---- Component ----

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Note-active LED (green)
    bool anyActive = activeNoteCount.get() > 0;
    g.setColour(anyActive ? juce::Colours::limegreen : juce::Colour(0xff334433));
    g.fillEllipse(getWidth() - 24.0f, 10.0f, 12.0f, 12.0f);

    // Sustain pedal LED (blue)
    bool sustain = sustainPedalDown.get() != 0;
    g.setColour(sustain ? juce::Colour(0xff44aaff) : juce::Colour(0xff223344));
    g.fillEllipse(getWidth() - 42.0f, 10.0f, 12.0f, 12.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);

    // Title row: label on the left, preset buttons on the right
    {
        auto row = area.removeFromTop(50);
        rhodesButton.setBounds(row.removeFromRight(100).reduced(0, 10));
        row.removeFromRight(6);
        wurlitzerButton.setBounds(row.removeFromRight(120).reduced(0, 10));
        titleLabel.setBounds(row);
    }
    area.removeFromTop(8);
    statusLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(12);
    hintLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(16);

    const int kLabelH = 14;
    const int kKnobH  = 80;
    const int cellW   = area.getWidth() / 4;

    // Row 1: TremoloRate | TremoloDepth | ReverbWet | FMDepth
    {
        auto row = area.removeFromTop(kLabelH + kKnobH);
        auto placeKnob = [&](juce::Label& lbl, juce::Slider& sld)
        {
            auto cell = row.removeFromLeft(cellW);
            lbl.setBounds(cell.removeFromTop(kLabelH));
            sld.setBounds(cell.reduced(10, 0));
        };
        placeKnob(labelTremoloRate,  sliderTremoloRate);
        placeKnob(labelTremoloDepth, sliderTremoloDepth);
        placeKnob(labelReverbWet,    sliderReverbWet);
        placeKnob(labelFMDepth,      sliderFMDepth);
    }

    area.removeFromTop(10);

    // Row 2: Attack | Release | Drive — 3 knobs centered in the 4-cell width.
    // Skip cellW/2 on each side so 3×cellW is perfectly centered in 4×cellW.
    {
        auto row = area.removeFromTop(kLabelH + kKnobH);
        row.removeFromLeft(cellW / 2);
        auto placeKnob = [&](juce::Label& lbl, juce::Slider& sld)
        {
            auto cell = row.removeFromLeft(cellW);
            lbl.setBounds(cell.removeFromTop(kLabelH));
            sld.setBounds(cell.reduced(10, 0));
        };
        placeKnob(labelAttack,  sliderAttack);
        placeKnob(labelRelease, sliderRelease);
        placeKnob(labelDrive,   sliderDrive);
    }

    auto bottom = getLocalBounds().removeFromBottom(40).reduced(8);
    panicButton.setBounds(bottom.removeFromLeft(80));
    bottom.removeFromLeft(8);
    settingsButton.setBounds(bottom.removeFromRight(110));
}
