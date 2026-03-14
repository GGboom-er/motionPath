//
//  TimeKey.h
//  MotionPath
//
//  Time key utilities for map/set operations with tolerance-based comparison.
//  Converts floating-point frame times to integer ticks to avoid precision issues.
//

#ifndef TIMEKEY_H
#define TIMEKEY_H

#include <maya/MTime.h>
#include <cmath>
#include <functional>

// Tick-based time key to avoid floating-point precision issues
// Maya uses 6000 ticks per second internally (141120000 ticks per frame at 23.976fps)
// We use a simpler approach: multiply frame time by a large factor and round to integer

class TimeKey
{
public:
    // Precision factor: 1/10000 of a frame is sufficient for all practical purposes
    static constexpr int64_t PRECISION_FACTOR = 10000;

    int64_t ticks;

    TimeKey() : ticks(0) {}

    explicit TimeKey(double frameTime)
        : ticks(static_cast<int64_t>(std::round(frameTime * PRECISION_FACTOR))) {}

    explicit TimeKey(const MTime& time)
        : ticks(static_cast<int64_t>(std::round(time.value() * PRECISION_FACTOR))) {}

    // Convert back to double for use with Maya API
    double toDouble() const
    {
        return static_cast<double>(ticks) / PRECISION_FACTOR;
    }

    // Comparison operators
    bool operator<(const TimeKey& other) const { return ticks < other.ticks; }
    bool operator>(const TimeKey& other) const { return ticks > other.ticks; }
    bool operator<=(const TimeKey& other) const { return ticks <= other.ticks; }
    bool operator>=(const TimeKey& other) const { return ticks >= other.ticks; }
    bool operator==(const TimeKey& other) const { return ticks == other.ticks; }
    bool operator!=(const TimeKey& other) const { return ticks != other.ticks; }
};

// Comparator for double keys with tolerance (for use with existing maps)
// Tolerance of 1e-6 frames is approximately 0.04 microseconds at 24fps
struct FrameTimeComparator
{
    static constexpr double TOLERANCE = 1e-6;

    bool operator()(const double& a, const double& b) const
    {
        // If values are within tolerance, they are considered equal (return false for both a<b and b<a)
        if (std::abs(a - b) < TOLERANCE)
            return false;
        return a < b;
    }
};

// Hash function for TimeKey (for use with unordered containers if needed)
namespace std
{
    template<>
    struct hash<TimeKey>
    {
        size_t operator()(const TimeKey& key) const
        {
            return hash<int64_t>()(key.ticks);
        }
    };
}

#endif // TIMEKEY_H
