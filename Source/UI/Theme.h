#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace amp::ui
{
    struct Palette
    {
        juce::Colour bg, panel, panelAlt, text, textDim, accent, accent2, good, warn, bad, grid, meterBg;
    };

    inline bool& isLightTheme() { static bool light = false; return light; }

    inline const Palette& palette()
    {
        static const Palette dark { juce::Colour (0xff0e1014), juce::Colour (0xff171a21), juce::Colour (0xff1f242d),
                                    juce::Colour (0xffe8ecf2), juce::Colour (0xff8a93a0), juce::Colour (0xff3ea6ff),
                                    juce::Colour (0xffff9f43), juce::Colour (0xff3ddc84), juce::Colour (0xffffc233),
                                    juce::Colour (0xffff4d5e), juce::Colour (0xff2a303b), juce::Colour (0xff0b0d11) };
        static const Palette light { juce::Colour (0xffeef0f4), juce::Colour (0xffffffff), juce::Colour (0xffe3e6ec),
                                     juce::Colour (0xff1b1f27), juce::Colour (0xff5b6472), juce::Colour (0xff0a7cff),
                                     juce::Colour (0xffe07b00), juce::Colour (0xff1b9e55), juce::Colour (0xffd69a00),
                                     juce::Colour (0xffd62839), juce::Colour (0xffcfd5de), juce::Colour (0xffdfe3ea) };
        return isLightTheme() ? light : dark;
    }

    inline juce::Font fnt (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
    {
        const auto& p = palette();
        g.setColour (p.panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (p.grid);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (p.textDim);
        g.setFont (fnt (12.0f, true));
        g.drawText (title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, juce::Justification::centredLeft);
    }

    class AmpLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        AmpLookAndFeel() { applyPalette(); }

        void applyPalette()
        {
            const auto& p = palette();
            setColour (juce::ResizableWindow::backgroundColourId, p.bg);
            setColour (juce::Label::textColourId, p.text);
            setColour (juce::Slider::textBoxTextColourId, p.text);
            setColour (juce::Slider::textBoxOutlineColourId, p.grid);
            setColour (juce::Slider::textBoxBackgroundColourId, p.panelAlt);
            setColour (juce::Slider::rotarySliderFillColourId, p.accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, p.grid);
            setColour (juce::Slider::thumbColourId, p.text);
            setColour (juce::Slider::trackColourId, p.accent);
            setColour (juce::Slider::backgroundColourId, p.meterBg);
            setColour (juce::TextButton::buttonColourId, p.panelAlt);
            setColour (juce::TextButton::buttonOnColourId, p.accent);
            setColour (juce::TextButton::textColourOffId, p.text);
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            setColour (juce::ComboBox::backgroundColourId, p.panelAlt);
            setColour (juce::ComboBox::textColourId, p.text);
            setColour (juce::ComboBox::outlineColourId, p.grid);
            setColour (juce::ComboBox::arrowColourId, p.textDim);
            setColour (juce::PopupMenu::backgroundColourId, p.panel);
            setColour (juce::PopupMenu::textColourId, p.text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::ToggleButton::textColourId, p.text);
            setColour (juce::ToggleButton::tickColourId, p.accent);
            setColour (juce::ListBox::backgroundColourId, p.panel);
            setColour (juce::ListBox::textColourId, p.text);
            setColour (juce::TextEditor::backgroundColourId, p.panelAlt);
            setColour (juce::TextEditor::textColourId, p.text);
            setColour (juce::TextEditor::outlineColourId, p.grid);
        }
    };
}
