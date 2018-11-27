#pragma once

#include <chrono>
#include <limits>

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnTimePeriod
{
    /**
     * Infinite duration value.
     */
    static constexpr qint64 kInfiniteDuration = -1;

    /**
     * Minimal possible timestamp value.
     */
    static constexpr qint64 kMinTimeValue = 0;

    /**
     * Maximal possible timestamp value.
     */
    static constexpr qint64 kMaxTimeValue = std::numeric_limits<qint64>::max();

    /**
     * Constructs a null time period.
     */
    constexpr QnTimePeriod() = default;

    /**
     * Constructor.
     *
     * \param startTimeMs Period's start time, normally in milliseconds since epoch.
     * \param durationMs Period's duration, in milliseconds.
     */
    constexpr QnTimePeriod(qint64 startTimeMs, qint64 durationMs):
        startTimeMs(startTimeMs),
        durationMs(durationMs)
    {
    }

    constexpr QnTimePeriod(
        const std::chrono::milliseconds& startTime,
        const std::chrono::milliseconds& duration)
        :
        QnTimePeriod(startTime.count(), duration.count())
    {
    }

    static constexpr QnTimePeriod fromInterval(qint64 startTimeMs, qint64 endTimeMs)
    {
        return startTimeMs <= endTimeMs
            ? QnTimePeriod(startTimeMs, endTimeMs - startTimeMs)
            : QnTimePeriod(endTimeMs, startTimeMs - endTimeMs);
    };

    static constexpr QnTimePeriod fromInterval(
        const std::chrono::milliseconds& startTime,
        const std::chrono::milliseconds& endTime)
    {
        return fromInterval(startTime.count(), endTime.count());
    };

    bool isLeftIntersection(const QnTimePeriod& other) const;

    bool isRightIntersection(const QnTimePeriod& other) const;

    bool isContainedIn(const QnTimePeriod& other) const;

    bool contains(qint64 timeMs) const;
    bool contains(std::chrono::milliseconds time) const { return contains(time.count()); }
    bool contains(const QnTimePeriod &timePeriod) const;

    /**
     * Expand period so it will contain target period.
     * \param timePeriod                Target time period.
     */
    void addPeriod(const QnTimePeriod &timePeriod);

    QnTimePeriod intersected(const QnTimePeriod &other) const;
    bool intersects(const QnTimePeriod &other) const;
    void clear();

    /** Time value bound by this period. */
    qint64 bound(qint64 timeMs) const;

    /**
     * Whether this is an empty period - a period of zero length.
     */
    constexpr bool isEmpty() const { return durationMs == 0; };

    qint64 endTimeMs() const;
    void setEndTimeMs(qint64 value);

    /**
     * Whether this is a null time period.
     */
    constexpr bool isNull() const { return startTimeMs == 0 && durationMs == 0; };

    /**
     * Whether this is a infinite time period.
     */
    constexpr bool isInfinite() const { return durationMs == kInfiniteDuration; }

    constexpr bool isValid() const { return durationMs == kInfiniteDuration || durationMs > 0; };

    /**
     * Returns infinite period starting from zero.
     */
    static constexpr QnTimePeriod anytime()
    {
        return QnTimePeriod(kMinTimeValue, kInfiniteDuration);
    };

    /**
     * \returns distance from the nearest period edge to the time in ms. Returns zerro if timeMs inside period
     */
    qint64 distanceToTime(qint64 timeMs) const;

    /**
     * Truncate period duration to the specified timeMs. This functions does nothing if truncate point is outside period.
     */
    void truncate(qint64 timeMs);

    /**
     * Truncate period start time to the specified timeMs. This functions does nothing if truncate point is outside period.
     */
    void truncateFront(qint64 timeMs);

    QnTimePeriod truncated(qint64 timeMs) const;

    QnTimePeriod truncatedFront(qint64 timeMs) const;

    QByteArray serialize() const;
    QnTimePeriod& deserialize(const QByteArray& data);

    void setStartTime(std::chrono::milliseconds value);
    std::chrono::milliseconds startTime() const;

    void setEndTime(std::chrono::milliseconds value);
    std::chrono::milliseconds endTime() const;

    void setDuration(std::chrono::milliseconds value);
    std::chrono::milliseconds duration() const;

    constexpr bool operator==(const QnTimePeriod& other) const
    {
        return startTimeMs == other.startTimeMs && durationMs == other.durationMs;
    };

    constexpr bool operator!=(const QnTimePeriod& other) const
    {
        return !(*this == other);
    };

    constexpr bool operator<(const QnTimePeriod& other) const
    {
        return startTimeMs < other.startTimeMs;
    };

    constexpr bool operator<(qint64 timeMs) const
    {
        return this->startTimeMs < timeMs;
    };

    /** Start time in milliseconds. */
    qint64 startTimeMs = 0;

    /**
     * Duration in milliseconds.
     * kInfiniteDuration if duration is infinite or unknown. It may be the case if this time period
     * represents a video chunk that is being recorded at the moment.
     */
    qint64 durationMs = 0;
};

constexpr bool operator<(qint64 timeMs, const QnTimePeriod& other)
{
    return timeMs < other.startTimeMs;
};

void PrintTo(const QnTimePeriod& period, ::std::ostream* os);
QDebug operator<<(QDebug dbg, const QnTimePeriod &period);

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);

QN_FUSION_DECLARE_FUNCTIONS(QnTimePeriod, (json)(metatype)(ubjson)(xml)(csv_record));
