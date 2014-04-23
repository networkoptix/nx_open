#ifndef QN_TIME_PERIOD_H
#define QN_TIME_PERIOD_H

#include <QtCore/QMetaType>

#include <common/common_globals.h>

#include <utils/common/json_fwd.h>

class QnTimePeriod;
class QnTimePeriodList;

class QnAbstractTimePeriodList;
typedef QSharedPointer<QnAbstractTimePeriodList> QnAbstractTimePeriodListPtr;

class QnAbstractTimePeriodList {
public:
    QnAbstractTimePeriodList() {}
    virtual bool isEmpty() const = 0;
    virtual void append(const QnAbstractTimePeriodListPtr &other) = 0;
    virtual QnTimePeriod last() const = 0;

    // method is not static because we need to know subclass implementation
    virtual QnAbstractTimePeriodListPtr merged(const QVector<QnAbstractTimePeriodListPtr> &source) = 0;
};

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

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(QnTimePeriod);

QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS(QnTimePeriod);

#endif // QN_TIME_PERIOD_H
