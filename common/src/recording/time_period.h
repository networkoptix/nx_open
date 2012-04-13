#ifndef __TIME_PERIOD_H__
#define __TIME_PERIOD_H__

#include <QVector>

struct QnTimePeriod;
class QnTimePeriodList;
//typedef QVector<QnTimePeriod> QnTimePeriodList;

struct QN_EXPORT QnTimePeriod
{
    QnTimePeriod(): startTimeMs(0), durationMs(0) {}
    QnTimePeriod(qint64 _startTimeMs, qint64 _durationMs): startTimeMs(_startTimeMs), durationMs(_durationMs) {}

    bool operator==(const QnTimePeriod& other) const;

    static QnTimePeriodList mergeTimePeriods(QVector<QnTimePeriodList> periods);
    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
    
    /** Encode(compress) data to a byteArray. 
    * TimePeriods must be arranged by time and does not intersects. If condition is not meet, function returns false
    * Average compressed QnTimePeriod size near 6 bytes */
    static bool encode(QByteArray& stream, const QnTimePeriodList& periods);
    
    /** Decode(decompress) data from a byteArray. */
    static bool decode(QByteArray& stream, QnTimePeriodList& periods);
    static bool decode(const quint8* data, int dataSize, QnTimePeriodList& periods);

    bool containTime(qint64 timeMs) const;
    bool containPeriod(const QnTimePeriod& timePeriod) const;
    void addPeriod(const QnTimePeriod& timePeriod);
    QnTimePeriod intersect(const QnTimePeriod& other) const;
    void clear();
    bool isEmpty() const;

    qint64 endTimeMs() const;

    /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * -1 if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;

};

class QnTimePeriodList: public QVector<QnTimePeriod>
{
public:
    QnTimePeriodList(): QVector<QnTimePeriod>() 
    {

    }

    QnTimePeriodList::const_iterator findNearestPeriod(qint64 timeMs, bool searchForward) const
    {
        if (isEmpty())
            return end();
        QnTimePeriodList::const_iterator itr = qUpperBound(begin(), end(), timeMs);
        if (itr != begin())
            --itr;
        if (searchForward && !itr->containTime(timeMs))
            ++itr;
        return itr;
    }
};

bool operator < (const QnTimePeriod& first, const QnTimePeriod& other);
bool operator < (qint64 first, const QnTimePeriod& other);
bool operator < (const QnTimePeriod& other, qint64 first);

#endif // __TIME_PERIOD_H__
