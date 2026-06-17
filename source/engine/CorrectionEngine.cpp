#include "CorrectionEngine.h"

#include <cmath>

namespace voxretune
{

void CorrectionEngine::prepare(double newSampleRate, int maxBlockSizeIn, int numChannels)
{
    sampleRate = juce::jmax(1.0, newSampleRate);
    channels = juce::jmax(1, numChannels);
    maxBlockSize = juce::jmax(1, maxBlockSizeIn);

    stretcher.presetDefault(channels, static_cast<float>(sampleRate), false);
    configured = true;
    totalLatencySamples = stretcher.inputLatency() + stretcher.outputLatency();

    delayLines.resize(static_cast<size_t>(channels));
    for (auto& delayLine : delayLines)
    {
        delayLine.setMaximumDelayInSamples(totalLatencySamples + maxBlockSize);
        delayLine.setDelay(static_cast<float>(totalLatencySamples));
        delayLine.reset();
    }

    inputScratch.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(maxBlockSize), 0.0f));
    outputScratch.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(maxBlockSize), 0.0f));

    reset();
    primeStretcher();
}

void CorrectionEngine::reset()
{
    if (configured)
        stretcher.reset();

    for (auto& delayLine : delayLines)
    {
        delayLine.reset();
        delayLine.setDelay(static_cast<float>(totalLatencySamples));
    }

    smoothedSemitones = 0.0f;
    slowPitchMidi = 0.0f;
    slowPitchInitialised = false;
    wasCorrecting = false;
}

void CorrectionEngine::primeStretcher()
{
    if (!configured)
        return;

    const int seekSamples = stretcher.outputSeekLength(1.0f);
    if (seekSamples <= 0)
        return;

    inputScratch.assign(static_cast<size_t>(channels), std::vector<float>(static_cast<size_t>(seekSamples), 0.0f));

    struct ChannelInput
    {
        const std::vector<std::vector<float>>* data;
        int length;

        const float* operator[](int c) const
        {
            return data->at(static_cast<size_t>(c)).data();
        }
    } input { &inputScratch, seekSamples };

    stretcher.outputSeek(input, seekSamples);
}

void CorrectionEngine::process(juce::AudioBuffer<float>& buffer,
                               const CorrectionBox* activeBox,
                               float detectedMidiPitch,
                               bool voiced)
{
    if (!configured)
        return;

    const bool correcting = activeBox != nullptr && !activeBox->bypass;

    if (correcting)
    {
        if (!wasCorrecting)
            primeStretcher();

        const float targetSemitones = computeTargetSemitones(*activeBox, detectedMidiPitch, voiced);
        updateSmoothedSemitones(targetSemitones, activeBox->speedMs, buffer.getNumSamples());
        applyFormantSettings(*activeBox, smoothedSemitones);
        stretcher.setTransposeSemitones(smoothedSemitones);
        runStretcher(buffer);
    }
    else
    {
        smoothedSemitones = 0.0f;
        processDelayLine(buffer);
    }

    wasCorrecting = correcting;
}

void CorrectionEngine::processDelayLine(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(channels, buffer.getNumChannels());

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto& delayLine = delayLines[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i)
        {
            delayLine.pushSample(0, data[i]);
            data[i] = delayLine.popSample(0);
        }
    }
}

void CorrectionEngine::runStretcher(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        auto& in = inputScratch[static_cast<size_t>(ch)];
        std::copy(src, src + numSamples, in.begin());
    }

    struct ChannelInput
    {
        const std::vector<std::vector<float>>* data;
        int length;

        const float* operator[](int c) const
        {
            return data->at(static_cast<size_t>(c)).data();
        }
    } input { &inputScratch, numSamples };

    struct ChannelOutput
    {
        std::vector<std::vector<float>>* data;
        int length;

        float* operator[](int c)
        {
            return data->at(static_cast<size_t>(c)).data();
        }
    } output { &outputScratch, numSamples };

    stretcher.process(input, numSamples, output, numSamples);

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto& out = outputScratch[static_cast<size_t>(ch)];
        buffer.copyFrom(ch, 0, out.data(), numSamples);
    }
}

float CorrectionEngine::computeTargetSemitones(const CorrectionBox& box,
                                                 float detectedMidiPitch,
                                                 bool voiced) noexcept
{
    if (!voiced)
        return smoothedSemitones;

    const float targetMidi = static_cast<float>(box.targetMidiNote());

    if (!slowPitchInitialised)
    {
        slowPitchMidi = detectedMidiPitch;
        slowPitchInitialised = true;
    }

    const float vibratoBlend = juce::jlimit(0.0f, 1.0f, box.vibratoKeep);
    const float fastComponent = detectedMidiPitch - slowPitchMidi;
    slowPitchMidi += (1.0f - vibratoBlend) * fastComponent;
    slowPitchMidi += vibratoBlend * 0.02f * fastComponent;

    const float centsFromTarget = (slowPitchMidi - targetMidi) * 100.0f;
    if (std::abs(centsFromTarget) <= box.deviationCents)
        return 0.0f;

    const float rawSemitones = targetMidi - slowPitchMidi;
    const float clamped = juce::jlimit(-6.0f, 6.0f, rawSemitones);
    return clamped * juce::jlimit(0.0f, 1.0f, box.strength);
}

void CorrectionEngine::updateSmoothedSemitones(float targetSemitones, float speedMs, int numSamples) noexcept
{
    const float blockMs = static_cast<float>(numSamples * 1000.0 / sampleRate);
    const float tauMs = juce::jmax(1.0f, speedMs);
    const float alpha = 1.0f - std::exp(-blockMs / tauMs);
    smoothedSemitones += alpha * (targetSemitones - smoothedSemitones);
}

void CorrectionEngine::applyFormantSettings(const CorrectionBox& box, float semitoneShift) noexcept
{
    if (box.formantPreserve)
        stretcher.setFormantSemitones(-semitoneShift, true);
    else
        stretcher.setFormantSemitones(0.0f, false);
}

} // namespace voxretune
