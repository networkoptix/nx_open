#ifndef QN_TIME_PERIOD_H
#define QN_TIME_PERIOD_H

#include <QtCore/QMetaType>

#include <common/common_globals.h>

#include <utils/common/model_functions_fwd.h>

class QnTimePeriod;

class QN_EXPORT QnTimePeriod {
public:
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

    bool operator==(const QnTimePeriod &other) const;

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
    bool isEmpty() const {
        return durationMs == 0;
    }

    qint64 endTimeMs() const;

    /**
     * \returns                         Whether this is a null time period. 
     */
    bool isNull() const;
    
    /**
     * \returns                         Whether this is a infinite time period. 
     */
    bool isInfinite() const;

    /**
     * \returns                         Infinite duration constant value (-1). 
     */
    static qint64 infiniteDuration();

    /**
     * \return                          Type of this time period.
     */
    Qn::TimePeriodType type() const;

    QByteArray serialize() const;
    QnTimePeriod& deserialize(const QByteArray& data);

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * infiniteDuration() if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;
};

bool operator<(const QnTimePeriod &first, const QnTimePeriod &other);
bool operator<(qint64 first, const QnTimePeriod &other);
bool operator<(const QnTimePeriod &other, qint64 first);

QDebug operator<<(QDebug dbg, const QnTimePeriod &period);

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);
QN_FUSION_DECLARE_FUNCTIONS(QnTimePeriod, (json)(metatype));

#endif // QN_TIME_PERIOD_H
