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
    // Apply vintage look to this component and all children
    setLookAndFeel(&vintageTheme);

    synthesiser.addSound(new ElectricPianoSound());
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* voice = new ElectricPianoVoice();
        voice->setParams(&synthParams);
        synthesiser.addVoice(voice);
    }

    // ---- Title ----
    titleLabel.setText("ELECTRIC PIANO", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(24.0f).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    // ---- Status ----
    statusLabel.setText("Audio: starting...  |  MIDI: scanning...", juce::dontSendNotification);
    statusLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(VintageLookAndFeel::kColourTextDim));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    // hintLabel not shown in vintage layout
    addChildComponent(hintLabel);

    // ---- Buttons ----
    settingsButton.onClick = [this]
    {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 0, 0, 2, true, false, false, false);
        selector->setSize(450, 300);
        juce::DialogWindow::showDialog("Audio & MIDI Settings", selector, nullptr,
                                       juce::Colour(VintageLookAndFeel::kColourPanel), true);
    };
    addAndMakeVisible(settingsButton);

    panicButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff5a1010));
    panicButton.setColour(juce::TextButton::textColourOffId, juce::Colour(VintageLookAndFeel::kColourText));
    panicButton.onClick = [this]
    {
        synthesiser.allNotesOff(0, false);
        activeNoteCount.set(0);
    };
    addAndMakeVisible(panicButton);

    // Preset buttons: use toggle state so VintageLookAndFeel can highlight the active one
    wurlitzerButton.setClickingTogglesState(false);
    rhodesButton.setClickingTogglesState(false);
    wurlitzerButton.setToggleState(true,  juce::dontSendNotification);
    rhodesButton.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(wurlitzerButton);
    addAndMakeVisible(rhodesButton);
    wurlitzerButton.onClick = [this] { applyPreset(Preset::Wurlitzer); };
    rhodesButton.onClick    = [this] { applyPreset(Preset::Rhodes);    };

    // ---- Knob helpers ----
    auto setupKnob = [&](juce::Slider& s, double lo, double hi, double def, int decimals)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
        s.setRange(lo, hi);
        s.setValue(def, juce::dontSendNotification);
        s.setNumDecimalPlacesToDisplay(decimals);
        addAndMakeVisible(s);
    };
    auto setupLabel = [&](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };

    // ---- SOUND section knobs ----
    setupKnob(sliderFMDepth,  0.5,   2.0,   1.0,  2);  sliderFMDepth .setTextValueSuffix("x");
    setupKnob(sliderAttack,   1.0, 200.0,   1.0,  0);  sliderAttack  .setTextValueSuffix("ms");
    setupKnob(sliderRelease, 50.0, 2000.0, 400.0, 0);  sliderRelease .setTextValueSuffix("ms");
    setupKnob(sliderDrive,    0.0,   3.0,   1.2,  2);

    setupLabel(labelFMDepth,  "FM DEPTH");
    setupLabel(labelAttack,   "ATTACK");
    setupLabel(labelRelease,  "RELEASE");
    setupLabel(labelDrive,    "DRIVE");

    sliderFMDepth .onValueChange = [this] { synthParams.fmDepthScale = (float)sliderFMDepth .getValue(); };
    sliderAttack  .onValueChange = [this] { synthParams.attackTime   = (float)sliderAttack  .getValue() / 1000.0f; };
    sliderRelease .onValueChange = [this] { synthParams.releaseTime  = (float)sliderRelease .getValue() / 1000.0f; };
    sliderDrive   .onValueChange = [this] { synthParams.driveAmount  = (float)sliderDrive   .getValue(); };

    // ---- FX section knobs ----
    setupKnob(sliderTremoloRate,  1.0,   8.0,  5.0,  1);  sliderTremoloRate .setTextValueSuffix(" Hz");
    setupKnob(sliderTremoloDepth, 0.0,  50.0, 25.0,  0);  sliderTremoloDepth.setTextValueSuffix("%");
    setupKnob(sliderReverbWet,    0.0,  60.0, 35.0,  0);  sliderReverbWet   .setTextValueSuffix("%");
    setupKnob(sliderChorus,       0.0,  50.0,  2.0,  0);  sliderChorus      .setTextValueSuffix("%");

    setupLabel(labelTremoloRate,  "TREM RATE");
    setupLabel(labelTremoloDepth, "TREM DEPTH");
    setupLabel(labelReverbWet,    "REVERB");
    setupLabel(labelChorus,       "CHORUS");

    sliderTremoloRate .onValueChange = [this] { tremoloRateHz   = (float)sliderTremoloRate .getValue(); };
    sliderTremoloDepth.onValueChange = [this] { tremoloDepthAmt = (float)sliderTremoloDepth.getValue() / 100.0f; };
    sliderReverbWet   .onValueChange = [this] { reverbWetAmt    = (float)sliderReverbWet   .getValue() / 100.0f;
                                                reverbDirty     = true; };
    sliderChorus      .onValueChange = [this] { chorusMixAmt    = (float)sliderChorus      .getValue() / 100.0f; };

    setSize(800, 520);
    setAudioChannels(0, 2);
    openAllMidiInputs();
    startTimer(80);   // ~12fps repaint for VU meter
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
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
    sliderChorus      .setValue(d.chorusMixPct,    juce::sendNotification);

    // Toggle state drives VintageLookAndFeel button highlight.
    bool isWurl = (p == Preset::Wurlitzer);
    wurlitzerButton.setToggleState( isWurl, juce::dontSendNotification);
    rhodesButton   .setToggleState(!isWurl, juce::dontSendNotification);

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

    // VU meter: exponential-smoothed RMS (attack fast, release slow)
    {
        float sumSq = 0.0f;
        const float* L = bufferToFill.buffer->getReadPointer(0);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
        {
            const float s = L[bufferToFill.startSample + i];
            sumSq += s * s;
        }
        const float rms     = std::sqrt(sumSq / (float)juce::jmax(1, bufferToFill.numSamples));
        const float current = vuLevel.load();
        const float alpha   = (rms > current) ? 0.80f : 0.96f;   // fast attack, slow release
        vuLevel.store(current * alpha + rms * (1.0f - alpha));
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
    using C = VintageLookAndFeel;
    const int W = getWidth();
    const int H = getHeight();

    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll(juce::Colour(C::kColourBody));

    // Header panel (darker walnut)
    g.setColour(juce::Colour(C::kColourPanel));
    g.fillRect(0, 0, W, 64);

    // Header bottom edge
    g.setColour(juce::Colour(C::kColourDivider));
    g.fillRect(0, 63, W, 2);

    // ── "ELECTRIC PIANO" vintage logo engraving ───────────────────────────────
    // (title label is drawn by JUCE on top; this adds a subtle emboss shadow)
    g.setColour(juce::Colour(C::kColourDivider).withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(24.0f).withStyle("Bold")));
    g.drawText("ELECTRIC PIANO", 16 + 1, 20 + 1, 280, 28, juce::Justification::centredLeft);

    // ── Preset region label ───────────────────────────────────────────────────
    g.setColour(juce::Colour(C::kColourTextDim));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
    g.drawText("MODEL", W - 14 - 238, 8, 40, 10, juce::Justification::centredLeft);

    // ── LED indicators (top-right corner) ────────────────────────────────────
    const bool anyActive = activeNoteCount.get() > 0;
    const bool sustain   = sustainPedalDown.get() != 0;
    const float ledD  = 9.0f;
    const float ledY  = 10.0f;
    const float ledX1 = (float)(W - 14 - 9);   // rightmost: note-active (amber)
    const float ledX2 = ledX1 - 17.0f;          // sustain (cyan/blue)

    // LED halos
    if (anyActive)
    {
        g.setColour(juce::Colour(C::kColourLEDOn).withAlpha(0.22f));
        g.fillEllipse(ledX1 - 4.0f, ledY - 4.0f, ledD + 8.0f, ledD + 8.0f);
    }
    if (sustain)
    {
        g.setColour(juce::Colour(0xff00aadd).withAlpha(0.18f));
        g.fillEllipse(ledX2 - 4.0f, ledY - 4.0f, ledD + 8.0f, ledD + 8.0f);
    }

    // LED bodies
    g.setColour(anyActive ? juce::Colour(C::kColourLEDOn) : juce::Colour(C::kColourLEDOff));
    g.fillEllipse(ledX1, ledY, ledD, ledD);
    g.setColour(sustain ? juce::Colour(0xff00aadd) : juce::Colour(0xff0a2030));
    g.fillEllipse(ledX2, ledY, ledD, ledD);

    // Tiny LED labels
    g.setColour(juce::Colour(C::kColourTextDim));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(7.5f)));
    g.drawText("KEY", (int)ledX1 - 2, (int)(ledY + ledD + 1), 14, 8, juce::Justification::centred);
    g.drawText("SUS", (int)ledX2 - 2, (int)(ledY + ledD + 1), 14, 8, juce::Justification::centred);

    // ── Section panel backgrounds ─────────────────────────────────────────────
    const int kSectionTop = 66;
    const int kSectionH   = 190;
    const int kDivX       = W * 54 / 100;

    // Subtle inner glow border around sections
    g.setColour(juce::Colour(C::kColourDivider).withAlpha(0.45f));
    g.drawRect(14, kSectionTop, kDivX - 20, kSectionH, 1);
    g.drawRect(kDivX + 6, kSectionTop, W - kDivX - 20, kSectionH, 1);

    // Section title labels
    g.setColour(juce::Colour(C::kColourTextDim));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
    g.drawText("SOUND", 20, kSectionTop + 5, 50, 11, juce::Justification::centredLeft);
    g.drawText("FX",    kDivX + 12, kSectionTop + 5, 30, 11, juce::Justification::centredLeft);

    // Vertical divider between SOUND and FX
    g.setColour(juce::Colour(C::kColourDivider));
    g.fillRect(kDivX, kSectionTop + 16, 1, kSectionH - 20);

    // ── Chassis decorative area ───────────────────────────────────────────────
    const int chassisTop = kSectionTop + kSectionH + 8;
    const int chassisH   = H - chassisTop - 56;
    if (chassisH > 20)
    {
        // Inset recessed panel
        g.setColour(juce::Colour(0xff161410));
        g.fillRect(14, chassisTop, W - 28, chassisH);
        g.setColour(juce::Colour(C::kColourDivider).withAlpha(0.30f));
        g.drawRect(14, chassisTop, W - 28, chassisH, 1);

        // Model name plate
        const juce::String modelName = (currentPreset == Preset::Wurlitzer)
                                        ? "WURLITZER 200A"
                                        : "RHODES MARK II";
        g.setColour(juce::Colour(C::kColourKnobCap).withAlpha(0.55f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(18.0f).withStyle("Bold")));
        g.drawText(modelName, 14, chassisTop + chassisH / 2 - 14, W - 28, 28,
                   juce::Justification::centred);

        // Decorative vent lines
        g.setColour(juce::Colour(C::kColourDivider).withAlpha(0.25f));
        const int ventY0 = chassisTop + 8;
        for (int v = 0; v < 4; ++v)
        {
            int vy = ventY0 + v * 5;
            if (vy + 2 < chassisTop + chassisH - 8)
                g.fillRect(W / 2 - 60, vy, 120, 2);
        }
    }

    // ── VU meter ──────────────────────────────────────────────────────────────
    const int vuY  = H - 50;
    const int vuX  = 14;
    const int vuW  = W - 28;
    const int vuH  = 12;

    // dB tick marks (above bar)
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f)));
    for (float db : { -30.0f, -20.0f, -12.0f, -6.0f, -3.0f })
    {
        const float t  = juce::jmap(db, -42.0f, 0.0f, 0.0f, 1.0f);
        const int   tx = vuX + (int)(t * (float)vuW);
        g.setColour(juce::Colour(C::kColourTextDim).withAlpha(0.45f));
        g.fillRect(tx, vuY - 6, 1, 4);
        g.setColour(juce::Colour(C::kColourTextDim).withAlpha(0.6f));
        g.drawText(juce::String((int)db), tx - 8, vuY - 16, 20, 10,
                   juce::Justification::centred);
    }
    // 0 dB marker (brighter)
    {
        const int tx = vuX + vuW;
        g.setColour(juce::Colour(C::kColourLEDOn).withAlpha(0.55f));
        g.fillRect(tx - 1, vuY - 6, 1, 4);
        g.setColour(juce::Colour(C::kColourLEDOn).withAlpha(0.8f));
        g.drawText("0", tx - 8, vuY - 16, 20, 10, juce::Justification::centred);
    }

    // Bar background
    g.setColour(juce::Colour(C::kColourTrackBg));
    g.fillRoundedRectangle((float)vuX, (float)vuY, (float)vuW, (float)vuH, 3.0f);

    // Bar fill with green→amber→red gradient
    const float levelRMS   = vuLevel.load();
    const float levelDB    = 20.0f * std::log10(juce::jmax(levelRMS, 1e-5f));
    const float normalised = juce::jlimit(0.0f, 1.0f,
                                           juce::jmap(levelDB, -42.0f, 0.0f, 0.0f, 1.0f));
    if (normalised > 0.001f)
    {
        const float fillW = normalised * (float)vuW;
        juce::ColourGradient grad (juce::Colour(0xff3aaa1a), (float)vuX,        0.0f,
                                    juce::Colour(0xffff2200), (float)(vuX + vuW), 0.0f, false);
        grad.addColour(0.65, juce::Colour(C::kColourTrackFill));
        grad.addColour(0.85, juce::Colour(C::kColourLEDOn));
        g.setGradientFill(grad);
        g.fillRoundedRectangle((float)vuX, (float)vuY, fillW, (float)vuH, 3.0f);
    }

    // "VU" label
    g.setColour(juce::Colour(C::kColourTextDim));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
    g.drawText("VU", vuX, vuY - 16, 18, 10, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // ── Header row (y 0–64) ───────────────────────────────────────────────────
    // Title label (left)
    titleLabel.setBounds(16, 18, 260, 32);

    // Preset buttons (right side of header)
    const int btnY = 16, btnH = 32;
    rhodesButton   .setBounds(W - 14 - 96,       btnY, 96,  btnH);
    wurlitzerButton.setBounds(W - 14 - 96 - 110, btnY, 106, btnH);

    // Settings button (bottom-right)
    settingsButton.setBounds(W - 14 - 110, H - 44, 110, 34);

    // Panic button (bottom-left)
    panicButton.setBounds(14, H - 44, 78, 34);

    // Status label (bottom, left of center)
    statusLabel.setBounds(100, H - 40, W / 2, 14);

    // ── Knob sections ─────────────────────────────────────────────────────────
    const int kSectionTop = 66;
    const int kDivX       = W * 54 / 100;   // divider x between SOUND and FX sections
    const int kLabelH     = 14;
    const int kKnobH      = 110;            // slider bounds height (knob + text box)
    const int kSectionPad = 16;             // top padding inside section (below section title)

    // SOUND section: 4 knobs (FM DEPTH, ATTACK, RELEASE, DRIVE)
    {
        const int secW  = kDivX - 14 - 8;          // section width
        const int cellW = secW / 4;
        int x = 14 + 4;
        const int y0 = kSectionTop + kSectionPad;

        auto placeKnob = [&](juce::Label& lbl, juce::Slider& sld)
        {
            lbl.setBounds(x, y0,            cellW, kLabelH);
            sld.setBounds(x, y0 + kLabelH,  cellW, kKnobH);
            x += cellW;
        };
        placeKnob(labelFMDepth,  sliderFMDepth);
        placeKnob(labelAttack,   sliderAttack);
        placeKnob(labelRelease,  sliderRelease);
        placeKnob(labelDrive,    sliderDrive);
    }

    // FX section: 4 knobs (TREM RATE, TREM DEPTH, REVERB, CHORUS)
    {
        const int secW  = W - kDivX - 6 - 14;
        const int cellW = secW / 4;
        int x = kDivX + 8;
        const int y0 = kSectionTop + kSectionPad;

        auto placeKnob = [&](juce::Label& lbl, juce::Slider& sld)
        {
            lbl.setBounds(x, y0,            cellW, kLabelH);
            sld.setBounds(x, y0 + kLabelH,  cellW, kKnobH);
            x += cellW;
        };
        placeKnob(labelTremoloRate,  sliderTremoloRate);
        placeKnob(labelTremoloDepth, sliderTremoloDepth);
        placeKnob(labelReverbWet,    sliderReverbWet);
        placeKnob(labelChorus,       sliderChorus);
    }
}
