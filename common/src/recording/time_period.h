#ifndef QN_TIME_PERIOD_H
#define QN_TIME_PERIOD_H

#include <QtCore/QVector>
#include <QtCore/QMetaType>

struct QnTimePeriod;
class QnTimePeriodList;

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

    static QnTimePeriodList mergeTimePeriods(const QVector<QnTimePeriodList>& periods);
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
     * \returns                         Position of a time period that is closest to the given time value.
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
        if (searchForward && itr->endTimeMs() <= timeMs)
            ++itr;
        return itr;
    }

    /**
     * Returns true if timePeriodList intersect period
     */
    bool containPeriod(const QnTimePeriod& period) const
    {
        const_iterator itrStart = findNearestPeriod(period.startTimeMs, true);
        const_iterator itrEnd = findNearestPeriod(period.endTimeMs(), true);
        for (const_iterator itr = itrStart; itr != itrEnd; ++itr) {
            if (!itr->intersect(period).isEmpty())
                return true;
        }
        return itrEnd != end() && !itrEnd->intersect(period).isEmpty();
    }
};

bool operator < (const QnTimePeriod& first, const QnTimePeriod& other);
bool operator < (qint64 first, const QnTimePeriod& other);
bool operator < (const QnTimePeriod& other, qint64 first);

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::TimePeriodTypes);

Q_DECLARE_TYPEINFO(QnTimePeriod, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(Qn::TimePeriodTypes);
Q_DECLARE_METATYPE(Qn::TimePeriodType);
Q_DECLARE_METATYPE(Qn::TimePeriodRole);
Q_DECLARE_METATYPE(QnTimePeriod);

#endif // QN_TIME_PERIOD_H
