#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

VoxReTuneAudioProcessor::VoxReTuneAudioProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void VoxReTuneAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    pitchDetector.prepare(sampleRate, getTotalNumInputChannels());
    correctionEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    blockDurationSeconds = samplesPerBlock / juce::jmax(1.0, sampleRate);
    latencySeconds = correctionEngine.getTotalLatencySamples() / juce::jmax(1.0, sampleRate);

    const int reportedLatency = correctionEngine.getTotalLatencySamples();
    setLatencySamples(reportedLatency);
    updateHostDisplay();
}

void VoxReTuneAudioProcessor::releaseResources()
{
    pitchDetector.reset();
    correctionEngine.reset();
}

void VoxReTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto transport = readTransportState();
    uiTransport.isPlaying = transport.isPlaying;
    uiTransport.playheadSeconds = transport.timeSeconds;

    handleTransportEdges(transport);

    const auto detection = pitchDetector.processBlock(buffer);
    const float blockRms = computeBlockRms(buffer);
    passPeakRms = juce::jmax(passPeakRms, blockRms);

    const bool yinVoiced = detection.voiced && detection.frequencyHz > 0.0f;
    const bool rmsSilent = blockRms < adaptiveSilenceThreshold();
    const bool drawable = yinVoiced && !rmsSilent;

    if (yinVoiced)
    {
        lastDetectedMidi = 69.0f + 12.0f * std::log2(detection.frequencyHz / 440.0f);
        lastVoiced = true;
    }
    else
    {
        lastVoiced = false;
        forceSegmentBreak = true;
    }

    if (rmsSilent)
        consecutiveSilenceSeconds += blockDurationSeconds;
    else
        consecutiveSilenceSeconds = 0.0;

    if (consecutiveSilenceSeconds >= 0.05)
        forceSegmentBreak = true;

    const juce::ScopedLock lock(boxLock);
    const auto* activeBox = findActiveBox(transport.timeSeconds, lastDetectedMidi);
    correctionEngine.process(buffer, activeBox, lastDetectedMidi, lastVoiced);

    if (transport.isPlaying)
    {
        if (drawable)
        {
            const bool breakBefore = forceSegmentBreak;
            forceSegmentBreak = false;

            float displayMidi = lastDetectedMidi + correctionEngine.getSmoothedSemitones();
            queueDisplayPoint(transport.timeSeconds, displayMidi, breakBefore);
        }

        flushPendingDisplayPoints(transport.timeSeconds);
    }
}

juce::AudioProcessorEditor* VoxReTuneAudioProcessor::createEditor()
{
    return new VoxReTuneAudioProcessorEditor(*this);
}

voxretune::PitchLine VoxReTuneAudioProcessor::getDisplayPitchLine() const
{
    const juce::ScopedLock lock(pitchLineLock);
    return displayPitchLine;
}

void VoxReTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree root { "VoxReTune" };
    juce::ValueTree boxes { "Boxes" };

    const juce::ScopedLock lock(boxLock);
    for (const auto& box : correctionBoxes)
        boxes.appendChild(box.toValueTree(), nullptr);

    root.appendChild(boxes, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary(*xml, destData);
}

void VoxReTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml == nullptr)
        return;

    const auto root = juce::ValueTree::fromXml(*xml);
    if (!root.isValid())
        return;

    const juce::ScopedLock lock(boxLock);
    correctionBoxes.clear();

    const auto boxes = root.getChildWithName("Boxes");
    for (int i = 0; i < boxes.getNumChildren(); ++i)
        correctionBoxes.push_back(voxretune::CorrectionBox::fromValueTree(boxes.getChild(i)));
}

bool VoxReTuneAudioProcessor::addCorrectionBox(const voxretune::CorrectionBox& box)
{
    const juce::ScopedLock lock(boxLock);
    for (const auto& existing : correctionBoxes)
        if (existing.overlaps(box))
            return false;

    correctionBoxes.push_back(box);
    return true;
}

bool VoxReTuneAudioProcessor::updateCorrectionBox(const voxretune::CorrectionBox& box)
{
    const juce::ScopedLock lock(boxLock);
    for (const auto& existing : correctionBoxes)
        if (existing.id != box.id && existing.overlaps(box))
            return false;

    for (auto& existing : correctionBoxes)
    {
        if (existing.id == box.id)
        {
            existing = box;
            return true;
        }
    }

    return false;
}

bool VoxReTuneAudioProcessor::removeCorrectionBox(const juce::Uuid& id)
{
    const juce::ScopedLock lock(boxLock);
    const auto it = std::remove_if(correctionBoxes.begin(), correctionBoxes.end(),
                                   [&](const voxretune::CorrectionBox& box) { return box.id == id; });
    if (it == correctionBoxes.end())
        return false;

    correctionBoxes.erase(it, correctionBoxes.end());
    return true;
}

VoxReTuneAudioProcessor::TransportState VoxReTuneAudioProcessor::readTransportState() const
{
    TransportState state;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            state.isPlaying = position->getIsPlaying();
            if (auto seconds = position->getTimeInSeconds())
                state.timeSeconds = *seconds;
        }
    }
    return state;
}

const voxretune::CorrectionBox* VoxReTuneAudioProcessor::findActiveBox(double timeSeconds,
                                                                       float midiPitch) const noexcept
{
    const voxretune::CorrectionBox* found = nullptr;
    for (const auto& box : correctionBoxes)
    {
        if (!box.bypass && box.containsTime(timeSeconds) && box.containsPitch(midiPitch))
            found = &box;
    }
    return found;
}

void VoxReTuneAudioProcessor::handleTransportEdges(const TransportState& transport)
{
    if (transport.isPlaying && !wasPlaying)
    {
        const juce::ScopedLock lock(pitchLineLock);
        displayPitchLine.markPassStart(transport.timeSeconds);
        pendingDisplayPoints.clear();
        passPeakRms = 0.0f;
        consecutiveSilenceSeconds = 0.0;
        forceSegmentBreak = false;
    }
    else if (!transport.isPlaying && wasPlaying)
    {
        flushPendingDisplayPoints(std::numeric_limits<double>::max());
    }

    wasPlaying = transport.isPlaying;
}

float VoxReTuneAudioProcessor::computeBlockRms(const juce::AudioBuffer<float>& buffer) const noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return 0.0f;

    double sumSquares = 0.0;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            sumSquares += static_cast<double>(data[i]) * data[i];
    }

    return static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numChannels * numSamples)));
}

float VoxReTuneAudioProcessor::adaptiveSilenceThreshold() const noexcept
{
    constexpr float kFloor = 0.000'05f;
    return juce::jmax(kFloor, passPeakRms * 0.02f);
}

void VoxReTuneAudioProcessor::queueDisplayPoint(double inputPlayheadSeconds,
                                                float midiPitch,
                                                bool breakBefore)
{
    pendingDisplayPoints.push_back({ inputPlayheadSeconds, midiPitch, breakBefore });
}

void VoxReTuneAudioProcessor::flushPendingDisplayPoints(double currentPlayheadSeconds)
{
    while (!pendingDisplayPoints.empty())
    {
        const auto& pending = pendingDisplayPoints.front();
        if (pending.inputPlayheadSeconds + latencySeconds > currentPlayheadSeconds)
            break;

        voxretune::PitchPoint point;
        point.timeSeconds = pending.inputPlayheadSeconds + latencySeconds;
        point.midiPitch = pending.midiPitch;
        point.voiced = true;
        point.breakBefore = pending.breakBefore;

        {
            const juce::ScopedLock lock(pitchLineLock);
            displayPitchLine.upsertPoint(point, blockDurationSeconds * 0.5);
        }

        pendingDisplayPoints.pop_front();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxReTuneAudioProcessor();
}
