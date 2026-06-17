#pragma once

#include "../ui/PianoRollComponent.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

class VoxReTuneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            private juce::Timer
{
public:
    explicit VoxReTuneAudioProcessorEditor(VoxReTuneAudioProcessor&);
    ~VoxReTuneAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    VoxReTuneAudioProcessor& processor;
    juce::Component topPanel;
    voxretune::PianoRollComponent pianoRoll;
    juce::Label infoBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoxReTuneAudioProcessorEditor)
};
