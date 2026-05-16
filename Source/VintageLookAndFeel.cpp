#include "VintageLookAndFeel.h"

VintageLookAndFeel::VintageLookAndFeel()
{
    bodyFont  = juce::Font (juce::FontOptions{}.withHeight (13.0f));
    labelFont = juce::Font (juce::FontOptions{}.withHeight (11.0f));

    // Global colour overrides used by any component that doesn't have an explicit override.
    setColour (juce::ResizableWindow::backgroundColourId,         juce::Colour (kColourBody));
    setColour (juce::Label::textColourId,                         juce::Colour (kColourText));
    setColour (juce::Label::backgroundColourId,                   juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId,            juce::Colour (kColourTrackFill));
    setColour (juce::Slider::rotarySliderOutlineColourId,         juce::Colour (kColourTrackBg));
    setColour (juce::Slider::textBoxTextColourId,                 juce::Colour (kColourTextDim));
    setColour (juce::Slider::textBoxBackgroundColourId,           juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,              juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId,                  juce::Colour (kColourButtonNorm));
    setColour (juce::TextButton::buttonOnColourId,                juce::Colour (kColourButtonAct));
    setColour (juce::TextButton::textColourOffId,                 juce::Colour (kColourText));
    setColour (juce::TextButton::textColourOnId,                  juce::Colour (kColourLEDOn));
    setColour (juce::ComboBox::backgroundColourId,                juce::Colour (kColourButtonNorm));
    setColour (juce::ComboBox::textColourId,                      juce::Colour (kColourText));
    setColour (juce::ComboBox::outlineColourId,                   juce::Colour (kColourDivider));
}

// ============================================================================
// Rotary Knob
// ============================================================================
void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float startAngle, float endAngle,
                                            juce::Slider& /*slider*/)
{
    const float cx = (float)x + (float)width  * 0.5f;
    const float cy = (float)y + (float)height * 0.5f;
    const float outerR = juce::jmin ((float)width, (float)height) * 0.42f;
    const float knobR  = outerR * 0.78f;
    const float trackW = outerR * 0.15f;

    // --- Arc track background ---
    {
        juce::Path trackBg;
        trackBg.addCentredArc (cx, cy, outerR, outerR, 0.0f,
                                startAngle, endAngle, true);
        juce::PathStrokeType stroke (trackW, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);
        g.setColour (juce::Colour (kColourTrackBg));
        g.strokePath (trackBg, stroke);
    }

    // --- Arc track fill ---
    {
        const float fillEnd = startAngle + (endAngle - startAngle) * sliderPos;
        juce::Path trackFill;
        trackFill.addCentredArc (cx, cy, outerR, outerR, 0.0f,
                                  startAngle, fillEnd, true);
        juce::PathStrokeType stroke (trackW, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);
        g.setColour (juce::Colour (kColourTrackFill));
        g.strokePath (trackFill, stroke);
    }

    // --- Knob body: multi-layer for depth ---
    // Shadow rim
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - knobR - 2.0f, cy - knobR - 2.0f,
                   (knobR + 2.0f) * 2.0f, (knobR + 2.0f) * 2.0f);

    // Outer ring (brass/metal feel)
    {
        juce::ColourGradient grad (juce::Colour (kColourKnobRing).brighter (0.2f), cx - knobR, cy - knobR,
                                    juce::Colour (kColourKnobRing).darker  (0.5f), cx + knobR, cy + knobR,
                                    false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
    }

    // Cap highlight
    const float capR = knobR * 0.82f;
    {
        juce::ColourGradient grad (juce::Colour (kColourKnobCap).brighter (0.3f), cx - capR * 0.3f, cy - capR * 0.5f,
                                    juce::Colour (kColourKnobBase),               cx + capR * 0.3f, cy + capR * 0.5f,
                                    false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    }

    // Small specular dot
    g.setColour (juce::Colour (kColourKnobCap).brighter (0.9f).withAlpha (0.4f));
    g.fillEllipse (cx - capR * 0.28f, cy - capR * 0.52f, capR * 0.28f, capR * 0.28f);

    // --- Indicator line ---
    const float angle = startAngle + (endAngle - startAngle) * sliderPos - juce::MathConstants<float>::halfPi;
    const float lineInner = capR * 0.40f;
    const float lineOuter = capR * 0.88f;
    const float lx1 = cx + std::cos (angle) * lineInner;
    const float ly1 = cy + std::sin (angle) * lineInner;
    const float lx2 = cx + std::cos (angle) * lineOuter;
    const float ly2 = cy + std::sin (angle) * lineOuter;

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (lx1 + 0.5f, ly1 + 0.5f, lx2 + 0.5f, ly2 + 0.5f, 2.4f);
    g.setColour (juce::Colour (kColourText).withAlpha (0.95f));
    g.drawLine (lx1, ly1, lx2, ly2, 2.0f);
}

// ============================================================================
// Text box below knob
// ============================================================================
juce::Label* VintageLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (labelFont);
    label->setColour (juce::Label::textColourId,       juce::Colour (kColourTextDim));
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId,    juce::Colours::transparentBlack);
    label->setColour (juce::TextEditor::textColourId,           juce::Colour (kColourText));
    label->setColour (juce::TextEditor::backgroundColourId,     juce::Colour (kColourButtonDown));
    label->setColour (juce::TextEditor::highlightColourId,      juce::Colour (kColourTrackFill).withAlpha (0.5f));
    label->setColour (juce::TextEditor::highlightedTextColourId, juce::Colour (kColourText));
    return label;
}

// ============================================================================
// Button background
// ============================================================================
void VintageLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& /*backgroundColour*/,
                                                bool isMouseOver, bool isButtonDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float cornerR = 4.0f;

    juce::Colour base;
    if (button.getToggleState())
        base = juce::Colour (kColourButtonAct);
    else if (isButtonDown)
        base = juce::Colour (kColourButtonDown);
    else if (isMouseOver)
        base = juce::Colour (kColourButtonOver);
    else
        base = juce::Colour (kColourButtonNorm);

    // Fill
    {
        juce::ColourGradient grad (base.brighter (0.12f), bounds.getX(),      bounds.getY(),
                                    base.darker   (0.18f), bounds.getX(), bounds.getBottom(),
                                    false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerR);
    }

    // Border
    g.setColour (juce::Colour (kColourDivider));
    g.drawRoundedRectangle (bounds, cornerR, 1.0f);

    // Active glow on bottom edge
    if (button.getToggleState())
    {
        g.setColour (juce::Colour (kColourLEDOn).withAlpha (0.55f));
        g.drawLine (bounds.getX() + cornerR, bounds.getBottom() - 0.5f,
                    bounds.getRight() - cornerR, bounds.getBottom() - 0.5f, 1.5f);
    }
}

// ============================================================================
// Button text
// ============================================================================
void VintageLookAndFeel::drawButtonText (juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool /*isMouseOver*/, bool /*isButtonDown*/)
{
    const juce::Colour textColour = button.getToggleState()
                                        ? juce::Colour (kColourLEDOn)
                                        : juce::Colour (kColourText);
    g.setColour (textColour);
    g.setFont (bodyFont.withHeight (12.0f));

    g.drawFittedText (button.getButtonText(),
                      button.getLocalBounds().reduced (4, 2),
                      juce::Justification::centred, 1);
}

// ============================================================================
// Label font
// ============================================================================
juce::Font VintageLookAndFeel::getLabelFont (juce::Label&)
{
    return labelFont;
}
