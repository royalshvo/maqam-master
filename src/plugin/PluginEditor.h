#pragma once

// JUCE headers first (see PluginProcessor.h).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

class MaqamProcessor;

/** On-screen keyboard that shows the maqam: tonic, ghammaz, other maqam notes and the
    descending-form notes in their own colours, with note names on the maqam keys. */
class MaqamKeyboard final : public juce::MidiKeyboardComponent {
public:
    MaqamKeyboard(MaqamProcessor& processor, juce::MidiKeyboardState& state);

    void setGhammazDegree(int degree) { ghammaz_ = degree; repaint(); }

protected:
    void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour noteFillColour) override;
    juce::String getWhiteNoteText(int note) override;

private:
    juce::Colour colourFor(int note) const;
    /** Mute and Snap dim the keys outside the maqam. */
    void dimIfOutside(int note, juce::Graphics& g, juce::Rectangle<float> area) const;

    MaqamProcessor& processor_;
    std::atomic<float>* limitParam_ = nullptr;
    int ghammaz_ = 0;
};

/** Phase 2 editor: maqam and tonic pickers, sound controls, the scale summary and the
    illuminated keyboard. The full Explorer / Play Mode UI is Phase 3. */
class MaqamEditor final : public juce::AudioProcessorEditor, private juce::ChangeListener {
public:
    explicit MaqamEditor(MaqamProcessor&);
    ~MaqamEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void showSelectedMaqam();

    struct Knob {
        juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    MaqamProcessor& processor_;
    juce::ComboBox maqamBox_;
    juce::ComboBox tonicBox_;
    juce::Label tonicLabel_;
    struct Choice {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::vector<std::unique_ptr<Choice>> choices_;  // Sound, Keys (note limit), Voice mode, Drone octave
    juce::ToggleButton droneButton_{"Drone"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> droneAttachment_;
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::vector<std::unique_ptr<Knob>> steppers_;  // Octave, Transpose (- / + buttons)
    juce::Label summary_;
    juce::Label draftWarning_;
    MaqamKeyboard keyboard_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqamEditor)
};
