#include "PluginProcessor.h"

#include <BinaryData.h>

#include "PluginEditor.h"

#include "core/NoteNames.h"

using maqam::synth::kNumKeys;

namespace {
const juce::Identifier kStateType{"MaqamMasterState"};
}

juce::AudioProcessorValueTreeState::ParameterLayout MaqamProcessor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"sound", 1}, "Sound",
                                                      StringArray{"Pluck", "Synth", "Ney"}, 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"brightness", 1}, "Brightness",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{"decay", 1}, "Decay", NormalisableRange<float>(0.2f, 10.0f, 0.0f, 0.5f), 3.0f,
        AudioParameterFloatAttributes().withLabel("s")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"volume", 1}, "Volume",
                                                     NormalisableRange<float>(-36.0f, 6.0f), -6.0f,
                                                     AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"reverb", 1}, "Reverb",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"limit", 1}, "Note limit",
                                                      StringArray{"All keys", "Mute outside", "Snap to maqam"}, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"voiceMode", 1}, "Voice mode",
                                                      StringArray{"Poly", "Mono", "Legato"}, 0));
    layout.add(std::make_unique<AudioParameterFloat>(
        ParameterID{"glide", 1}, "Glide", NormalisableRange<float>(0.0f, 1.0f, 0.0f, 0.5f), 0.12f,
        AudioParameterFloatAttributes().withLabel("s")));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"vibrato", 1}, "Vibrato",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"vibratoRate", 1}, "Vibrato rate",
                                                     NormalisableRange<float>(3.0f, 8.0f), 5.5f,
                                                     AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{"droneOn", 1}, "Drone", false));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{"droneLevel", 1}, "Drone level",
                                                     NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"droneOctave", 1}, "Drone octave",
                                                      StringArray{"2 oct. down", "1 oct. down", "Tonic"}, 1));
    return layout;
}

MaqamProcessor::MaqamProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, kStateType, createLayout()) {
    soundParam_ = parameters.getRawParameterValue("sound");
    brightnessParam_ = parameters.getRawParameterValue("brightness");
    decayParam_ = parameters.getRawParameterValue("decay");
    volumeParam_ = parameters.getRawParameterValue("volume");
    reverbParam_ = parameters.getRawParameterValue("reverb");
    limitParam_ = parameters.getRawParameterValue("limit");
    modeParam_ = parameters.getRawParameterValue("voiceMode");
    glideParam_ = parameters.getRawParameterValue("glide");
    vibratoParam_ = parameters.getRawParameterValue("vibrato");
    vibratoRateParam_ = parameters.getRawParameterValue("vibratoRate");
    droneOnParam_ = parameters.getRawParameterValue("droneOn");
    droneLevelParam_ = parameters.getRawParameterValue("droneLevel");
    droneOctaveParam_ = parameters.getRawParameterValue("droneOctave");

    try {
        library_ = maqam::parseLibrary(std::string(BinaryData::maqams_json, BinaryData::maqams_jsonSize));
        if (library_.findMaqam(selectedMaqamId_) == nullptr && !library_.maqamat.empty())
            selectedMaqamId_ = library_.maqamat.front().id;
    } catch (const std::exception& e) {
        loadError_ = e.what();
    }
    retune();
}

void MaqamProcessor::setMaqam(const std::string& id) {
    if (library_.findMaqam(id) == nullptr)
        return;
    selectedMaqamId_ = id;
    retune();
}

void MaqamProcessor::setTonic(const maqam::Tonic& tonic) {
    tonic_ = tonic;
    retune();
}

void MaqamProcessor::retune() {
    {
        const juce::ScopedLock lock(retuneLock_);
        std::array<KeyInfo, kNumKeys> info{};
        maqam::synth::KeyTuning tuning = maqam::synth::twelveTetKeyTuning();

        if (const maqam::Maqam* m = library_.findMaqam(selectedMaqamId_)) {
            try {
                // Ascending notes plus the descending form's notes on free keys (12-key layout).
                // Keys outside the maqam play 12-TET.
                const maqam::TuningTable table = maqam::makeTwelveKeyTable(*m, tonic_);
                tuning = maqam::synth::makeKeyTuning(table);

                const auto up = maqam::resolveScale(*m, maqam::Direction::Ascending);
                const auto down = maqam::resolveScale(*m, maqam::Direction::Descending);
                const auto names = maqam::stepNoteNames(table, *up, down ? &*down : nullptr, tonic_);
                const int steps = table.stepsPerPeriod();
                for (int k = 0; k < kNumKeys; ++k) {
                    const int step = ((k - table.tonicMidiNote) % steps + steps) % steps;
                    auto& ki = info[static_cast<size_t>(k)];
                    ki.degree = tuning.degree[static_cast<size_t>(k)];
                    ki.descending = table.alternateOfStep[static_cast<size_t>(step)];
                    ki.name = juce::String::fromUTF8(names[static_cast<size_t>(step)].c_str());
                }
            } catch (const std::exception&) {
                // A maqam that cannot be laid on 12 keys falls back to plain 12-TET.
            }
        }
        keyInfo_ = info;
        tuningExchange_.publish(tuning);
    }
    sendChangeMessage();
}

void MaqamProcessor::prepareToPlay(double sampleRate, int) {
    synth_.prepare(sampleRate);
    drone_.prepare(sampleRate);
    drone_.setTonicHz(tuningExchange_.current().tonicHz, 0.0);
    reverb_.setSampleRate(sampleRate);
    reverb_.reset();
    keyboardState.reset();
}

bool MaqamProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MaqamProcessor::handleMidi(const juce::MidiMessage& m) {
    if (m.isNoteOn())
        synth_.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        synth_.noteOff(m.getNoteNumber());
    else if (m.isSustainPedalOn())
        synth_.setSustain(true);
    else if (m.isSustainPedalOff())
        synth_.setSustain(false);
    else if (m.isAllSoundOff())
        synth_.killAll();
    else if (m.isAllNotesOff())
        synth_.allNotesOff();
    else if (m.isControllerOfType(1))  // mod wheel: vibrato depth
        synth_.setModWheel(static_cast<float>(m.getControllerValue()) / 127.0f);
}

void MaqamProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // Notes played on the on-screen keyboard join the host's MIDI.
    keyboardState.processNextMidiBuffer(midi, 0, numSamples, true);

    if (tuningExchange_.fetch()) {
        synth_.setTuning(tuningExchange_.current());
        drone_.setTonicHz(tuningExchange_.current().tonicHz);
    }

    using namespace maqam::synth;
    auto choice = [](std::atomic<float>* param) { return juce::roundToInt(param->load()); };
    SynthParams p;
    p.sound = static_cast<SoundType>(juce::jlimit(0, 2, choice(soundParam_)));
    p.brightness = brightnessParam_->load();
    p.decaySeconds = decayParam_->load();
    p.gain = juce::Decibels::decibelsToGain(volumeParam_->load(), -100.0f);
    p.limit = static_cast<NoteLimit>(juce::jlimit(0, 2, choice(limitParam_)));
    p.mode = static_cast<VoiceMode>(juce::jlimit(0, 2, choice(modeParam_)));
    p.glideSeconds = glideParam_->load();
    p.vibrato = vibratoParam_->load();
    p.vibratoRateHz = vibratoRateParam_->load();
    synth_.setParams(p);

    DroneParams dp;
    dp.on = droneOnParam_->load() > 0.5f;
    const float droneLevel = droneLevelParam_->load();
    dp.gain = droneLevel * droneLevel * 0.6f * p.gain;
    dp.octave = juce::jlimit(0, 2, choice(droneOctaveParam_)) - 2;
    drone_.setParams(dp);

    // Render up to each MIDI event so notes start on their exact sample.
    float* out = buffer.getWritePointer(0);
    int pos = 0;
    for (const auto meta : midi) {
        const int t = juce::jlimit(pos, numSamples, meta.samplePosition);
        if (t > pos)
            synth_.render(out + pos, t - pos);
        pos = t;
        handleMidi(meta.getMessage());
    }
    if (numSamples > pos)
        synth_.render(out + pos, numSamples - pos);
    drone_.render(out, numSamples);

    const float amount = reverbParam_->load();
    const int channels = buffer.getNumChannels();
    if (channels > 1)
        for (int c = 1; c < channels; ++c)
            buffer.copyFrom(c, 0, buffer, 0, 0, numSamples);
    if (amount > 0.001f) {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f;
        rp.damping = 0.5f;
        rp.width = 1.0f;
        rp.dryLevel = 0.5f;  // juce::Reverb doubles the dry level: 0.5 = unity
        rp.wetLevel = 0.3f * amount;
        reverb_.setParameters(rp);
        if (channels > 1)
            reverb_.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), numSamples);
        else
            reverb_.processMono(buffer.getWritePointer(0), numSamples);
    }
}

juce::AudioProcessorEditor* MaqamProcessor::createEditor() {
    return new MaqamEditor(*this);
}

void MaqamProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("maqam", juce::String(selectedMaqamId_), nullptr);
    state.setProperty("tonicMidi", tonic_.midiNote, nullptr);
    state.setProperty("tonicCents", tonic_.centsOffset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MaqamProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(kStateType))
        return;
    const auto state = juce::ValueTree::fromXml(*xml);
    parameters.replaceState(state);
    const std::string id = state.getProperty("maqam", juce::String(selectedMaqamId_)).toString().toStdString();
    if (library_.findMaqam(id) != nullptr)
        selectedMaqamId_ = id;
    tonic_.midiNote = juce::jlimit(0, 127, static_cast<int>(state.getProperty("tonicMidi", tonic_.midiNote)));
    tonic_.centsOffset = static_cast<double>(state.getProperty("tonicCents", tonic_.centsOffset));
    retune();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MaqamProcessor();
}
