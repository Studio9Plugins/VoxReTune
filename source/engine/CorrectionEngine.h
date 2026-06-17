#pragma once

#include "../models/CorrectionBox.h"
#include "signalsmith-stretch.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace voxretune
{

class CorrectionEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 const CorrectionBox* activeBox,
                 float detectedMidiPitch,
                 bool voiced);

    int getTotalLatencySamples() const noexcept { return totalLatencySamples; }
    float getSmoothedSemitones() const noexcept { return smoothedSemitones; }

private:
    void primeStretcher();
    void processDelayLine(juce::AudioBuffer<float>& buffer);
    void runStretcher(juce::AudioBuffer<float>& buffer);
    float computeTargetSemitones(const CorrectionBox& box, float detectedMidiPitch, bool voiced) noexcept;
    void updateSmoothedSemitones(float targetSemitones, float speedMs, int numSamples) noexcept;
    void applyFormantSettings(const CorrectionBox& box, float semitoneShift) noexcept;

    signalsmith::stretch::SignalsmithStretch<float> stretcher;
    std::vector<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>> delayLines;
    std::vector<std::vector<float>> inputScratch;
    std::vector<std::vector<float>> outputScratch;

    double sampleRate = 44100.0;
    int channels = 2;
    int maxBlockSize = 512;
    int totalLatencySamples = 0;
    bool configured = false;
    bool wasCorrecting = false;
    float smoothedSemitones = 0.0f;
    float slowPitchMidi = 0.0f;
    bool slowPitchInitialised = false;
};

} // namespace voxretune
