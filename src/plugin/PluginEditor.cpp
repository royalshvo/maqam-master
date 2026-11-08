#include "PluginEditor.h"

#include "PluginProcessor.h"

#include "core/NoteNames.h"

MaqamEditor::MaqamEditor(MaqamProcessor& p) : AudioProcessorEditor(&p), processor_(p) {
    const auto& lib = processor_.library();
    int id = 1;
    for (const auto& m : lib.maqamat)
        maqamBox_.addItem(juce::String(m.name) + "  " + juce::String::fromUTF8(m.nameAr.c_str()), id++);
    maqamBox_.onChange = [this] {
        const int index = maqamBox_.getSelectedItemIndex();
        const auto& maqamat = processor_.library().maqamat;
        if (index >= 0 && index < static_cast<int>(maqamat.size()))
            processor_.selectedMaqamId = maqamat[static_cast<size_t>(index)].id;
        showSelectedMaqam();
    };
    addAndMakeVisible(maqamBox_);

    summary_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(summary_);

    draftWarning_.setColour(juce::Label::textColourId, juce::Colours::orange);
    draftWarning_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(draftWarning_);

    for (size_t i = 0; i < lib.maqamat.size(); ++i)
        if (lib.maqamat[i].id == processor_.selectedMaqamId)
            maqamBox_.setSelectedItemIndex(static_cast<int>(i), juce::dontSendNotification);
    showSelectedMaqam();

    setSize(560, 300);
}

void MaqamEditor::showSelectedMaqam() {
    if (processor_.loadError().isNotEmpty()) {
        summary_.setText("Could not load maqam data:\n" + processor_.loadError(), juce::dontSendNotification);
        draftWarning_.setText({}, juce::dontSendNotification);
        return;
    }
    const maqam::Maqam* m = processor_.library().findMaqam(processor_.selectedMaqamId);
    if (m == nullptr)
        return;

    const auto& tonic = processor_.tonic;
    auto noteList = [&](const maqam::ResolvedScale& s) {
        juce::String out;
        for (const auto& n : maqam::spellScale(s, tonic))
            out << juce::String::fromUTF8(n.compact().c_str()) << "   ";
        return out;
    };

    const auto scale = maqam::resolveScale(*m, maqam::Direction::Ascending);
    juce::String text;
    text << "Ascending:   " << noteList(*scale);
    if (const auto down = maqam::resolveScale(*m, maqam::Direction::Descending))
        text << "\nDescending:  " << noteList(*down);
    else
        text << "\nDescending:  not yet specified";
    text << "\nCents from tonic: ";
    for (double c : scale->cents)
        text << juce::String(c, 0) << "  ";
    text << "\nAjnas: ";
    for (const auto& j : scale->ajnas) {
        const auto* def = processor_.library().findJins(j.jinsId);
        text << (def ? juce::String(def->name) : juce::String(j.jinsId)) << " on degree " << j.rootDegree << "   ";
    }
    text << "\nGhammaz: degree " << m->ghammazDegree;
    text << "\nTonic: " << juce::String::fromUTF8(maqam::spellTonic(tonic).compact(true).c_str())
         << "  (MIDI " << tonic.midiNote << ", " << juce::String(maqam::frequencyHz(tonic, 0.0), 2) << " Hz)";
    summary_.setText(text, juce::dontSendNotification);

    draftWarning_.setText(m->reviewed ? juce::String()
                                      : "DRAFT: this maqam has not yet been reviewed by a player or teacher.",
                          juce::dontSendNotification);
}

void MaqamEditor::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MaqamEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    maqamBox_.setBounds(area.removeFromTop(28));
    area.removeFromTop(12);
    draftWarning_.setBounds(area.removeFromBottom(24));
    summary_.setBounds(area);
}
