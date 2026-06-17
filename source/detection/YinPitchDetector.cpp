#include "YinPitchDetector.h"

#include <algorithm>
#include <cmath>

namespace voxretune
{

void YinPitchDetector::prepare(double newSampleRate, int channels)
{
    sampleRate = newSampleRate;
    numChannels = juce::jmax(1, channels);
    windowSize = juce::jlimit(1024, 4096, static_cast<int>(sampleRate * 0.046));
    yinBuffer.assign(static_cast<size_t>(windowSize / 2), 0.0f);
    window.assign(static_cast<size_t>(windowSize), 0.0f);
    writeIndex = 0;
    samplesWritten = 0;
    reset();
}

void YinPitchDetector::reset() noexcept
{
    lastMidiPitch = 0.0f;
    std::fill(window.begin(), window.end(), 0.0f);
    writeIndex = 0;
    samplesWritten = 0;
}

YinResult YinPitchDetector::processBlock(const juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return {};

    const float* primary = buffer.getReadPointer(0);
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = primary[i];
        if (numChannels > 1)
        {
            sample = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sample += buffer.getReadPointer(ch)[i];
            sample /= static_cast<float>(numChannels);
        }

        window[static_cast<size_t>(writeIndex)] = sample;
        writeIndex = (writeIndex + 1) % windowSize;
        ++samplesWritten;
    }

    if (samplesWritten < static_cast<size_t>(windowSize))
        return {};

    std::vector<float> ordered(static_cast<size_t>(windowSize));
    for (int i = 0; i < windowSize; ++i)
    {
        const int index = (writeIndex + i) % windowSize;
        ordered[static_cast<size_t>(i)] = window[static_cast<size_t>(index)];
    }

    return analyseChannel(ordered.data(), windowSize);
}

YinResult YinPitchDetector::analyseChannel(const float* samples, int numSamples) noexcept
{
    const int halfWindow = numSamples / 2;
    yinBuffer.assign(static_cast<size_t>(halfWindow), 0.0f);

    float runningSum = 0.0f;
    for (int tau = 1; tau < halfWindow; ++tau)
    {
        yinBuffer[static_cast<size_t>(tau)] = 0.0f;
        for (int i = 0; i < halfWindow; ++i)
        {
            const float delta = samples[i] - samples[i + tau];
            yinBuffer[static_cast<size_t>(tau)] += delta * delta;
        }

        runningSum += yinBuffer[static_cast<size_t>(tau)];
        if (runningSum > 0.0f)
            yinBuffer[static_cast<size_t>(tau)] *= static_cast<float>(tau) / runningSum;
    }

    int bestTau = -1;
    for (int tau = 2; tau < halfWindow; ++tau)
    {
        if (yinBuffer[static_cast<size_t>(tau)] < yinThreshold)
        {
            while (tau + 1 < halfWindow
                && yinBuffer[static_cast<size_t>(tau + 1)] < yinBuffer[static_cast<size_t>(tau)])
                ++tau;

            bestTau = tau;
            break;
        }
    }

    YinResult result;
    if (bestTau < 0)
        return result;

    const float y0 = yinBuffer[static_cast<size_t>(bestTau - 1)];
    const float y1 = yinBuffer[static_cast<size_t>(bestTau)];
    const float y2 = yinBuffer[static_cast<size_t>(bestTau + 1)];
    const float denominator = 2.0f * (2.0f * y1 - y2 - y0);
    const float betterTau = std::abs(denominator) > 1.0e-6f
        ? static_cast<float>(bestTau) + (y2 - y0) / denominator
        : static_cast<float>(bestTau);

    const float frequencyHz = static_cast<float>(sampleRate / betterTau);
    if (frequencyHz < 60.0f || frequencyHz > 1200.0f)
        return result;

    result.frequencyHz = frequencyHz;
    result.confidence = juce::jlimit(0.0f, 1.0f, 1.0f - y1);
    result.voiced = y1 < yinThreshold;
    lastMidiPitch = hzToMidi(frequencyHz);
    return result;
}

float YinPitchDetector::hzToMidi(float hz) const noexcept
{
    if (hz <= 0.0f)
        return 0.0f;
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

} // namespace voxretune
