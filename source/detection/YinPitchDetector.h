#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace voxretune
{

struct YinResult
{
    float frequencyHz = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

// YIN monophonic pitch tracker (de Cheveigné & Kawahara, 2002).
class YinPitchDetector
{
public:
    void prepare(double sampleRate, int channels);
    void reset() noexcept;

    YinResult processBlock(const juce::AudioBuffer<float>& buffer) noexcept;

    float getLastMidiPitch() const noexcept { return lastMidiPitch; }

private:
    YinResult analyseChannel(const float* samples, int numSamples) noexcept;
    float hzToMidi(float hz) const noexcept;

    double sampleRate = 44100.0;
    int numChannels = 1;
    int windowSize = 2048;
    float yinThreshold = 0.15f;

    std::vector<float> yinBuffer;
    std::vector<float> window;
    int writeIndex = 0;
    size_t samplesWritten = 0;
    float lastMidiPitch = 0.0f;
};

} // namespace voxretune
