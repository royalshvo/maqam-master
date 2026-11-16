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
    /** The degree the upper jins starts on (0 = none): notes from there up get its colour. */
    void setUpperJinsDegree(int degree) { upperJins_ = degree; repaint(); }

protected:
    void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                       juce::Colour noteFillColour) override;
    juce::String getWhiteNoteText(int note) override;

private:
    struct KeyStyle {
        juce::Colour colour;   // transparent: not a maqam note
        bool hollow = false;   // outline only: a note used on the way down
    };
    KeyStyle styleFor(int note) const;
    void drawMarker(int note, juce::Graphics& g, juce::Rectangle<float> cap, bool isDown) const;
    /** Mute and Snap dim the keys outside the maqam. */
    void dimIfOutside(int note, juce::Graphics& g, juce::Rectangle<float> area) const;

    MaqamProcessor& processor_;
    std::atomic<float>* limitParam_ = nullptr;
    int ghammaz_ = 0;
    int upperJins_ = 0;
};

/** A small playable qanun: one string per note of the maqam (lowest and longest at the bottom),
    in the keyboard's colours. Click a string to pluck it, drag across to strum; strings light
    up when their note is played from anywhere. */
class MaqamQanun final : public juce::Component, private juce::Timer {
public:
    MaqamQanun(MaqamProcessor& processor, juce::MidiKeyboardState& state);

    struct String {
        juce::Colour colour;
        juce::String name;
        int key = 0;  // the key whose pitch it plays (before octave / transpose)
    };
    void setStrings(std::vector<String> strings);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    int stringAt(juce::Point<float> position) const;  // -1: none
    void pluck(int index);                            // -1: let go
    int noteFor(int index) const;                     // the MIDI note to send, or -1
    float stringY(int index) const;
    float stringLeft(int index) const;
    float bridgeX() const;

    MaqamProcessor& processor_;
    juce::MidiKeyboardState& state_;
    std::vector<String> strings_;
    std::vector<bool> lit_;
    int held_ = -1, heldNote_ = -1;
};

/** The maqam in plain words: rows of icon, label and value. */
class MaqamInfo final : public juce::Component {
public:
    enum class Icon { Up, Down, Home, Rest, Families, QuarterTone, Tip };
    struct Row {
        Icon icon;
        juce::Colour colour;
        juce::String label, value;
        juce::Colour second{};  // Families: the second family's colour
    };
    void setRows(std::vector<Row> rows) {
        rows_ = std::move(rows);
        repaint();
    }
    void paint(juce::Graphics&) override;

private:
    std::vector<Row> rows_;
};

/** One row explaining the keyboard colours, with the current jins names in it. */
class MaqamLegend final : public juce::Component {
public:
    struct Item {
        juce::Colour colour;
        juce::String text;
        bool hollow = false;  // outline swatch (descending notes)
        bool badge = false;   // small pill swatch (the cents badge)
    };
    void setItems(std::vector<Item> items) {
        items_ = std::move(items);
        repaint();
    }
    void paint(juce::Graphics&) override;

private:
    std::vector<Item> items_;
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
    void updateSyncControls();

    struct Knob {
        // A bar: click anywhere to set it, drag sideways, scroll, or double-click to reset.
        juce::Slider slider{juce::Slider::LinearBar, juce::Slider::TextBoxLeft};
        juce::Label label;
        juce::String paramId;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    MaqamProcessor& processor_;
    juce::ComboBox maqamBox_;
    juce::ComboBox variantBox_;  // "Main form" plus the maqam's variants (e.g. Bayati Shuri)
    juce::ComboBox tonicBox_;
    juce::Label tonicLabel_;
    struct Choice {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::vector<std::unique_ptr<Choice>> choices_;  // Sound, Keys (note limit), Voice mode, Bend, Drone octave
    juce::ToggleButton droneButton_{"Drone"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> droneAttachment_;
    std::vector<std::unique_ptr<Knob>> knobs_;
    std::vector<std::unique_ptr<Knob>> steppers_;  // Octave, Transpose (- / + buttons)
    // Tempo sync under the Glide and Vib. rate knobs: when on, a note-value menu replaces the knob.
    struct Sync {
        Knob* knob = nullptr;
        juce::ToggleButton button{"Sync"};
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachment;
    };
    std::vector<std::unique_ptr<Sync>> syncs_;
    juce::Label title_;    // the maqam's name, large
    juce::Label mood_;     // one sentence on how it feels
    MaqamInfo info_;       // the notes and how the maqam is built, in plain words
    MaqamQanun qanun_;
    juce::Label draftWarning_;
    MaqamLegend legend_;
    MaqamKeyboard keyboard_;
    juce::Rectangle<int> controlsPanel_, keysPanel_;  // painted backdrops
    juce::Image pattern_;                             // star-pattern backdrop, drawn once per size
    juce::LookAndFeel_V4 look_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqamEditor)
};
