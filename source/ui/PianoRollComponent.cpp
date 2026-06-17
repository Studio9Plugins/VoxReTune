#include "PianoRollComponent.h"

#include <cmath>

namespace voxretune
{

PianoRollComponent::PianoRollComponent()
{
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);

    viewDurationSeconds = kInitialViewDuration;
    viewStartSeconds = 0.0;
    viewMinMidi = kInitialCenterMidi - kInitialPitchSpan / 2;
    viewMaxMidi = viewMinMidi + kInitialPitchSpan;
    hasInitialisedView = true;
}

void PianoRollComponent::setPitchLine(const PitchLine& line)
{
    pitchLine = line;
    updateDataExtentFromLine();
    clampViewToData();
    repaint();
}

void PianoRollComponent::setCorrectionBoxes(const std::vector<CorrectionBox>& boxes)
{
    correctionBoxes = boxes;
    repaint();
}

void PianoRollComponent::setTransportState(bool isPlaying, double playheadSecondsIn)
{
    if (isPlaying && !transportPlaying)
    {
        autoFollowEnabled = true;
        userScrolledThisPass = false;
    }

    transportPlaying = isPlaying;
    playheadSeconds = playheadSecondsIn;
    updateAutoFollow();
    repaint();
}

void PianoRollComponent::setOnBoxChanged(BoxChangedCallback callback)
{
    onBoxChanged = std::move(callback);
}

void PianoRollComponent::setOnHoverNote(HoverNoteCallback callback)
{
    onHoverNote = std::move(callback);
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));

    const auto keyboard = keyboardArea();
    const auto grid = gridArea();
    const auto ruler = timeRulerArea();
    const auto content = gridContentArea();

    paintKeyboard(g, keyboard);
    paintTimeRuler(g, ruler);
    paintGrid(g, content);
    paintPitchLine(g, content);
    paintPlayhead(g, grid);

    g.setColour(juce::Colour(0xff404040));
    g.drawVerticalLine(keyboard.getRight(), static_cast<float>(getY()), static_cast<float>(getBottom()));
    g.drawHorizontalLine(ruler.getBottom(), static_cast<float>(grid.getX()), static_cast<float>(grid.getRight()));
}

void PianoRollComponent::resized()
{
    repaint();
}

void PianoRollComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto grid = gridArea();
    const auto content = gridContentArea();
    const auto keyboard = keyboardArea();
    if (!grid.contains(e.getPosition()) && !keyboard.contains(e.getPosition()))
        return;

    const double wheelDelta = wheel.deltaY + wheel.deltaX;

    if (e.mods.isCtrlDown())
    {
        const double factor = wheelDelta > 0 ? 0.9 : 1.1;
        zoomHorizontally(factor, xToTime(static_cast<float>(e.getPosition().getX()), content));
    }
    else if (e.mods.isShiftDown())
    {
        const double factor = wheelDelta > 0 ? 0.9 : 1.1;
        zoomVertically(factor, yToMidi(static_cast<float>(e.getPosition().getY()), content));
    }
    else if (e.mods.isAltDown())
    {
        beginUserScroll();
        scrollHorizontally(-wheelDelta * viewDurationSeconds * 0.1);
    }
    else
    {
        beginUserScroll();
        scrollVertically(wheelDelta > 0 ? -1 : 1);
    }

    repaint();
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!gridContentArea().contains(e.getPosition()))
        return;

    isDragging = true;
    dragStartPos = e.position;
    dragStartViewSeconds = viewStartSeconds;
    beginUserScroll();
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging)
        return;

    const auto content = gridContentArea();
    const float deltaX = dragStartPos.x - e.position.x;
    const double deltaSeconds = static_cast<double>(deltaX) * viewDurationSeconds
                              / juce::jmax(1, content.getWidth());
    viewStartSeconds = dragStartViewSeconds + deltaSeconds;
    clampViewToData();
    repaint();
}

void PianoRollComponent::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
}

void PianoRollComponent::mouseMove(const juce::MouseEvent& e)
{
    if (onHoverNote == nullptr)
        return;

    if (auto nearest = findNearestPitchPoint(e.position))
        onHoverNote(midiToNoteName(nearest->midiPitch));
    else
        onHoverNote({});
}

void PianoRollComponent::mouseExit(const juce::MouseEvent&)
{
    if (onHoverNote != nullptr)
        onHoverNote({});
}

void PianoRollComponent::paintKeyboard(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.setColour(juce::Colour(0xff202020));
    g.fillRect(area);

    const auto content = gridContentArea();
    const float rowHeight = semitoneRowHeight();
    const bool showAllNotes = rowHeight > kMinRowHeightForAllNotes;

    for (int midi = viewMinMidi; midi <= viewMaxMidi; ++midi)
    {
        const bool isC = (midi % 12) == 0;
        const float y = midiToY(static_cast<float>(midi), content);
        if (y < content.getY() || y > content.getBottom())
            continue;

        g.setColour(isC ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2f2f2f));
        g.drawHorizontalLine(juce::roundToInt(y), static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));

        if (!showAllNotes && !isC)
            continue;

        const float labelTop = y - rowHeight * 0.5f;
        const float labelBottom = y + rowHeight * 0.5f;
        if (labelBottom < content.getY() || labelTop > content.getBottom())
            continue;

        const int labelHeight = juce::jmax(12, juce::roundToInt(juce::jmax(rowHeight, kKeyboardFontSize + 2.0f)));
        auto labelArea = juce::Rectangle<int>(area.getX(),
                                              juce::roundToInt(y) - labelHeight / 2,
                                              area.getWidth() - 4,
                                              labelHeight);
        labelArea = labelArea.getIntersection(area);

        g.setColour(juce::Colours::lightgrey);
        g.setFont(kKeyboardFontSize);
        g.drawText(midiToNoteName(static_cast<float>(midi)),
                   labelArea,
                   juce::Justification::centredRight,
                   false);
    }
}

void PianoRollComponent::paintTimeRuler(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRect(area);

    const auto grid = chooseTimeGrid();
    const double firstLine = std::ceil(viewStartSeconds / grid.intervalSeconds) * grid.intervalSeconds;

    g.setFont(juce::FontOptions(11.0f));
    g.setColour(juce::Colours::lightgrey);

    for (double t = firstLine; t <= viewStartSeconds + viewDurationSeconds + grid.intervalSeconds; t += grid.intervalSeconds)
    {
        const float x = timeToX(t, area);
        if (x < area.getX() || x > area.getRight())
            continue;

        const bool major = std::fmod(t + 0.000'1, grid.intervalSeconds * 5.0) < grid.intervalSeconds * 0.1;
        g.setColour(major ? juce::Colour(0xff555555) : juce::Colour(0xff383838));
        g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));

        const float nextX = timeToX(t + grid.intervalSeconds, area);
        const int labelWidth = juce::jmax(1, juce::roundToInt(nextX - x) - 2);
        if (labelWidth < 18)
            continue;

        const auto label = formatTimelineSeconds(t, viewDurationSeconds);
        auto labelArea = juce::Rectangle<int>(juce::roundToInt(x) + 1, area.getY(), labelWidth, area.getHeight());
        labelArea = labelArea.getIntersection(area);
        g.setColour(juce::Colours::lightgrey);
        g.drawText(label, labelArea, juce::Justification::centredLeft, false);
    }
}

void PianoRollComponent::paintGrid(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(area);

    for (int midi = viewMinMidi; midi <= viewMaxMidi; ++midi)
    {
        const bool isC = (midi % 12) == 0;
        const float y = midiToY(static_cast<float>(midi), area);
        g.setColour(isC ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2f2f2f));
        g.drawHorizontalLine(juce::roundToInt(y), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    }

    const auto grid = chooseTimeGrid();
    const double firstLine = std::ceil(viewStartSeconds / grid.intervalSeconds) * grid.intervalSeconds;
    for (double t = firstLine; t <= viewStartSeconds + viewDurationSeconds + grid.intervalSeconds; t += grid.intervalSeconds)
    {
        const float x = timeToX(t, area);
        if (x < area.getX() || x > area.getRight())
            continue;

        const bool major = std::fmod(t + 0.000'1, grid.intervalSeconds * 5.0) < grid.intervalSeconds * 0.1;
        g.setColour(major ? juce::Colour(0xff4a4a4a) : juce::Colour(0xff333333));
        g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }

    for (const auto& box : correctionBoxes)
    {
        const float x0 = timeToX(box.startTimeSeconds, area);
        const float x1 = timeToX(box.endTimeSeconds, area);
        const float y0 = midiToY(static_cast<float>(box.pitchMaxMidi), area);
        const float y1 = midiToY(static_cast<float>(box.pitchMinMidi), area);

        const auto boxRect = juce::Rectangle<float>(x0, y0, x1 - x0, y1 - y0).constrainedWithin(area.toFloat());
        g.setColour(box.bypass ? juce::Colour(0x66ffcc00) : juce::Colour(0x8800c8ff));
        g.fillRect(boxRect);
        g.setColour(juce::Colour(0xff00c8ff));
        g.drawRect(boxRect, 1.0f);

        g.setColour(juce::Colours::white);
        g.drawText(midiToNoteName(static_cast<float>(box.targetMidiNote())),
                   boxRect.reduced(4.0f),
                   juce::Justification::centred,
                   false);
    }
}

void PianoRollComponent::paintPitchLine(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    if (pitchLine.isEmpty())
        return;

    juce::Path path;
    bool started = false;

    for (const auto& point : pitchLine.getPoints())
    {
        if (!point.voiced)
            continue;

        const float x = timeToX(point.timeSeconds, area);
        const float y = midiToY(point.midiPitch, area);

        if (point.breakBefore || !started)
        {
            path.startNewSubPath(x, y);
            started = true;
        }
        else
        {
            path.lineTo(x, y);
        }
    }

    g.setColour(juce::Colour(0xff44dd66));
    g.strokePath(path, juce::PathStrokeType(3.0f));
}

void PianoRollComponent::paintPlayhead(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    const float x = timeToX(playheadSeconds, area);
    if (x < area.getX() || x > area.getRight())
        return;

    g.setColour(juce::Colour(0xccffffff));
    g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
}

void PianoRollComponent::updateDataExtentFromLine()
{
    if (pitchLine.isEmpty())
        return;

    dataStartSeconds = pitchLine.getPassStartTime();
    dataEndSeconds = juce::jmax(dataStartSeconds + kMinViewDuration, pitchLine.getPassEndTime());

}

void PianoRollComponent::clampViewToData()
{
    viewDurationSeconds = juce::jmax(kMinViewDuration, viewDurationSeconds);

    const int span = juce::jlimit(kMinPitchSpan, kMaxPitchSpan, viewMaxMidi - viewMinMidi);
    viewMaxMidi = juce::jlimit(span, 127, viewMaxMidi);
    viewMinMidi = viewMaxMidi - span;
    viewMinMidi = juce::jlimit(0, 127 - span, viewMinMidi);
    viewMaxMidi = viewMinMidi + span;
}

void PianoRollComponent::updateAutoFollow()
{
    if (!transportPlaying || !autoFollowEnabled || userScrolledThisPass)
        return;

    const double viewEnd = viewStartSeconds + viewDurationSeconds;
    const double rightEdgeThreshold = viewEnd - viewDurationSeconds * 0.01;

    if (playheadSeconds >= rightEdgeThreshold)
        viewStartSeconds += viewDurationSeconds * 0.25;
}

PianoRollComponent::TimeGrid PianoRollComponent::chooseTimeGrid() const noexcept
{
    const double duration = viewDurationSeconds;

    if (duration <= 0.15)
        return { 0.02, 5 };
    if (duration <= 0.5)
        return { 0.05, static_cast<int>(std::ceil(duration / 0.05)) };
    if (duration <= 2.0)
        return { 0.1, static_cast<int>(std::ceil(duration / 0.1)) };
    if (duration <= 10.0)
        return { 1.0, static_cast<int>(std::ceil(duration)) };
    if (duration <= 60.0)
        return { 5.0, static_cast<int>(std::ceil(duration / 5.0)) };
    return { 10.0, static_cast<int>(std::ceil(duration / 10.0)) };
}

std::optional<PitchPoint> PianoRollComponent::findNearestPitchPoint(juce::Point<float> pos) const
{
    const auto content = gridContentArea();
    if (!content.contains(pos.toInt()))
        return std::nullopt;

    const double hoverTime = xToTime(pos.x, content);
    const float hoverMidi = static_cast<float>(yToMidi(pos.y, content));

    std::optional<PitchPoint> nearest;
    double bestDistance = 12.0;

    for (const auto& point : pitchLine.getPoints())
    {
        if (!point.voiced)
            continue;

        const double timeDistance = std::abs(point.timeSeconds - hoverTime);
        const double pitchDistance = std::abs(point.midiPitch - hoverMidi);
        const double distance = std::sqrt(timeDistance * timeDistance * 400.0 + pitchDistance * pitchDistance);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = point;
        }
    }

    return nearest;
}

void PianoRollComponent::beginUserScroll()
{
    userScrolledThisPass = true;
    autoFollowEnabled = false;
}

void PianoRollComponent::scrollHorizontally(double deltaSeconds)
{
    viewStartSeconds += deltaSeconds;
    clampViewToData();
}

void PianoRollComponent::scrollVertically(int deltaSemitones)
{
    const int span = viewMaxMidi - viewMinMidi;
    viewMinMidi = juce::jlimit(0, 127 - span, viewMinMidi + deltaSemitones);
    viewMaxMidi = viewMinMidi + span;
    clampViewToData();
}

void PianoRollComponent::zoomHorizontally(double factor, double anchorSeconds)
{
    beginUserScroll();

    const double oldDuration = viewDurationSeconds;
    const double maxDuration = juce::jmax(kMinViewDuration, dataEndSeconds - dataStartSeconds);
    const double newDuration = juce::jlimit(kMinViewDuration, maxDuration, oldDuration * factor);
    const double anchorRatio = (anchorSeconds - viewStartSeconds) / juce::jmax(kMinViewDuration, oldDuration);

    viewDurationSeconds = newDuration;
    viewStartSeconds = anchorSeconds - anchorRatio * newDuration;
    clampViewToData();
}

void PianoRollComponent::zoomVertically(double factor, int anchorMidi)
{
    beginUserScroll();

    const int oldSpan = viewMaxMidi - viewMinMidi;
    const int newSpan = juce::jlimit(kMinPitchSpan, kMaxPitchSpan, static_cast<int>(std::lround(oldSpan * factor)));
    const float anchorRatio = static_cast<float>(viewMaxMidi - anchorMidi) / juce::jmax(1, oldSpan);

    viewMaxMidi = anchorMidi + static_cast<int>(std::lround(anchorRatio * newSpan));
    viewMinMidi = viewMaxMidi - newSpan;
    clampViewToData();
}

juce::String PianoRollComponent::midiToNoteName(float midi) noexcept
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midiInt = juce::jlimit(0, 127, juce::roundToInt(midi));
    const int note = midiInt % 12;
    const int octave = midiInt / 12 - 1;
    return juce::String(names[note]) + juce::String(octave);
}

juce::Rectangle<int> PianoRollComponent::keyboardArea() const noexcept
{
    return getLocalBounds().withTrimmedLeft(0).withWidth(kKeyboardWidth);
}

juce::Rectangle<int> PianoRollComponent::gridArea() const noexcept
{
    return getLocalBounds().withTrimmedLeft(kKeyboardWidth);
}

juce::Rectangle<int> PianoRollComponent::timeRulerArea() const noexcept
{
    return gridArea().withHeight(kTimeRulerHeight);
}

juce::Rectangle<int> PianoRollComponent::gridContentArea() const noexcept
{
    return gridArea().withTrimmedTop(kTimeRulerHeight);
}

float PianoRollComponent::semitoneRowHeight() const noexcept
{
    const auto content = gridContentArea();
    const int span = juce::jmax(1, viewMaxMidi - viewMinMidi);
    return static_cast<float>(content.getHeight()) / static_cast<float>(span);
}

juce::String PianoRollComponent::formatTimelineSeconds(double seconds, double viewDuration) noexcept
{
    if (viewDuration <= 2.0)
        return juce::String(seconds, 2) + "s";

    if (viewDuration <= 60.0)
        return juce::String(seconds, 1) + "s";

    const int minutes = static_cast<int>(seconds) / 60;
    const int secs = static_cast<int>(seconds) % 60;
    return juce::String(minutes) + ":" + juce::String(secs).paddedLeft('0', 2);
}

float PianoRollComponent::timeToX(double seconds, const juce::Rectangle<int>& area) const noexcept
{
    const double norm = (seconds - viewStartSeconds) / juce::jmax(kMinViewDuration, viewDurationSeconds);
    return static_cast<float>(area.getX() + norm * area.getWidth());
}

float PianoRollComponent::midiToY(float midi, const juce::Rectangle<int>& area) const noexcept
{
    const float span = static_cast<float>(juce::jmax(1, viewMaxMidi - viewMinMidi));
    const float norm = (viewMaxMidi - midi) / span;
    return static_cast<float>(area.getY() + norm * area.getHeight());
}

double PianoRollComponent::xToTime(float x, const juce::Rectangle<int>& area) const noexcept
{
    const double norm = (x - area.getX()) / juce::jmax(1, area.getWidth());
    return viewStartSeconds + norm * viewDurationSeconds;
}

int PianoRollComponent::yToMidi(float y, const juce::Rectangle<int>& area) const noexcept
{
    const float span = static_cast<float>(juce::jmax(1, viewMaxMidi - viewMinMidi));
    const float norm = (y - area.getY()) / juce::jmax(1.0f, static_cast<float>(area.getHeight()));
    return juce::roundToInt(viewMaxMidi - norm * span);
}

} // namespace voxretune
