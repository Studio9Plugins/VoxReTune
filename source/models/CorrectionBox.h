#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace voxretune
{

struct CorrectionBox
{
    juce::Uuid id;

    double startTimeSeconds = 0.0;
    double endTimeSeconds = 0.0;

    // Vertical placement on the piano roll defines the correction region and target note.
    int pitchMinMidi = 60;
    int pitchMaxMidi = 62;

    float strength = 1.0f;        // 0..1 blend toward target pitch
    float speedMs = 50.0f;        // portamento time to reach target
    float deviationCents = 25.0f;   // dead zone around target before correction
    float vibratoKeep = 0.5f;       // 0..1 preserve fast pitch modulation
    bool formantPreserve = true;
    bool bypass = false;

    int targetMidiNote() const noexcept
    {
        return (pitchMinMidi + pitchMaxMidi) / 2;
    }

    double targetHz(double referenceA4Hz = 440.0) const noexcept
    {
        return referenceA4Hz * std::pow(2.0, (targetMidiNote() - 69) / 12.0);
    }

    bool containsTime(double timeSeconds) const noexcept
    {
        return timeSeconds >= startTimeSeconds && timeSeconds < endTimeSeconds;
    }

    bool containsPitch(float midiPitch) const noexcept
    {
        return midiPitch >= static_cast<float>(pitchMinMidi)
            && midiPitch <= static_cast<float>(pitchMaxMidi);
    }

    bool overlaps(const CorrectionBox& other) const noexcept
    {
        const bool timeOverlap = startTimeSeconds < other.endTimeSeconds
                              && endTimeSeconds > other.startTimeSeconds;
        const bool pitchOverlap = pitchMinMidi <= other.pitchMaxMidi
                               && pitchMaxMidi >= other.pitchMinMidi;
        return timeOverlap && pitchOverlap;
    }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree tree { "CorrectionBox" };
        tree.setProperty("id", id.toString(), nullptr);
        tree.setProperty("startTime", startTimeSeconds, nullptr);
        tree.setProperty("endTime", endTimeSeconds, nullptr);
        tree.setProperty("pitchMin", pitchMinMidi, nullptr);
        tree.setProperty("pitchMax", pitchMaxMidi, nullptr);
        tree.setProperty("strength", strength, nullptr);
        tree.setProperty("speedMs", speedMs, nullptr);
        tree.setProperty("deviationCents", deviationCents, nullptr);
        tree.setProperty("vibratoKeep", vibratoKeep, nullptr);
        tree.setProperty("formantPreserve", formantPreserve, nullptr);
        tree.setProperty("bypass", bypass, nullptr);
        return tree;
    }

    static CorrectionBox fromValueTree(const juce::ValueTree& tree)
    {
        CorrectionBox box;
        box.id = juce::Uuid(tree.getProperty("id").toString());
        box.startTimeSeconds = tree.getProperty("startTime");
        box.endTimeSeconds = tree.getProperty("endTime");
        box.pitchMinMidi = tree.getProperty("pitchMin");
        box.pitchMaxMidi = tree.getProperty("pitchMax");
        box.strength = tree.getProperty("strength");
        box.speedMs = tree.getProperty("speedMs");
        box.deviationCents = tree.getProperty("deviationCents");
        box.vibratoKeep = tree.getProperty("vibratoKeep");
        box.formantPreserve = tree.getProperty("formantPreserve");
        box.bypass = tree.getProperty("bypass");
        return box;
    }
};

} // namespace voxretune
