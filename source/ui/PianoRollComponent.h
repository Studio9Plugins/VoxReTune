#pragma once

#include "../models/CorrectionBox.h"
#include "../models/PitchLine.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>
#include <vector>

namespace voxretune
{

class PianoRollComponent final : public juce::Component
{
public:
    using BoxChangedCallback = std::function<void(const CorrectionBox&)>;
    using HoverNoteCallback = std::function<void(const juce::String&)>;

    PianoRollComponent();

    void setPitchLine(const PitchLine& line);
    void setCorrectionBoxes(const std::vector<CorrectionBox>& boxes);
    void setTransportState(bool isPlaying, double playheadSeconds);
    void setOnBoxChanged(BoxChangedCallback callback);
    void setOnHoverNote(HoverNoteCallback callback);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    struct TimeGrid
    {
        double intervalSeconds = 1.0;
        int lineCount = 5;
    };

    static juce::String midiToNoteName(float midi) noexcept;
    juce::Rectangle<int> keyboardArea() const noexcept;
    juce::Rectangle<int> gridArea() const noexcept;
    juce::Rectangle<int> timeRulerArea() const noexcept;
    juce::Rectangle<int> gridContentArea() const noexcept;
    float timeToX(double seconds, const juce::Rectangle<int>& area) const noexcept;
    float midiToY(float midi, const juce::Rectangle<int>& area) const noexcept;
    double xToTime(float x, const juce::Rectangle<int>& area) const noexcept;
    int yToMidi(float y, const juce::Rectangle<int>& area) const noexcept;

    void paintKeyboard(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintTimeRuler(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintGrid(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintPitchLine(juce::Graphics& g, const juce::Rectangle<int>& area);
    void paintPlayhead(juce::Graphics& g, const juce::Rectangle<int>& area);

    static juce::String formatTimelineSeconds(double seconds, double viewDuration) noexcept;
    float semitoneRowHeight() const noexcept;

    void updateDataExtentFromLine();
    void clampViewToData();
    void updateAutoFollow();
    TimeGrid chooseTimeGrid() const noexcept;
    std::optional<PitchPoint> findNearestPitchPoint(juce::Point<float> pos) const;

    void beginUserScroll();
    void scrollHorizontally(double deltaSeconds);
    void scrollVertically(int deltaSemitones);
    void zoomHorizontally(double factor, double anchorSeconds);
    void zoomVertically(double factor, int anchorMidi);

    PitchLine pitchLine;
    std::vector<CorrectionBox> correctionBoxes;
    BoxChangedCallback onBoxChanged;
    HoverNoteCallback onHoverNote;

    double dataStartSeconds = 0.0;
    double dataEndSeconds = 30.0;
    double viewStartSeconds = 0.0;
    double viewDurationSeconds = 30.0;

    int viewMinMidi = 48;
    int viewMaxMidi = 84;

    bool transportPlaying = false;
    double playheadSeconds = 0.0;
    bool autoFollowEnabled = true;
    bool userScrolledThisPass = false;
    bool hasInitialisedView = false;
    bool isDragging = false;
    juce::Point<float> dragStartPos;
    double dragStartViewSeconds = 0.0;

    static constexpr int kKeyboardWidth = 52;
    static constexpr int kTimeRulerHeight = 24;
    static constexpr float kKeyboardFontSize = 13.5f;
    static constexpr float kMinRowHeightForAllNotes = 14.0f;
    static constexpr double kMinViewDuration = 0.1;
    static constexpr double kInitialViewDuration = 5.0;
    static constexpr int kInitialCenterMidi = 48;
    static constexpr int kInitialPitchSpan = 24;
    static constexpr int kMinPitchSpan = 6;
    static constexpr int kMaxPitchSpan = 48;
};

} // namespace voxretune
