// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_TIME_PERIOD_LIST_H
#define QN_TIME_PERIOD_LIST_H

#define QN_TIME_PERIODS_STD

#ifdef QN_TIME_PERIODS_STD
#include <vector>
#else
#include <QtCore/QVector>
#endif

#include "time_period.h"
#include <nx/utils/uuid.h>
#include <nx/utils/log/assert.h>

/**
 * A sorted list of time periods that basically is an implementation of
 * an interval container concept.
 */
class NX_VMS_COMMON_API QnTimePeriodList:
#ifdef QN_TIME_PERIODS_STD
    public std::vector<QnTimePeriod>
#else
    public QVector<QnTimePeriod>
#endif
{
#ifdef QN_TIME_PERIODS_STD
    typedef std::vector<QnTimePeriod> base_type;
#else
    typedef QVector<QnTimePeriod> base_type;
#endif

public:
    QnTimePeriodList();
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
    bool intersects(const QnTimePeriod& period) const;
    bool containTime(qint64 timeMs) const;
    bool containPeriod(const QnTimePeriod &period) const;
    QnTimePeriodList intersected(const QnTimePeriod &period) const;
    QnTimePeriodList intersected(const QnTimePeriodList& other) const;

    /** Get list of periods, that starts in the target period. */
    QnTimePeriodList intersectedPeriods(const QnTimePeriod &period) const;

    /**
     * \returns                         Total duration of all periods in this list,
     *                                  or -1 if the last time period of this list is infinite.
     *
     * \note                            This function performs in O(N), so you may want to cache the results.
     */
    qint64 duration() const;

    /**
     * \returns                         Bounding period for this period list.
     * \param truncateInfinite          Whether the infinite period should be truncated to fixed value.
     */
    QnTimePeriod boundingPeriod(qint64 truncateInfinite = QnTimePeriod::kInfiniteDuration) const;

#ifdef QN_TIME_PERIODS_STD
    const QnTimePeriod &first() const { return *begin(); }
    QnTimePeriod &first() { return *begin(); }

    const QnTimePeriod &last() const { return *(end() - 1); }
    QnTimePeriod &last() { return *(end() - 1); }

    bool isEmpty() const { return empty(); }
#endif

    /**
     * Encode (compress) data to a byte array.
     * In high-optimized mode time periods must be arranged by time and must not intersect.
     * If this condition is not met, the function returns false.
     * Average compressed QnTimePeriod size is close to 6 bytes in high-optimized mode and 7 bytes otherwise.
     *
     * \param stream                    Byte array to compress time periods to.
     */
    bool encode(QByteArray &stream);

    /**
     * Decode (decompress) data from a byte array.
     *
     * \param[in] stream                Byte array to decompress time periods from.
     */
    bool decode(QByteArray &stream);

    /**
     * Decode (decompress) data from a byte array.
     *
     * \param[in] data                  Compressed data pointer.
     * \param[in] dataSize              Size of the compressed data.
     */
    bool decode(const quint8 *data, int dataSize);

    /**
     * Find nearest period for specified time.
     *
     * \param[in] timeMs                Time to find at usec
     * \param[in] searchForward         Rount time to the future if true or to the past if false
     * \returns                         Time moved to nearest chunk at usec
     */
    qint64 roundTimeToPeriodUSec(qint64 timeUsec, bool searchForward) const;

    /** Merge some time period lists into one. */
    static QnTimePeriodList mergeTimePeriods(
        const std::vector<QnTimePeriodList>& periods,
        int limit = std::numeric_limits<int>::max(),
        Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder);

    /** Update tail of the period list with provided tail.
     *
     * \param[in] periods               Base list.
     * \param[in] tail                  List of updates.
     * \param[in] dividerPoint          Point where the tail should be appended.
     * \returns                         Updated list.
     */
    static void overwriteTail(QnTimePeriodList& periods, const QnTimePeriodList &tail, qint64 dividerPoint);

    /** Union two time period lists. Does not delete periods from the base list.
     *
     * \param[in] basePeriods           Base list.
     * \param[in] appendingPeriods      Appending list.
     * \returns                         Merged list.
     */
    static void unionTimePeriods(QnTimePeriodList& basePeriods, const QnTimePeriodList &appendingPeriods);

    static QnTimePeriodList aggregateTimePeriodsUnconstrained(
        const QnTimePeriodList& periods,
        std::chrono::milliseconds detailLevel);

    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
    static QnTimePeriodList aggregateTimePeriods(
        const QnTimePeriodList& periods,
        std::chrono::milliseconds detailLevel);

    static QnTimePeriodList intersection(
        const QnTimePeriodList& first, const QnTimePeriodList& second);

    /**
     * Include a period to the period list
     *
     * \param[in] period                A period to be included.
     *
     * The function includes the given period to the list.
     * If the period overlaps the other periods, all of them will be merged.
     * If the period list is unsorted or contains overlapping periods
     * the result is undefined.
     */
    void includeTimePeriod(const QnTimePeriod &period);

    /**
     * Exclude a period to the period list
     *
     * \param[in] period                A period to be excluded.
     *
     * The function excludes the given period from the list.
     * If the period overlaps the other periods, intersected pieces or whole periods will be removed.
     * If the period list is unsorted or contains overlapping periods
     * the result is undefined.
     */
    void excludeTimePeriod(const QnTimePeriod& period);

    /**
     * Exclude a periods. This functions is the same as operator-.
     *
     * \param[in] periodList    A period list to be excluded.
     *
     * The function excludes the given period list from the source list.
     * If the period list overlaps the other periods, intersected pieces or whole periods will be removed.
     * If the source or destination period list is unsorted or contains overlapping periods
     * the result is undefined.
     */
    void excludeTimePeriods(const QnTimePeriodList& periodList);

    /**
     * Converts any period list to a simple list: sorted and containing only non-overlapped periods.
     */
    QnTimePeriodList simplified() const;
};

template<typename Container>
QnTimePeriodList timePeriodListFromRange(
    const Container& container,
    int64_t startTimeMs, int64_t endTimeMs,
    // Skip periods less than that or merge adjacent if the gap between them is less than this
    // value.
    int64_t detailLevelMs,
    bool keepSmallChunks, int limit,
    Qt::SortOrder sortOrder,
    bool excludeInfinite = false)
{
    QnTimePeriodList result;
    if (!NX_ASSERT(startTimeMs < endTimeMs))
        return result;

    // Searching for the chunk range bounds.
    auto itr = std::lower_bound(container.cbegin(), container.cend(), startTimeMs);
    /* Checking if we should include a chunk, containing startTime. */
    if (itr != container.cbegin())
    {
        --itr;
        if (itr->endTimeMs() <= startTimeMs)
            ++itr; //< Case if previous chunk does not contain startTime.
    }

    auto endItr = std::lower_bound(container.begin(), container.end(), endTimeMs);
    if (itr == endItr || itr->startTimeMs >= endTimeMs)
        return result;

    // The chunk range is determined. Building  a time period list from it by merging chunks and
    // skipping small ones if needed.
    if (itr->isInfinite() && excludeInfinite)
        return result;

    result.push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
    for (++itr; itr != endItr; ++itr)
    {
        QnTimePeriod& last = result.last();
        if (last.isInfinite())
            break;

        if (itr->isInfinite() && excludeInfinite)
            break;

        if (itr->startTimeMs - last.endTimeMs() < detailLevelMs)
        {
            last.durationMs = itr->isInfinite()
                ? QnTimePeriod::kInfiniteDuration
                : qMax(last.durationMs, itr->endTimeMs() - last.startTimeMs);
        }
        else
        {
            if (last.durationMs < detailLevelMs && !keepSmallChunks)
                result.pop_back();

            result.push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
        }
    }

    if (!result.last().isInfinite() && result.last().durationMs < detailLevelMs && !keepSmallChunks)
        result.pop_back();

    if (sortOrder == Qt::DescendingOrder)
        std::reverse(result.begin(), result.end());

    while ((int) result.size() > limit)
        result.pop_back();

    return result;
}

struct NX_VMS_COMMON_API MultiServerPeriodData
{
    /** Guid of the server, owning this periods. */
    QnUuid guid;
    QnTimePeriodList periods;

    bool operator==(const MultiServerPeriodData& other) const = default;
};

typedef std::vector<MultiServerPeriodData> MultiServerPeriodDataList;

QN_FUSION_DECLARE_FUNCTIONS(MultiServerPeriodData,
    (json)(metatype)(ubjson)(xml)(csv_record)(compressed_time),
    NX_VMS_COMMON_API);

Q_DECLARE_METATYPE(QnTimePeriodList);
Q_DECLARE_METATYPE(MultiServerPeriodDataList);


#endif // QN_TIME_PERIOD_LIST_H
