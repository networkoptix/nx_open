#ifndef QN_TIME_PERIOD_LIST_H
#define QN_TIME_PERIOD_LIST_H

#include <vector>

#include <QtCore/QVector>

#include <api/api_fwd.h>

#include "time_period.h"
#include "utils/common/uuid.h"

class QnTimePeriodListTimeIterator;

/**
 * A sorted list of time periods that basically is an implementation of 
 * an interval container concept.
 */
class QnTimePeriodList: public std::vector<QnTimePeriod> {
public:
    QnTimePeriodList(): std::vector<QnTimePeriod>() {}
    QnTimePeriodList(const QnTimePeriod &singlePeriod);

    /**
     * \param timeMs                    Time value to search for, in milliseconds.
     * \param searchForward             How to behave when there is no interval containing the given time value.
     *                                  If false, position of an interval preceding the value is returned. 
     *                                  Otherwise, position of an interval that follows the value is returned. 
     *                                  Note that this position may equal <tt>end</tt>.
     * \returns                         Position of a time period that is closest to the given time value.
     */
    const_iterator findNearestPeriod(qint64 timeMs, bool searchForward) const;

    /**
     * \param period                    Period to check.
     * \returns                         Whether the given period intersects with
     *                                  any of the periods in this time period list.
     */
    bool intersects(const QnTimePeriod &period) const;

    bool containTime(qint64 timeMs) const;

    bool containPeriod(const QnTimePeriod &period) const;

    QnTimePeriodList intersected(const QnTimePeriod &period) const;

    /**
     * \returns                         Total duration of all periods in this list,
     *                                  or -1 if the last time period of this list is infinite.
     * 
     * \note                            This function performs in O(N), so you may want to cache the results.
     */
    qint64 duration() const;

    inline QnTimePeriodListTimeIterator timeBegin() const;
    inline QnTimePeriodListTimeIterator timeEnd() const;

    /** 
     * Encode (compress) data to a byte array. 
     * In high-optimized mode time periods must be arranged by time and must not intersect.
     * If this condition is not met, the function returns false. 
     * Average compressed QnTimePeriod size is close to 6 bytes in high-optimized mode and 7 bytes otherwise.
     * 
     * \param stream                    Byte array to compress time periods to. 
     * \param intersected               Disables high-optimized mode. That allow time periods to be intersected.
     */
    bool encode(QByteArray &stream, bool intersected = false);
    
    /** 
     * Decode (decompress) data from a byte array. 
     * 
     * \param[in] stream                Byte array to decompress time periods from.
     * \param[in] intersected           Flag that incoming time periods were encoded as intersected values.
     */
    bool decode(QByteArray &stream, bool intersected = false);

    /**
     * Decode (decompress) data from a byte array. 
     * 
     * \param[in] data                  Compressed data pointer.
     * \param[in] dataSize              Size of the compressed data.
     * \param[in] intersected           Flag that incoming time periods were encoded as intersected values.
     */
    bool decode(const quint8 *data, int dataSize, bool intersected = false);

    /** 
     * Find nearest period for specified time.
     * 
     * \param[in] timeMs                Time to find at usec
     * \param[in] searchForward         Rount time to the future if true or to the past if false
     * \returns                         Time moved to nearest chunk at usec
     */
    qint64 roundTimeToPeriodUSec(qint64 timeUsec, bool searchForward) const;

    static QnTimePeriodList mergeTimePeriods(const std::vector<QnTimePeriodList>& periods);
    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
};

struct MultiServerPeriodData
{
    QnUuid guid;
    QnTimePeriodList periods;
};

QN_FUSION_DECLARE_FUNCTIONS(MultiServerPeriodData, (json)(metatype)(ubjson)(xml)(csv_record)(compressed_time)(eq));

Q_DECLARE_METATYPE(QnTimePeriodList);
Q_DECLARE_METATYPE(MultiServerPeriodDataList);

#endif // QN_TIME_PERIOD_LIST_H
