#pragma once

#include <chrono>

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

class QnTimePeriod;

class QN_EXPORT QnTimePeriod
{
public:

    static const qint64 UnlimitedPeriod = -1;
    static const qint64 kMaxTimeValue;
    static const qint64 kMinTimeValue;

    /**
     * Constructs a null time period.
     */
    QnTimePeriod();

    /**
     * Constructor.
     *
     * \param startTimeMs               Period's start time, normally in milliseconds since epoch.
     * \param durationMs                Period's duration, in milliseconds.
     */
    QnTimePeriod(qint64 startTimeMs, qint64 durationMs);
    QnTimePeriod(
        const std::chrono::milliseconds& startTime,
        const std::chrono::milliseconds& duration);

    static QnTimePeriod fromInterval(qint64 startTimeMs, qint64 endTimeMs);

    QnTimePeriod& operator = (const QnTimePeriod &other);

    bool isLeftIntersection(const QnTimePeriod& other) const;

    bool isRightIntersection(const QnTimePeriod& other) const;

    bool isContainedIn(const QnTimePeriod& other) const;

    bool contains(qint64 timeMs) const;
    bool contains(const QnTimePeriod &timePeriod) const;

    /**
     * Expand period so it will contain target period.
     * \param timePeriod                Target time period.
     */
    void addPeriod(const QnTimePeriod &timePeriod);

    QnTimePeriod intersected(const QnTimePeriod &other) const;
    bool intersects(const QnTimePeriod &other) const;
    void clear();

    /**
     * \returns                         Whether this is an empty period --- a
     *                                  period of zero length.
     */
    bool isEmpty() const;

    qint64 endTimeMs() const;

    /**
     * \returns                         Whether this is a null time period.
     */
    bool isNull() const;

    /**
     * \returns                         Whether this is a infinite time period.
     */
    bool isInfinite() const;

    bool isValid() const;

    /**
     * \returns                         Infinite duration constant value (-1).
     */
    static qint64 infiniteDuration();

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

    void setDuration(std::chrono::milliseconds value);
    std::chrono::milliseconds duration() const;

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds.
     *
     * infiniteDuration() if duration is infinite or unknown. It may be the case if this time period
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;
};

bool operator==(const QnTimePeriod &first, const QnTimePeriod &other);
bool operator<(const QnTimePeriod &first, const QnTimePeriod &other);
bool operator<(qint64 first, const QnTimePeriod &other);
bool operator<(const QnTimePeriod &other, qint64 first);

QDebug operator<<(QDebug dbg, const QnTimePeriod &period);

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);

QN_FUSION_DECLARE_FUNCTIONS(QnTimePeriod, (json)(metatype)(ubjson)(xml)(csv_record));
