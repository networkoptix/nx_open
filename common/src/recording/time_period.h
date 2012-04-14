#ifndef QN_TIME_PERIOD_H
#define QN_TIME_PERIOD_H

#include <QVector>

struct QnTimePeriod;
class QnTimePeriodList;

struct QN_EXPORT QnTimePeriod
{
    /**
     * Constructs a null time period.
     */
    QnTimePeriod(): startTimeMs(0), durationMs(0) {}

    /**
     * \param startTimeMs               Period's start time, normally in milliseconds since epoch.
     * \param durationMs                Period's duration, in milliseconds.
     */
    QnTimePeriod(qint64 startTimeMs, qint64 durationMs): startTimeMs(startTimeMs), durationMs(durationMs) {}

    bool operator==(const QnTimePeriod& other) const;

    static QnTimePeriodList mergeTimePeriods(QVector<QnTimePeriodList> periods);
    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
    
    /** 
     * Encode (compress) data to a byte array. 
     * TimePeriods must be arranged by time and must not intersect. If this condition is not met, the function returns false. 
     * Average compressed QnTimePeriod size is close to 6 bytes.
     * 
     * \param stream                    Byte array to compress time periods to. 
     * \param periods                   List of time periods to compress. 
     */
    static bool encode(QByteArray &stream, const QnTimePeriodList &periods);
    
    /** 
     * Decode (decompress) data from a byte array. 
     * 
     * \param[in] stream                Byte array to decompress time periods from.
     * \param[out] periods              Decompressed time periods.
     */
    static bool decode(QByteArray &stream, QnTimePeriodList &periods);

    /**
     * Decode (decompress) data from a byte array. 
     * 
     * \param[in] data                  Compressed data pointer.
     * \param[in] dataSize              Size of the compressed data.
     * \param[out] periods              Decompressed time periods.
     */
    static bool decode(const quint8 *data, int dataSize, QnTimePeriodList &periods);

    bool containTime(qint64 timeMs) const;
    bool containPeriod(const QnTimePeriod &timePeriod) const;
    void addPeriod(const QnTimePeriod &timePeriod);
    QnTimePeriod intersect(const QnTimePeriod &other) const;
    void clear();
    bool isEmpty() const;

    qint64 endTimeMs() const;

    /**
     * \returns                         Whether this is a null time period. 
     */
    bool isNull() const {
        return startTimeMs == 0 && durationMs == 0;
    }

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

    /**
     * \param timeMs                    Time value to search for, in milliseconds.
     * \param searchForward             How to behave when there is no interval containing the given time value.
     *                                  If false, position of an interval preceding the value is returned. 
     *                                  Otherwise, position of an interval that follows the value is returned. 
     *                                  Note that this position may equal <tt>end</tt>.
     * \returns                         Position of a time period that is the closest to the given time value.
     */
    const_iterator findNearestPeriod(qint64 timeMs, bool searchForward) const
    {
        if (isEmpty())
            return end();

        const_iterator itr = qUpperBound(begin(), end(), timeMs);
        if (itr != begin())
            --itr;

        /* Note that there is no need to check for itr != end() here as
         * the container is not empty. */
        if (searchForward && !itr->containTime(timeMs)) 
            ++itr;
        return itr;
    }
};

bool operator < (const QnTimePeriod& first, const QnTimePeriod& other);
bool operator < (qint64 first, const QnTimePeriod& other);
bool operator < (const QnTimePeriod& other, qint64 first);


namespace Qn {
    enum TimePeriodType {
        RecordingTimePeriod,
        MotionTimePeriod,
        TimePeriodTypeCount
    };

} // namespace Qn


#endif // QN_TIME_PERIOD_H
