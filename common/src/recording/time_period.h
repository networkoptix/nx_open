#ifndef __TIME_PERIOD_H__
#define __TIME_PERIOD_H__

#include <QVector>

struct QnTimePeriod;
typedef QVector<QnTimePeriod> QnTimePeriodList;

struct QnTimePeriod
{
    QnTimePeriod(): startTimeMs(0), durationMs(0) {}
    QnTimePeriod(qint64 _startTimeMs, int _durationMs): startTimeMs(_startTimeMs), durationMs(_durationMs) {}

    static QnTimePeriodList mergeTimePeriods(QVector<QnTimePeriodList> periods);

    bool containTime(qint64 timeMs) const;
    bool containPeriod(const QnTimePeriod& timePeriod) const;
    void addPeriod(const QnTimePeriod& timePeriod);
    void clear();

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * -1 if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;

};
bool operator < (const QnTimePeriod& first, const QnTimePeriod& other);
bool operator < (qint64 first, const QnTimePeriod& other);
bool operator < (const QnTimePeriod& other, qint64 first);

#endif // __TIME_PERIOD_H__
