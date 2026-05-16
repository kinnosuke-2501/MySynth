#pragma once
#include <JuceHeader.h>

// ============================================================================
// VintageLookAndFeel
// "70年代製造の電子ピアノがデスクに置かれている" コンセプト
//
// カラーパレット:
//   本体:    #1C1B18  ウォームブラック
//   パネル:  #2A2416  ダークウォルナット
//   ノブ:    #C8922A  アンバーゴールド真鍮
//   テキスト:#F0EDE4  クリームホワイト
//   LED on:  #FF8C00  アンバーオレンジ
//   アクセント:#3A7D6E  ヴィンテージティール
// ============================================================================
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();

    // --- Rotary knob ---
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    // --- Text box (value readout below the knob) ---
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // --- Push button (preset + utility buttons) ---
    void drawButtonBackground (juce::Graphics&, juce::Button&,
                                const juce::Colour& backgroundColour,
                                bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool isMouseOverButton, bool isButtonDown) override;

    // --- Label ---
    juce::Font getLabelFont (juce::Label&) override;

    // Named colours for use throughout the UI
    static constexpr juce::uint32 kColourBody       = 0xff1c1b18;
    static constexpr juce::uint32 kColourPanel      = 0xff2a2416;
    static constexpr juce::uint32 kColourKnobBase   = 0xff3a3020;
    static constexpr juce::uint32 kColourKnobCap    = 0xffc8922a;
    static constexpr juce::uint32 kColourKnobRing   = 0xff8a6010;
    static constexpr juce::uint32 kColourTrackBg    = 0xff1a1408;
    static constexpr juce::uint32 kColourTrackFill  = 0xffd4860a;
    static constexpr juce::uint32 kColourText       = 0xfff0ede4;
    static constexpr juce::uint32 kColourTextDim    = 0xff9e9a8e;
    static constexpr juce::uint32 kColourLEDOn      = 0xffff8c00;
    static constexpr juce::uint32 kColourLEDOff     = 0xff3a2800;
    static constexpr juce::uint32 kColourButtonNorm = 0xff2e2a1e;
    static constexpr juce::uint32 kColourButtonOver = 0xff3e3a2e;
    static constexpr juce::uint32 kColourButtonDown = 0xff1a1610;
    static constexpr juce::uint32 kColourButtonAct  = 0xff4a3800;
    static constexpr juce::uint32 kColourDivider    = 0xff4a4030;

private:
    juce::Font bodyFont;
    juce::Font labelFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VintageLookAndFeel)
};
