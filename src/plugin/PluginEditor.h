#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class MaqamProcessor;

/** Placeholder editor: maqam picker, scale summary and the draft-data warning.
    The real Explorer / keyboard UI is Phase 3. */
class MaqamEditor final : public juce::AudioProcessorEditor {
public:
    explicit MaqamEditor(MaqamProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showSelectedMaqam();

    MaqamProcessor& processor_;
    juce::ComboBox maqamBox_;
    juce::Label summary_;
    juce::Label draftWarning_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqamEditor)
};
