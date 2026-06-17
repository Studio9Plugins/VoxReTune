#pragma once

#include "../detection/YinPitchDetector.h"
#include "../engine/CorrectionEngine.h"
#include "../models/CorrectionBox.h"
#include "../models/PitchLine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include <vector>

class VoxReTuneAudioProcessor final : public juce::AudioProcessor
{
public:
    struct UiTransport
    {
        bool isPlaying = false;
        double playheadSeconds = 0.0;
    };

    VoxReTuneAudioProcessor();
    ~VoxReTuneAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
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

    voxretune::PitchLine getDisplayPitchLine() const;
    UiTransport getUiTransport() const noexcept { return uiTransport; }
    const std::vector<voxretune::CorrectionBox>& getCorrectionBoxes() const noexcept { return correctionBoxes; }

    bool addCorrectionBox(const voxretune::CorrectionBox& box);
    bool updateCorrectionBox(const voxretune::CorrectionBox& box);
    bool removeCorrectionBox(const juce::Uuid& id);

private:
    struct PendingPitchPoint
    {
        double inputPlayheadSeconds = 0.0;
        float midiPitch = 0.0f;
        bool breakBefore = false;
    };

    struct TransportState
    {
        bool isPlaying = false;
        double timeSeconds = 0.0;
    };

    TransportState readTransportState() const;
    const voxretune::CorrectionBox* findActiveBox(double timeSeconds, float midiPitch) const noexcept;
    void handleTransportEdges(const TransportState& transport);
    float computeBlockRms(const juce::AudioBuffer<float>& buffer) const noexcept;
    float adaptiveSilenceThreshold() const noexcept;
    void flushPendingDisplayPoints(double currentPlayheadSeconds);
    void queueDisplayPoint(double inputPlayheadSeconds, float midiPitch, bool breakBefore);

    voxretune::YinPitchDetector pitchDetector;
    voxretune::CorrectionEngine correctionEngine;
    voxretune::PitchLine displayPitchLine;

    std::deque<PendingPitchPoint> pendingDisplayPoints;
    std::vector<voxretune::CorrectionBox> correctionBoxes;

    mutable juce::CriticalSection pitchLineLock;
    juce::CriticalSection boxLock;

    UiTransport uiTransport;
    bool wasPlaying = false;
    float lastDetectedMidi = 0.0f;
    bool lastVoiced = false;
    float passPeakRms = 0.0f;
    double consecutiveSilenceSeconds = 0.0;
    bool forceSegmentBreak = false;
    double latencySeconds = 0.0;
    double blockDurationSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxReTuneAudioProcessor)
};
