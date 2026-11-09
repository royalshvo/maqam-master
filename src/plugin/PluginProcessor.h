#pragma once

// JUCE headers must come before any standard header here: with this toolchain, including e.g.
// <array> first makes JUCE's macOS SDK check fail.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <string>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "core/Tuning.h"
#include "synth/Drone.h"
#include "synth/KeyTuning.h"
#include "synth/Synth.h"

class MaqamProcessor final : public juce::AudioProcessor, public juce::ChangeBroadcaster {
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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Message-thread API (the editor) ----
    const maqam::MaqamLibrary& library() const { return library_; }
    const juce::String& loadError() const { return loadError_; }
    const std::string& selectedMaqamId() const { return selectedMaqamId_; }
    const maqam::Tonic& tonic() const { return tonic_; }

    /** Changing maqam or tonic retunes the synth at once, including held notes, and sends a
        change message. */
    void setMaqam(const std::string& id);
    void setTonic(const maqam::Tonic& tonic);

    struct KeyInfo {
        int degree = 0;          // 1-based maqam degree, 0 outside the maqam
        bool descending = false; // the key holds a descending-form note
        juce::String name;       // note name on that key
    };
    /** What each MIDI key plays under the current maqam, for the on-screen keyboard. */
    const std::array<KeyInfo, maqam::synth::kNumKeys>& keyInfo() const { return keyInfo_; }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void retune();
    void handleMidi(const juce::MidiMessage& m);

    maqam::MaqamLibrary library_;
    juce::String loadError_;
    std::string selectedMaqamId_ = "hijaz";
    maqam::Tonic tonic_;   // default F4
    std::array<KeyInfo, maqam::synth::kNumKeys> keyInfo_;
    juce::CriticalSection retuneLock_;  // retune() can be called from the host's state thread too

    // Audio thread
    maqam::synth::Synth synth_;
    maqam::synth::Drone drone_;
    maqam::synth::TuningExchange tuningExchange_;
    juce::Reverb reverb_;
    std::atomic<float>* soundParam_ = nullptr;
    std::atomic<float>* brightnessParam_ = nullptr;
    std::atomic<float>* decayParam_ = nullptr;
    std::atomic<float>* volumeParam_ = nullptr;
    std::atomic<float>* reverbParam_ = nullptr;
    std::atomic<float>* limitParam_ = nullptr;
    std::atomic<float>* modeParam_ = nullptr;
    std::atomic<float>* glideParam_ = nullptr;
    std::atomic<float>* vibratoParam_ = nullptr;
    std::atomic<float>* vibratoRateParam_ = nullptr;
    std::atomic<float>* droneOnParam_ = nullptr;
    std::atomic<float>* droneLevelParam_ = nullptr;
    std::atomic<float>* droneOctaveParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaqamProcessor)
};
