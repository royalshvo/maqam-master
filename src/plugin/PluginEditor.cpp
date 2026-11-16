#include "PluginEditor.h"

#include <cmath>

#include "PluginProcessor.h"

#include "core/NoteNames.h"

namespace {

// Palette (PLAN.md): a dark instrument panel with strong, distinct note colours.
const juce::Colour kBackground{0xff12131b};
const juce::Colour kPanel{0xff1a1b26};
const juce::Colour kOutline{0xff2a2b3a};
const juce::Colour kText{0xffe8e6df};
const juce::Colour kDimText{0xffa9a8b8};
const juce::Colour kIvory{0xfff2efe6};
const juce::Colour kTonicColour{0xffe0a93b};       // gold
const juce::Colour kGhammazColour{0xff5fd068};     // green
const juce::Colour kLowerJinsColour{0xff3fb8af};   // teal
const juce::Colour kUpperJinsColour{0xff5b8def};   // lapis blue
const juce::Colour kDescendingColour{0xffe0709c};  // rose, drawn hollow
const juce::Colour kQuarterToneColour{0xffff8a6a}; // coral cents badge

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
    setBlackNoteLengthProportion(0.62f);
    setColour(whiteNoteColourId, kIvory);
    setColour(blackNoteColourId, kBackground);
    setColour(keySeparatorLineColourId, juce::Colour(0x55000000));
    setColour(textLabelColourId, kBackground);
    setColour(keyDownOverlayColourId, juce::Colour(0x40ffffff));  // the cap itself brightens
    setColour(mouseOverKeyOverlayColourId, juce::Colour(0x18000000));
    limitParam_ = processor.parameters.getRawParameterValue("limit");
}

void MaqamKeyboard::dimIfOutside(int note, juce::Graphics& g, juce::Rectangle<float> area) const {
    if (juce::roundToInt(limitParam_->load()) == 0 || infoFor(processor_, note).degree != 0)
        return;
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(area);
}

MaqamKeyboard::KeyStyle MaqamKeyboard::styleFor(int note) const {
    const auto& ki = infoFor(processor_, note);
    if (ki.degree == 0)
        return {};
    if (ki.descending)
        return {kDescendingColour, true};
    if (ki.degree == 1)
        return {kTonicColour};
    if (ki.degree == ghammaz_)
        return {kGhammazColour};
    return {upperJins_ > 1 && ki.degree >= upperJins_ ? kUpperJinsColour : kLowerJinsColour};
}

// The coloured cap on a maqam key, with a cents badge when the note is not the normal key pitch.
void MaqamKeyboard::drawMarker(int note, juce::Graphics& g, juce::Rectangle<float> cap, bool isDown) const {
    const auto style = styleFor(note);
    if (style.colour.isTransparent())
        return;
    const auto colour = isDown ? style.colour.brighter(0.35f) : style.colour;
    if (style.hollow) {
        g.setColour(colour.withAlpha(0.22f));
        g.fillRoundedRectangle(cap, 3.0f);
        g.setColour(colour);
        g.drawRoundedRectangle(cap.reduced(1.0f), 3.0f, 2.0f);
    } else {
        g.setGradientFill(juce::ColourGradient(colour.brighter(0.15f), cap.getX(), cap.getY(), colour.darker(0.12f),
                                               cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 3.0f);
    }
    const int cents = infoFor(processor_, note).centsOff;
    if (std::abs(cents) >= 5) {
        auto badge = cap.removeFromTop(13.0f).reduced(1.5f, 1.0f);
        g.setColour(kQuarterToneColour);
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(kBackground);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText((cents > 0 ? "+" : "") + juce::String(cents), badge, juce::Justification::centred, false);
    }
}

void MaqamKeyboard::drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour lineColour, juce::Colour textColour) {
    // The cap sits in the part of the key below the black keys, so nothing hides it.
    drawMarker(note, g, area.withTrimmedTop(area.getHeight() * 0.64f).reduced(2.0f, 0.0f).withTrimmedBottom(3.0f), isDown);
    juce::MidiKeyboardComponent::drawWhiteNote(note, g, area, isDown, isOver, lineColour, textColour);
    dimIfOutside(note, g, area);
}

void MaqamKeyboard::drawBlackNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour noteFillColour) {
    juce::MidiKeyboardComponent::drawBlackNote(note, g, area, isDown, isOver, noteFillColour);
    dimIfOutside(note, g, area);
    if (styleFor(note).colour.isTransparent())
        return;
    const auto band = area.reduced(area.getWidth() * 0.1f, 0.0f).withTrimmedTop(area.getHeight() * 0.3f).withTrimmedBottom(3.0f);
    drawMarker(note, g, band, isDown);
    g.setColour(styleFor(note).hollow ? kIvory : kBackground);
    g.setFont(juce::FontOptions(juce::jmin(10.5f, area.getWidth() * 0.78f), juce::Font::bold).withHorizontalScale(0.78f));
    g.drawText(infoFor(processor_, note).name, band.withTrimmedBottom(2.0f), juce::Justification::centredBottom, false);
}

juce::String MaqamKeyboard::getWhiteNoteText(int note) {
    const auto& ki = infoFor(processor_, note);
    return ki.degree != 0 ? ki.name : juce::String();
}

// ---------------------------------------------------------------- qanun

MaqamQanun::MaqamQanun(MaqamProcessor& processor, juce::MidiKeyboardState& state)
    : processor_(processor), state_(state) {
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(30);
}

void MaqamQanun::setStrings(std::vector<String> strings) {
    pluck(-1);
    strings_ = std::move(strings);
    lit_.assign(strings_.size(), false);
    repaint();
}

// Geometry: a right-angled trapezoid, slanted on the left like the instrument, with the bridge
// and the note names on the right.
float MaqamQanun::stringY(int index) const {
    const float t = (static_cast<float>(index) + 0.8f) / (static_cast<float>(strings_.size()) + 0.6f);
    return static_cast<float>(getHeight()) - 2.0f - t * (static_cast<float>(getHeight()) - 4.0f);
}

float MaqamQanun::stringLeft(int index) const {
    const float t = (static_cast<float>(getHeight()) - stringY(index)) / static_cast<float>(getHeight());
    return 2.0f + static_cast<float>(getWidth()) * 0.36f * t + 12.0f;
}

float MaqamQanun::bridgeX() const {
    return static_cast<float>(getWidth()) - 52.0f;
}

int MaqamQanun::noteFor(int index) const {
    if (index < 0 || index >= static_cast<int>(strings_.size()))
        return -1;
    const int note = strings_[static_cast<size_t>(index)].key - processor_.keyShift();
    return note >= 0 && note < 128 ? note : -1;
}

int MaqamQanun::stringAt(juce::Point<float> position) const {
    if (strings_.size() < 2)
        return strings_.empty() ? -1 : 0;
    const float spacing = stringY(0) - stringY(1);
    int best = -1;
    float bestDistance = spacing * 0.5f;
    for (int i = 0; i < static_cast<int>(strings_.size()); ++i) {
        const float d = std::abs(position.y - stringY(i));
        if (d <= bestDistance && position.x >= stringLeft(i) - 14.0f) {
            bestDistance = d;
            best = i;
        }
    }
    return best;
}

void MaqamQanun::pluck(int index) {
    if (index == held_)
        return;
    if (heldNote_ >= 0)
        state_.noteOff(1, heldNote_, 0.0f);
    held_ = index;
    heldNote_ = noteFor(index);
    if (heldNote_ >= 0)
        state_.noteOn(1, heldNote_, 0.8f);
}

void MaqamQanun::mouseDown(const juce::MouseEvent& e) {
    pluck(stringAt(e.position));
}

void MaqamQanun::mouseDrag(const juce::MouseEvent& e) {
    const int index = stringAt(e.position);
    if (index >= 0)
        pluck(index);  // strumming: each string sounds as the pointer crosses it
}

void MaqamQanun::mouseUp(const juce::MouseEvent&) {
    pluck(-1);
}

void MaqamQanun::timerCallback() {
    bool changed = false;
    for (size_t i = 0; i < strings_.size(); ++i) {
        const int note = noteFor(static_cast<int>(i));
        const bool on = note >= 0 && state_.isNoteOnForChannels(0xffff, note);
        if (on != lit_[i]) {
            lit_[i] = on;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void MaqamQanun::paint(juce::Graphics& g) {
    const auto a = getLocalBounds().toFloat().reduced(2.0f);
    juce::Path body;
    body.startNewSubPath(a.getX(), a.getBottom());
    body.lineTo(a.getRight(), a.getBottom());
    body.lineTo(a.getRight(), a.getY());
    body.lineTo(a.getX() + a.getWidth() * 0.36f, a.getY());
    body.closeSubPath();
    g.setColour(kPanel.withAlpha(0.94f));
    g.fillPath(body);
    g.setColour(kTonicColour.withAlpha(0.5f));
    g.strokePath(body, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

    g.setColour(kOutline.brighter(0.3f));
    g.fillRoundedRectangle(bridgeX() - 3.0f, a.getY() + 8.0f, 6.0f, a.getHeight() - 16.0f, 3.0f);

    for (int i = 0; i < static_cast<int>(strings_.size()); ++i) {
        const auto& string = strings_[static_cast<size_t>(i)];
        const bool lit = lit_[static_cast<size_t>(i)];
        const float y = stringY(i), left = stringLeft(i);
        if (lit) {
            g.setColour(string.colour.withAlpha(0.28f));
            g.drawLine(left, y, bridgeX(), y, 9.0f);
        }
        g.setColour(lit ? string.colour.brighter(0.5f) : string.colour);
        g.drawLine(left, y, bridgeX(), y, lit ? 3.2f : 2.2f);
        g.fillEllipse(left - 4.0f, y - 4.0f, 8.0f, 8.0f);  // the peg
        g.setColour(lit ? kIvory : kText.withAlpha(0.85f));
        g.setFont(juce::FontOptions(13.0f, lit ? juce::Font::bold : juce::Font::plain));
        g.drawText(string.name, juce::Rectangle<float>(bridgeX() + 8.0f, y - 9.0f, 42.0f, 18.0f),
                   juce::Justification::centredLeft, false);
    }
}

// ---------------------------------------------------------------- info rows

namespace {
void drawInfoIcon(juce::Graphics& g, MaqamInfo::Icon icon, juce::Rectangle<float> box, juce::Colour colour,
                  juce::Colour second) {
    using Icon = MaqamInfo::Icon;
    const auto b = box.withSizeKeepingCentre(16.0f, 16.0f);
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
    const juce::PathStrokeType stroke(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(colour);
    juce::Path path;
    switch (icon) {
    case Icon::Up:
    case Icon::Down: {  // steps with an arrow head, climbing or falling
        const bool up = icon == Icon::Up;
        auto py = [&](float t) { return up ? y + h * (1.0f - t) : y + h * t; };
        path.startNewSubPath(x, py(0.1f));
        path.lineTo(x + w * 0.35f, py(0.1f));
        path.lineTo(x + w * 0.35f, py(0.5f));
        path.lineTo(x + w * 0.7f, py(0.5f));
        path.lineTo(x + w * 0.7f, py(0.92f));
        path.startNewSubPath(x + w * 0.45f, py(0.68f));
        path.lineTo(x + w * 0.7f, py(0.95f));
        path.lineTo(x + w * 0.95f, py(0.68f));
        g.strokePath(path, stroke);
        break;
    }
    case Icon::Home:  // a house
        path.startNewSubPath(x, y + h * 0.5f);
        path.lineTo(x + w * 0.5f, y + h * 0.05f);
        path.lineTo(x + w, y + h * 0.5f);
        path.startNewSubPath(x + w * 0.16f, y + h * 0.42f);
        path.lineTo(x + w * 0.16f, y + h * 0.95f);
        path.lineTo(x + w * 0.84f, y + h * 0.95f);
        path.lineTo(x + w * 0.84f, y + h * 0.42f);
        g.strokePath(path, stroke);
        break;
    case Icon::Rest:  // a flag: a place to stop on the way
        path.startNewSubPath(x + w * 0.2f, y + h);
        path.lineTo(x + w * 0.2f, y);
        g.strokePath(path, stroke);
        path.clear();
        path.addTriangle(x + w * 0.2f, y + h * 0.05f, x + w * 0.95f, y + h * 0.3f, x + w * 0.2f, y + h * 0.55f);
        g.fillPath(path);
        break;
    case Icon::Families:  // two overlapping blocks, in the two families' colours
        g.fillRoundedRectangle(x, y + h * 0.3f, w * 0.62f, h * 0.62f, 2.5f);
        g.setColour(second.isTransparent() ? colour : second);
        g.fillRoundedRectangle(x + w * 0.38f, y + h * 0.05f, w * 0.62f, h * 0.62f, 2.5f);
        break;
    case Icon::QuarterTone: {  // the keyboard's cents badge
        const auto pill = box.withSizeKeepingCentre(24.0f, 13.0f);
        g.fillRoundedRectangle(pill, 3.0f);
        g.setColour(kBackground);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText("-50", pill, juce::Justification::centred, false);
        break;
    }
    case Icon::Tip:  // a light bulb
        g.drawEllipse(x + w * 0.2f, y, w * 0.6f, h * 0.6f, 1.8f);
        g.fillRoundedRectangle(x + w * 0.36f, y + h * 0.7f, w * 0.28f, h * 0.28f, 1.5f);
        break;
    }
}
} // namespace

void MaqamInfo::paint(juce::Graphics& g) {
    if (rows_.empty())
        return;
    const float rowHeight = juce::jmin(26.0f, static_cast<float>(getHeight()) / static_cast<float>(rows_.size()));
    for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        auto area = juce::Rectangle<float>(0.0f, rowHeight * static_cast<float>(i), static_cast<float>(getWidth()), rowHeight);
        drawInfoIcon(g, row.icon, area.removeFromLeft(30.0f), row.colour, row.second);
        g.setFont(juce::FontOptions(14.5f));
        g.setColour(kDimText);
        g.drawText(row.label, area.removeFromLeft(108.0f), juce::Justification::centredLeft, false);
        g.setColour(row.icon == Icon::Tip ? kDimText : kText);
        g.drawFittedText(row.value, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }
}

// ---------------------------------------------------------------- legend

void MaqamLegend::paint(juce::Graphics& g) {
    if (items_.empty())
        return;
    const float cell = static_cast<float>(getWidth()) / static_cast<float>(items_.size());
    g.setFont(juce::FontOptions(12.5f));
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& item = items_[i];
        auto area = juce::Rectangle<float>(cell * static_cast<float>(i), 0.0f, cell, static_cast<float>(getHeight())).reduced(4.0f, 2.0f);
        auto swatch = area.removeFromLeft(item.badge ? 26.0f : 15.0f).withSizeKeepingCentre(item.badge ? 26.0f : 15.0f, item.badge ? 13.0f : 15.0f);
        if (item.hollow) {
            g.setColour(item.colour.withAlpha(0.22f));
            g.fillRoundedRectangle(swatch, 3.0f);
            g.setColour(item.colour);
            g.drawRoundedRectangle(swatch.reduced(1.0f), 3.0f, 2.0f);
        } else {
            g.setColour(item.colour);
            g.fillRoundedRectangle(swatch, 3.0f);
        }
        if (item.badge) {
            g.setColour(kBackground);
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText("-50", swatch, juce::Justification::centred, false);
            g.setFont(juce::FontOptions(12.5f));
        }
        area.removeFromLeft(6.0f);
        g.setColour(kText);
        g.drawFittedText(item.text, area.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
    }
}

// ---------------------------------------------------------------- editor

MaqamEditor::MaqamEditor(MaqamProcessor& p)
    : AudioProcessorEditor(&p), processor_(p), qanun_(p, p.keyboardState), keyboard_(p, p.keyboardState) {
    // windowBackground, widgetBackground, menuBackground, outline, defaultText, defaultFill,
    // highlightedText, highlightedFill, menuText
    look_.setColourScheme({kBackground, kPanel, kPanel, kOutline, kText, kTonicColour, kBackground, kTonicColour, kText});
    look_.setColour(juce::Slider::thumbColourId, kIvory);
    look_.setColour(juce::Slider::rotarySliderOutlineColourId, kOutline);
    look_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    look_.setColour(juce::ComboBox::arrowColourId, kTonicColour);
    look_.setColour(juce::ToggleButton::tickColourId, kTonicColour);
    look_.setColour(juce::TextButton::buttonColourId, kOutline);
    setLookAndFeel(&look_);

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

    variantBox_.onChange = [this] {
        const maqam::Maqam* m = processor_.library().findMaqam(processor_.selectedMaqamId());
        const int index = variantBox_.getSelectedItemIndex();  // 0 = main form
        if (m == nullptr || index < 0)
            return;
        processor_.setVariant(index == 0 ? std::string() : m->ascendingVariants[static_cast<size_t>(index - 1)].id);
    };
    addAndMakeVisible(variantBox_);

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
                                        std::pair{"voiceMode", "Mode"}, std::pair{"bendRange", "Bend"},
                                        std::pair{"droneOctave", "Drone"}}) {
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
          std::pair{"chorus", "Chorus"}, std::pair{"reverb", "Reverb"}, std::pair{"volume", "Volume"}}) {
        auto knob = std::make_unique<Knob>();
        knob->paramId = paramId;
        knob->slider.setColour(juce::Slider::trackColourId, kTonicColour.withAlpha(0.55f));
        knob->slider.setColour(juce::Slider::backgroundColourId, kBackground);
        knob->slider.setColour(juce::Slider::textBoxTextColourId, kIvory);
        knob->slider.setColour(juce::Slider::textBoxOutlineColourId, kOutline);
        knob->slider.setMouseDragSensitivity(220);
        knob->slider.setTooltip("Click or drag to set. Double-click to reset.");
        knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor_.parameters, paramId, knob->slider);
        // Readable values: percent, seconds, Hz, dB.
        const juce::String id = paramId;
        knob->slider.textFromValueFunction = [id](double v) -> juce::String {
            if (id == "decay")
                return juce::String(v, 1) + " s";
            if (id == "glide")
                return juce::String(juce::roundToInt(v * 1000.0)) + " ms";
            if (id == "vibratoRate")
                return juce::String(v, 1) + " Hz";
            if (id == "volume")
                return juce::String(v, 1) + " dB";
            return juce::String(juce::roundToInt(v * 100.0)) + "%";
        };
        knob->slider.updateText();
        knob->label.setText(name, juce::dontSendNotification);
        knob->label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(knob->slider);
        addAndMakeVisible(knob->label);
        knobs_.push_back(std::move(knob));
    }

    for (const auto& [knobParam, syncParam, noteParam] :
         {std::tuple{"glide", "glideSync", "glideNote"}, std::tuple{"vibratoRate", "vibratoSync", "vibratoNote"}}) {
        auto sync = std::make_unique<Sync>();
        for (auto& k : knobs_)
            if (k->paramId == knobParam)
                sync->knob = k.get();
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters.getParameter(noteParam)))
            sync->box.addItemList(param->choices, 1);
        sync->boxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor_.parameters, noteParam, sync->box);
        sync->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor_.parameters, syncParam, sync->button);
        sync->button.onStateChange = [this] { updateSyncControls(); };
        addAndMakeVisible(sync->button);
        addChildComponent(sync->box);
        syncs_.push_back(std::move(sync));
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

    title_.setFont(juce::FontOptions(24.0f));
    title_.setColour(juce::Label::textColourId, kIvory);
    addAndMakeVisible(title_);
    mood_.setFont(juce::FontOptions(15.0f, juce::Font::italic));
    mood_.setColour(juce::Label::textColourId, kTonicColour);
    addAndMakeVisible(mood_);
    addAndMakeVisible(info_);
    addAndMakeVisible(qanun_);

    draftWarning_.setColour(juce::Label::textColourId, kQuarterToneColour);
    draftWarning_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(draftWarning_);

    addAndMakeVisible(legend_);
    addAndMakeVisible(keyboard_);

    processor_.addChangeListener(this);
    showSelectedMaqam();
    updateSyncControls();
    sendLookAndFeelChange();  // children made before they had a parent pick up the palette now
    setSize(900, 700);
}

MaqamEditor::~MaqamEditor() {
    setLookAndFeel(nullptr);
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
        title_.setText("Could not load maqam data", juce::dontSendNotification);
        mood_.setText(processor_.loadError(), juce::dontSendNotification);
        draftWarning_.setText({}, juce::dontSendNotification);
        return;
    }
    const maqam::Maqam* m = lib.findMaqam(processor_.selectedMaqamId());
    if (m == nullptr)
        return;
    // The variant menu lists this maqam's forms.
    variantBox_.clear(juce::dontSendNotification);
    variantBox_.addItem("Main form", 1);
    int selectedVariant = 0;
    for (size_t i = 0; i < m->ascendingVariants.size(); ++i) {
        variantBox_.addItem(m->ascendingVariants[i].name, static_cast<int>(i) + 2);
        if (m->ascendingVariants[i].id == processor_.selectedVariantId())
            selectedVariant = static_cast<int>(i) + 1;
    }
    variantBox_.setSelectedItemIndex(selectedVariant, juce::dontSendNotification);
    variantBox_.setEnabled(!m->ascendingVariants.empty());
    keyboard_.setGhammazDegree(m->ghammazDegree);
    keyboard_.repaint();

    const auto& tonic = processor_.tonic();
    auto noteList = [&](const maqam::ResolvedScale& s) {
        juce::String out;
        for (const auto& n : maqam::spellScale(s, tonic))
            out << juce::String::fromUTF8(n.compact().c_str()) << "   ";
        return out;
    };

    const auto scale = maqam::resolveScale(*m, maqam::Direction::Ascending, processor_.selectedVariantId());
    const auto names = maqam::spellScale(*scale, tonic);
    auto noteName = [&](int degree) {
        const auto index = static_cast<size_t>(juce::jlimit(1, static_cast<int>(names.size()), degree) - 1);
        return juce::String::fromUTF8(names[index].compact().c_str());
    };
    auto jinsLabel = [&](const maqam::JinsPlacement& j) {
        const auto* def = lib.findJins(j.jinsId);
        return def ? juce::String(def->name) : juce::String(j.jinsId);
    };
    const juce::String halfFlat = juce::String::fromUTF8("\xc2\xbd");  // the 1/2 sign in half-flat names

    // The name, large, then how it feels.
    juce::String title = juce::String(m->name);
    if (const auto* variant = m->findVariant(processor_.selectedVariantId()))
        title << " " << juce::String(variant->name).upToFirstOccurrenceOf(" (", false, false);
    title << "   " << juce::String::fromUTF8(m->nameAr.c_str());
    title_.setText(title, juce::dontSendNotification);
    mood_.setText(juce::String(m->mood), juce::dontSendNotification);

    // Then the notes and how it is built, in plain words: no cents, MIDI numbers or Hz. Each
    // row's icon uses the colour the same thing has on the keyboard.
    using Icon = MaqamInfo::Icon;
    std::vector<MaqamInfo::Row> rows;
    rows.push_back({Icon::Up, kIvory, "Going up", noteList(*scale)});
    if (const auto down = maqam::resolveScale(*m, maqam::Direction::Descending)) {
        const juce::String downNotes = noteList(*down);
        rows.push_back({Icon::Down, kDescendingColour, "Coming down",
                        downNotes == noteList(*scale) ? juce::String("the same notes") : downNotes});
    }
    rows.push_back({Icon::Home, kTonicColour, "Home note", noteName(1)});
    rows.push_back({Icon::Rest, kGhammazColour, "Resting point", noteName(m->ghammazDegree)});
    if (scale->ajnas.size() > 1)
        rows.push_back({Icon::Families, kLowerJinsColour, "Note families",
                        jinsLabel(scale->ajnas[0]) + " from " + noteName(scale->ajnas[0].rootDegree) + ",  then " +
                            jinsLabel(scale->ajnas[1]) + " from " + noteName(scale->ajnas[1].rootDegree),
                        kUpperJinsColour});
    juce::StringArray quarterTones;
    for (int d = 1; d < static_cast<int>(names.size()); ++d)
        if (noteName(d).contains(halfFlat))
            quarterTones.addIfNotAlreadyThere(noteName(d));
    rows.push_back({Icon::QuarterTone, kQuarterToneColour, quarterTones.size() > 1 ? "Quarter tones" : "Quarter tone",
                    quarterTones.isEmpty() ? juce::String("none: every note is a normal piano key")
                                           : quarterTones.joinIntoString(", ") + "   (between two piano keys)"});
    rows.push_back({Icon::Tip, kDimText, "Tip", "Set Keys to Snap: every key plays the maqam"});

    // Keyboard colours: the upper jins starts on the second jins' root, and the legend names both.
    const int upperDegree = scale->ajnas.size() > 1 ? scale->ajnas[1].rootDegree : 0;
    keyboard_.setUpperJinsDegree(upperDegree);
    auto jinsName = [&](size_t index) {
        if (index >= scale->ajnas.size())
            return juce::String();
        const auto* def = lib.findJins(scale->ajnas[index].jinsId);
        return def ? juce::String(def->name) : juce::String(scale->ajnas[index].jinsId);
    };
    std::vector<MaqamLegend::Item> legend;
    legend.push_back({kTonicColour, "Tonic"});
    legend.push_back({kLowerJinsColour, "Lower jins: " + jinsName(0)});
    legend.push_back({kGhammazColour, "Ghammaz"});
    if (upperDegree > 1)
        legend.push_back({kUpperJinsColour, "Upper jins: " + jinsName(1)});
    legend.push_back({kDescendingColour, "Only on the way down", true});
    legend.push_back({kQuarterToneColour, "Cents away from the normal key", false, true});
    legend_.setItems(std::move(legend));

    // The qanun: one string per maqam note from the tonic up to its octave, lowest first.
    {
        std::vector<MaqamQanun::String> strings;
        const auto& keys = processor_.keyInfo();
        const int tonicKey = tonic.midiNote;
        for (int k = tonicKey; k <= tonicKey + 12 && k < maqam::synth::kNumKeys; ++k) {
            const auto& ki = keys[static_cast<size_t>(k)];
            if (ki.degree == 0 || ki.descending)
                continue;
            const auto colour = ki.degree == 1 ? kTonicColour
                                : ki.degree == m->ghammazDegree ? kGhammazColour
                                : upperDegree > 1 && ki.degree >= upperDegree ? kUpperJinsColour
                                : kLowerJinsColour;
            strings.push_back({colour, ki.name, k});
        }
        qanun_.setStrings(std::move(strings));
    }
    info_.setRows(std::move(rows));

    draftWarning_.setText(m->reviewed ? juce::String()
                                      : "Based on common descriptions of this maqam. Not yet checked by a teacher.",
                          juce::dontSendNotification);
}

void MaqamEditor::updateSyncControls() {
    for (auto& sync : syncs_) {
        const bool on = sync->button.getToggleState();
        sync->box.setVisible(on);
        if (sync->knob != nullptr)
            sync->knob->slider.setVisible(!on);
    }
}

void MaqamEditor::paint(juce::Graphics& g) {
    g.fillAll(kBackground);
    g.drawImageAt(pattern_, 0, 0);

    for (const auto& panel : {controlsPanel_, keysPanel_}) {
        g.setColour(kPanel);
        g.fillRoundedRectangle(panel.toFloat(), 8.0f);
        g.setColour(kOutline);
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 8.0f, 1.0f);
    }
}

void MaqamEditor::resized() {
    auto area = getLocalBounds().reduced(16);

    auto top = area.removeFromTop(28);
    maqamBox_.setBounds(top.removeFromLeft(190));
    top.removeFromLeft(6);
    variantBox_.setBounds(top.removeFromLeft(180));
    top.removeFromLeft(6);
    tonicLabel_.setBounds(top.removeFromLeft(50));
    tonicBox_.setBounds(top.removeFromLeft(90));
    for (auto& st : steppers_) {
        top.removeFromLeft(8);
        st->label.setBounds(top.removeFromLeft(st.get() == steppers_.front().get() ? 52 : 70));
        st->slider.setBounds(top.removeFromLeft(100));
    }
    area.removeFromTop(8);

    auto row = area.removeFromTop(26);
    const int widths[] = {80, 120, 80, 130, 100};
    for (size_t i = 0; i < choices_.size(); ++i) {
        choices_[i]->label.setBounds(row.removeFromLeft(48));
        choices_[i]->box.setBounds(row.removeFromLeft(widths[i]));
        row.removeFromLeft(i + 1 == choices_.size() ? 4 : 14);
    }
    droneButton_.setBounds(row.removeFromLeft(50));
    area.removeFromTop(10);

    keysPanel_ = area.removeFromBottom(182);
    auto keysArea = keysPanel_.reduced(8, 6);
    legend_.setBounds(keysArea.removeFromTop(26));
    keysArea.removeFromTop(4);
    keyboard_.setBounds(keysArea);
    keyboard_.setKeyWidth(static_cast<float>(keysArea.getWidth()) / 22.0f);  // 22 white keys in C3..C6
    area.removeFromBottom(6);
    draftWarning_.setBounds(area.removeFromBottom(20));

    controlsPanel_ = area.removeFromBottom(146);
    // Three rows of three bars; each has its name above it (and Sync beside the name).
    {
        auto grid = controlsPanel_.reduced(12, 8);
        const int columns = 3;
        const int rows = (static_cast<int>(knobs_.size()) + columns - 1) / columns;
        const int cellW = grid.getWidth() / columns, cellH = grid.getHeight() / rows;
        for (size_t i = 0; i < knobs_.size(); ++i) {
            auto& k = knobs_[i];
            auto cell = juce::Rectangle<int>(grid.getX() + static_cast<int>(i % columns) * cellW,
                                             grid.getY() + static_cast<int>(i / columns) * cellH, cellW, cellH)
                            .reduced(8, 2);
            auto labelRow = cell.removeFromTop(18);
            const auto bar = cell.withTrimmedBottom(juce::jmax(0, cell.getHeight() - 22));
            k->slider.setBounds(bar);
            for (auto& sync : syncs_)
                if (sync->knob == k.get()) {
                    sync->button.setBounds(labelRow.removeFromRight(70));
                    sync->box.setBounds(bar);
                }
            k->label.setBounds(labelRow);
        }
    }
    area.removeFromBottom(6);
    qanun_.setBounds(area.removeFromRight(juce::jmin(380, area.getWidth() * 45 / 100)).reduced(4, 4));
    title_.setBounds(area.removeFromTop(32));
    mood_.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);
    info_.setBounds(area);

    // Backdrop: a faint tiling of eight-pointed stars (two crossed squares), the classic
    // pattern of Arabic geometric art. Drawn once per size.
    pattern_ = juce::Image(juce::Image::ARGB, juce::jmax(1, getWidth()), juce::jmax(1, getHeight()), true);
    {
        juce::Graphics pg(pattern_);
        const float cell = 64.0f, r = 22.0f;
        juce::Path stars;
        for (float cy = 0.0f; cy < static_cast<float>(getHeight()) + cell; cy += cell)
            for (float cx = 0.0f; cx < static_cast<float>(getWidth()) + cell; cx += cell) {
                juce::Path square;
                square.addRectangle(-r, -r, 2.0f * r, 2.0f * r);
                stars.addPath(square, juce::AffineTransform::translation(cx, cy));
                stars.addPath(square, juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4.0f).translated(cx, cy));
                // A small diamond where four stars meet.
                juce::Path diamond;
                const float d = cell * 0.5f - r * 1.05f;
                diamond.addRectangle(-d, -d, 2.0f * d, 2.0f * d);
                stars.addPath(diamond, juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 4.0f)
                                           .translated(cx + cell * 0.5f, cy + cell * 0.5f));
            }
        pg.setColour(kTonicColour.withAlpha(0.12f));
        pg.strokePath(stars, juce::PathStrokeType(1.0f));
    }
}
