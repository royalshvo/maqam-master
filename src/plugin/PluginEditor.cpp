#include "PluginEditor.h"

#include <cmath>

#include "PluginProcessor.h"

#include "core/NoteNames.h"

namespace {

const juce::Colour kTonicColour{0xffe0b040};
const juce::Colour kGhammazColour{0xff4caf7a};
const juce::Colour kNoteColour{0xff5b8fd9};
const juce::Colour kDescendingColour{0xffb07ad9};

// Tonic choices: every quarter tone from C4 up to B4 (24 entries, 50 cents apart).
maqam::Tonic tonicForItem(int index) {
    return maqam::Tonic{60 + (index + 1) / 2, (index % 2) != 0 ? -50.0 : 0.0};
}

int itemForTonic(const maqam::Tonic& t) {
    const int quarterTones = static_cast<int>(std::lround((t.absoluteCents() - 6000.0) / 50.0));
    return ((quarterTones % 24) + 24) % 24;
}

// What a key of the on-screen keyboard plays: the info of the key it is shifted to by octave and
// transpose. Keys shifted off the 128 keys play nothing.
const MaqamProcessor::KeyInfo& infoFor(const MaqamProcessor& processor, int note) {
    static const MaqamProcessor::KeyInfo nothing{};
    const int k = note + processor.keyShift();
    if (k < 0 || k >= maqam::synth::kNumKeys)
        return nothing;
    return processor.keyInfo()[static_cast<size_t>(k)];
}

} // namespace

// ---------------------------------------------------------------- keyboard

MaqamKeyboard::MaqamKeyboard(MaqamProcessor& processor, juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard), processor_(processor) {
    setAvailableRange(48, 84);  // C3..C6
    setOctaveForMiddleC(4);
    setKeyPressBaseOctave(5);   // computer keys A, W, S... start at middle C
    setColour(keyDownOverlayColourId, juce::Colour(0xccff7040));
    setColour(mouseOverKeyOverlayColourId, juce::Colour(0x33ffffff));
    limitParam_ = processor.parameters.getRawParameterValue("limit");
}

void MaqamKeyboard::dimIfOutside(int note, juce::Graphics& g, juce::Rectangle<float> area) const {
    if (juce::roundToInt(limitParam_->load()) == 0 || infoFor(processor_, note).degree != 0)
        return;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(area);
}

juce::Colour MaqamKeyboard::colourFor(int note) const {
    const auto& ki = infoFor(processor_, note);
    if (ki.degree == 0)
        return juce::Colours::transparentBlack;
    if (ki.descending)
        return kDescendingColour;
    if (ki.degree == 1)
        return kTonicColour;
    if (ki.degree == ghammaz_)
        return kGhammazColour;
    return kNoteColour;
}

void MaqamKeyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour lineColour, juce::Colour textColour) {
    const auto c = colourFor(note);
    if (!c.isTransparent()) {
        g.setColour(c.withAlpha(0.55f));
        g.fillRect(area.withTrimmedTop(area.getHeight() * 0.55f));
    }
    juce::MidiKeyboardComponent::drawWhiteNote(note, g, area, isDown, isOver, lineColour, textColour);
    dimIfOutside(note, g, area);
}

void MaqamKeyboard::drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour noteFillColour) {
    juce::MidiKeyboardComponent::drawBlackNote(note, g, area, isDown, isOver, noteFillColour);
    dimIfOutside(note, g, area);
    const auto c = colourFor(note);
    if (c.isTransparent())
        return;
    auto band = area.reduced(area.getWidth() * 0.12f, 0.0f).withTrimmedTop(area.getHeight() * 0.45f).withTrimmedBottom(3.0f);
    g.setColour(c.withAlpha(isDown ? 0.6f : 0.9f));
    g.fillRoundedRectangle(band, 2.0f);
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(juce::jmin(10.0f, area.getWidth() * 0.75f)).withHorizontalScale(0.75f));
    g.drawText(infoFor(processor_, note).name, band, juce::Justification::centredBottom, false);
}

juce::String MaqamKeyboard::getWhiteNoteText(int note) {
    const auto& ki = infoFor(processor_, note);
    return ki.degree != 0 ? ki.name : juce::String();
}

// ---------------------------------------------------------------- editor

MaqamEditor::MaqamEditor(MaqamProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), keyboard_(p, p.keyboardState) {
    const auto& lib = processor_.library();
    int id = 1;
    for (const auto& m : lib.maqamat)
        maqamBox_.addItem(juce::String(m.name) + "  " + juce::String::fromUTF8(m.nameAr.c_str()), id++);
    maqamBox_.onChange = [this] {
        const int index = maqamBox_.getSelectedItemIndex();
        const auto& maqamat = processor_.library().maqamat;
        if (index >= 0 && index < static_cast<int>(maqamat.size()))
            processor_.setMaqam(maqamat[static_cast<size_t>(index)].id);
    };
    addAndMakeVisible(maqamBox_);

    for (int i = 0; i < 24; ++i)
        tonicBox_.addItem(juce::String::fromUTF8(maqam::spellTonic(tonicForItem(i)).compact(false).c_str()), i + 1);
    tonicBox_.onChange = [this] {
        const int index = tonicBox_.getSelectedItemIndex();
        if (index >= 0)
            processor_.setTonic(tonicForItem(index));
    };
    tonicLabel_.setText("Tonic", juce::dontSendNotification);
    tonicLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(tonicLabel_);
    addAndMakeVisible(tonicBox_);

    for (const auto& [paramId, name] : {std::pair{"sound", "Sound"}, std::pair{"limit", "Keys"},
                                        std::pair{"voiceMode", "Mode"}, std::pair{"droneOctave", "Drone"}}) {
        auto c = std::make_unique<Choice>();
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters.getParameter(paramId)))
            c->box.addItemList(param->choices, 1);
        c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor_.parameters, paramId, c->box);
        c->label.setText(name, juce::dontSendNotification);
        c->label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(c->box);
        addAndMakeVisible(c->label);
        choices_.push_back(std::move(c));
    }
    choices_[1]->box.onChange = [this] { keyboard_.repaint(); };  // dim or undim outside keys
    droneButton_.setButtonText("On");
    droneAttachment_ =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor_.parameters, "droneOn", droneButton_);
    addAndMakeVisible(droneButton_);

    for (const auto& [paramId, name] :
         {std::pair{"brightness", "Brightness"}, std::pair{"decay", "Decay"}, std::pair{"glide", "Glide"},
          std::pair{"vibrato", "Vibrato"}, std::pair{"vibratoRate", "Vib. rate"}, std::pair{"droneLevel", "Drone level"},
          std::pair{"reverb", "Reverb"}, std::pair{"volume", "Volume"}}) {
        auto knob = std::make_unique<Knob>();
        knob->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor_.parameters, paramId, knob->slider);
        knob->label.setText(name, juce::dontSendNotification);
        knob->label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knob->slider);
        addAndMakeVisible(knob->label);
        knobs_.push_back(std::move(knob));
    }

    for (const auto& [paramId, name] : {std::pair{"octave", "Octave"}, std::pair{"transpose", "Transpose"}}) {
        auto st = std::make_unique<Knob>();
        st->slider.setSliderStyle(juce::Slider::IncDecButtons);
        st->slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 36, 24);
        st->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor_.parameters, paramId, st->slider);
        st->slider.onValueChange = [this] { keyboard_.repaint(); };  // show what each key now plays
        st->label.setText(name, juce::dontSendNotification);
        st->label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(st->slider);
        addAndMakeVisible(st->label);
        steppers_.push_back(std::move(st));
    }

    summary_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(summary_);

    draftWarning_.setColour(juce::Label::textColourId, juce::Colours::orange);
    draftWarning_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(draftWarning_);

    addAndMakeVisible(keyboard_);

    processor_.addChangeListener(this);
    showSelectedMaqam();
    setSize(800, 560);
}

MaqamEditor::~MaqamEditor() {
    processor_.removeChangeListener(this);
}

void MaqamEditor::changeListenerCallback(juce::ChangeBroadcaster*) {
    showSelectedMaqam();
}

void MaqamEditor::showSelectedMaqam() {
    const auto& lib = processor_.library();
    for (size_t i = 0; i < lib.maqamat.size(); ++i)
        if (lib.maqamat[i].id == processor_.selectedMaqamId())
            maqamBox_.setSelectedItemIndex(static_cast<int>(i), juce::dontSendNotification);
    tonicBox_.setSelectedItemIndex(itemForTonic(processor_.tonic()), juce::dontSendNotification);

    if (processor_.loadError().isNotEmpty()) {
        summary_.setText("Could not load maqam data:\n" + processor_.loadError(), juce::dontSendNotification);
        draftWarning_.setText({}, juce::dontSendNotification);
        return;
    }
    const maqam::Maqam* m = lib.findMaqam(processor_.selectedMaqamId());
    if (m == nullptr)
        return;
    keyboard_.setGhammazDegree(m->ghammazDegree);
    keyboard_.repaint();

    const auto& tonic = processor_.tonic();
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
        const auto* def = lib.findJins(j.jinsId);
        text << (def ? juce::String(def->name) : juce::String(j.jinsId)) << " on degree " << j.rootDegree << "   ";
    }
    text << "\nGhammaz: degree " << m->ghammazDegree;
    text << "\nTonic: " << juce::String::fromUTF8(maqam::spellTonic(tonic).compact(true).c_str())
         << "  (MIDI " << tonic.midiNote << ", " << juce::String(maqam::frequencyHz(tonic, 0.0), 2) << " Hz)";
    text << "\n\nKeyboard: gold = tonic, green = ghammaz, blue = maqam note, purple = descending note."
            "\nOther keys play normal 12-TET, or are muted / snapped by Keys. Mod wheel adds vibrato."
            "\nOctave / Transpose move which key plays which note (the maqam itself stays put):"
            "\n  e.g. Transpose +5 puts the tonic F on a C pad of a Launchpad."
            "\nDrone: the tonic and its octave, always in tune; it follows maqam and tonic changes.";
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

    auto top = area.removeFromTop(28);
    maqamBox_.setBounds(top.removeFromLeft(260));
    top.removeFromLeft(12);
    tonicLabel_.setBounds(top.removeFromLeft(50));
    tonicBox_.setBounds(top.removeFromLeft(90));
    for (auto& st : steppers_) {
        top.removeFromLeft(8);
        st->label.setBounds(top.removeFromLeft(st.get() == steppers_.front().get() ? 52 : 70));
        st->slider.setBounds(top.removeFromLeft(100));
    }
    area.removeFromTop(8);

    auto row = area.removeFromTop(26);
    const int widths[] = {90, 140, 90, 110};
    for (size_t i = 0; i < choices_.size(); ++i) {
        choices_[i]->label.setBounds(row.removeFromLeft(48));
        choices_[i]->box.setBounds(row.removeFromLeft(widths[i]));
        row.removeFromLeft(i + 1 == choices_.size() ? 4 : 14);
    }
    droneButton_.setBounds(row.removeFromLeft(50));
    area.removeFromTop(10);

    auto keys = area.removeFromBottom(120);
    keyboard_.setBounds(keys);
    keyboard_.setKeyWidth(static_cast<float>(keys.getWidth()) / 22.0f);  // 22 white keys in C3..C6
    area.removeFromBottom(8);
    draftWarning_.setBounds(area.removeFromBottom(22));

    auto knobRow = area.removeFromBottom(112);
    const int knobWidth = knobRow.getWidth() / static_cast<int>(knobs_.size());
    for (auto& k : knobs_) {
        auto col = knobRow.removeFromLeft(knobWidth);
        k->label.setBounds(col.removeFromTop(18));
        k->slider.setBounds(col);
    }
    area.removeFromBottom(6);
    summary_.setBounds(area);
}
