#ifndef QN_TIME_PERIOD_H
#define QN_TIME_PERIOD_H

#include <QtCore/QMetaType>

class QnTimePeriod;
class QnTimePeriodList;

// TODO: #Elric move to common_globals
namespace Qn {
    enum TimePeriodType {
        NullTimePeriod      = 0x1,  /**< No period. */
        EmptyTimePeriod     = 0x2,  /**< Period of zero length. */
        NormalTimePeriod    = 0x4,  /**< Normal period with non-zero length. */
    };
    Q_DECLARE_FLAGS(TimePeriodTypes, TimePeriodType);

    enum TimePeriodRole {
        RecordingRole,
        MotionRole,
        TimePeriodRoleCount
    };

} // namespace Qn


class QN_EXPORT QnTimePeriod {
public:
    /**
     * Constructs a null time period.
     */
    QnTimePeriod(): startTimeMs(0), durationMs(0) {}

    /**
     * Constructor.
     *
     * \param startTimeMs               Period's start time, normally in milliseconds since epoch.
     * \param durationMs                Period's duration, in milliseconds.
     */
    QnTimePeriod(qint64 startTimeMs, qint64 durationMs): startTimeMs(startTimeMs), durationMs(durationMs) {}

    bool operator==(const QnTimePeriod &other) const;

    static QnTimePeriodList mergeTimePeriods(const QVector<QnTimePeriodList>& periods);
    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
    

    bool contains(qint64 timeMs) const;
    bool contains(const QnTimePeriod &timePeriod) const;

    /**
     * Expand period so it will contain target period.
     * \param timePeriod                Target time period.
     */
    void addPeriod(const QnTimePeriod &timePeriod);

    QnTimePeriod intersected(const QnTimePeriod &other) const;
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
    bool isNull() const {
        return startTimeMs == 0 && durationMs == 0;
    }

    /**
     * \return                          Type of this time period.
     */
    Qn::TimePeriodType type() const {
        if(isNull()) {
            return Qn::NullTimePeriod;
        } else if(isEmpty()) {
            return Qn::EmptyTimePeriod;
        } else {
            return Qn::NormalTimePeriod;
        }
    }

    QByteArray serialize() const;
    QnTimePeriod& deserialize(const QByteArray& data);

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * -1 if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;
};

bool operator<(const QnTimePeriod &first, const QnTimePeriod &other);
bool operator<(qint64 first, const QnTimePeriod &other);
bool operator<(const QnTimePeriod &other, qint64 first);

QDebug operator<<(QDebug dbg, const QnTimePeriod &period);

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::TimePeriodTypes);

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Qn::TimePeriodTypes);
Q_DECLARE_METATYPE(Qn::TimePeriodType);
Q_DECLARE_METATYPE(Qn::TimePeriodRole);
Q_DECLARE_METATYPE(QnTimePeriod);

#endif // QN_TIME_PERIOD_H
