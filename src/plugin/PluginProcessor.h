#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/MaqamData.h"
#include "core/Tuning.h"

class MaqamProcessor final : public juce::AudioProcessor {
public:
    MaqamProcessor();
    ~MaqamProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Message-thread state (no audio yet; the synth arrives in Phase 2).
    const maqam::MaqamLibrary& library() const { return library_; }
    const juce::String& loadError() const { return loadError_; }

    std::string selectedMaqamId = "hijaz";
    maqam::Tonic tonic;   // default F4

private:
    maqam::MaqamLibrary library_;
    juce::String loadError_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqamProcessor)
};
