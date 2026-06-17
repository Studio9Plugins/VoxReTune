#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace voxretune
{

struct PitchPoint
{
    double timeSeconds = 0.0;
    float midiPitch = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
    bool breakBefore = false;
};

class PitchLine
{
public:
    void clear() noexcept
    {
        points.clear();
        passStartTimeSeconds = 0.0;
        passEndTimeSeconds = 0.0;
    }

    void markPassStart(double startTimeSeconds) noexcept
    {
        passStartTimeSeconds = startTimeSeconds;
        passEndTimeSeconds = juce::jmax(passEndTimeSeconds, startTimeSeconds);
    }

    void upsertPoint(const PitchPoint& point, double mergeWindowSeconds = 0.005)
    {
        points.erase(std::remove_if(points.begin(), points.end(),
                                    [&](const PitchPoint& existing)
                                    {
                                        return std::abs(existing.timeSeconds - point.timeSeconds) <= mergeWindowSeconds;
                                    }),
                       points.end());
        appendPoint(point);
    }

    void appendPoint(const PitchPoint& point)
    {
        if (points.empty() || point.timeSeconds >= points.back().timeSeconds)
            points.push_back(point);
        else
            points.insert(std::upper_bound(points.begin(), points.end(), point.timeSeconds,
                                           [](double t, const PitchPoint& p) { return t < p.timeSeconds; }),
                          point);

        passStartTimeSeconds = points.empty() ? point.timeSeconds
                                                : std::min(passStartTimeSeconds, point.timeSeconds);
        passEndTimeSeconds = std::max(passEndTimeSeconds, point.timeSeconds);
    }

    const std::vector<PitchPoint>& getPoints() const noexcept { return points; }

    double getPassStartTime() const noexcept { return passStartTimeSeconds; }
    double getPassEndTime() const noexcept { return passEndTimeSeconds; }

    bool isEmpty() const noexcept { return points.empty(); }

private:
    std::vector<PitchPoint> points;
    double passStartTimeSeconds = 0.0;
    double passEndTimeSeconds = 0.0;
};

} // namespace voxretune
