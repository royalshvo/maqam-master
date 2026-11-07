#include "PluginProcessor.h"

#include <BinaryData.h>

#include "PluginEditor.h"

MaqamProcessor::MaqamProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    try {
        library_ = maqam::parseLibrary(std::string(BinaryData::maqams_json, BinaryData::maqams_jsonSize));
        if (library_.findMaqam(selectedMaqamId) == nullptr && !library_.maqamat.empty())
            selectedMaqamId = library_.maqamat.front().id;
    } catch (const std::exception& e) {
        loadError_ = e.what();
    }
}

void MaqamProcessor::prepareToPlay(double, int) {}

bool MaqamProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void MaqamProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* MaqamProcessor::createEditor() {
    return new MaqamEditor(*this);
}

void MaqamProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::ValueTree state("MaqamMasterState");
    state.setProperty("maqam", juce::String(selectedMaqamId), nullptr);
    state.setProperty("tonicMidi", tonic.midiNote, nullptr);
    state.setProperty("tonicCents", tonic.centsOffset, nullptr);
    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void MaqamProcessor::setStateInformation(const void* data, int sizeInBytes) {
    const auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!state.hasType("MaqamMasterState"))
        return;
    const std::string id = state.getProperty("maqam", juce::String(selectedMaqamId)).toString().toStdString();
    if (library_.findMaqam(id) != nullptr)
        selectedMaqamId = id;
    tonic.midiNote = juce::jlimit(0, 127, static_cast<int>(state.getProperty("tonicMidi", tonic.midiNote)));
    tonic.centsOffset = static_cast<double>(state.getProperty("tonicCents", tonic.centsOffset));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MaqamProcessor();
}
