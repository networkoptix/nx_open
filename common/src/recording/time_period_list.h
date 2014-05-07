#ifndef QN_TIME_PERIOD_LIST_H
#define QN_TIME_PERIOD_LIST_H

#include <vector>

#include <QtCore/QVector>

#include "time_period.h"

class QnTimePeriodListTimeIterator;

/**
 * A sorted list of time periods that basically is an implementation of 
 * an interval container concept.
 */
class QnTimePeriodList: public QVector<QnTimePeriod> {
public:
    QnTimePeriodList(): QVector<QnTimePeriod>() {}
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

    static QnTimePeriodList mergeTimePeriods(const QVector<QnTimePeriodList>& periods);
    static QnTimePeriodList aggregateTimePeriods(const QnTimePeriodList& periods, int detailLevelMs);
};



/**
 * Iterator that presents a view into time period list as a list of time values
 * that this list contains. This allows for duration-based iteration through
 * the time period list.
 */
class QnTimePeriodListTimeIterator {
public:
    typedef QnTimePeriodListTimeIterator        this_type;
    typedef QnTimePeriodList::const_iterator    base_type;

    /* We don't really satisfy the requirements of a random access iterator, 
     * but we implement the interface. */
    typedef std::random_access_iterator_tag     iterator_category; 

    typedef qint64                              value_type;
    typedef qint64                              difference_type;
    typedef const qint64 *                      pointer;
    typedef qint64                              reference;

    QnTimePeriodListTimeIterator(): m_position(0) {}
    QnTimePeriodListTimeIterator(const base_type &base, qint64 position): m_base(base), m_position(0) { *this += position; }
    QnTimePeriodListTimeIterator(const this_type &other): m_base(other.m_base), m_position(other.m_position) {}

    reference operator*() const { return m_base->startTimeMs + m_position; }
    reference operator[](difference_type d) const { return *(*this + d); }

    this_type operator+(difference_type d) const { this_type result = *this; result += d; return result; }
    this_type &operator++() { return *this += 1; }
    this_type operator++(int) { this_type result = *this; ++*this; return result; }

    this_type operator-(difference_type d) const { this_type result = *this; result -= d; return result; }
    this_type &operator--() { return *this -= 1;}
    this_type operator--(int) { this_type result = *this; --*this; return result; }
    this_type &operator-=(difference_type d) { return *this += -d; }

    this_type &operator+=(difference_type d) {
        m_position += d;

        if(d <= 0) {
            /* It is important that (d == 0) case is processed here
             * as we don't want base iterator to be dereferenced in this case. */
            while(m_position < 0) {
                m_base--;
                m_position += m_base->durationMs;
            }
        } else {
            while(m_position > m_base->durationMs && m_base->durationMs != -1) {
                m_position -= m_base->durationMs;
                m_base++;
            }
        }

        return *this;
    }

    bool operator==(const this_type &other) const { return m_base == other.m_base && m_position == other.m_position; }
    bool operator!=(const this_type &other) const { return !(*this == other); }

    bool operator<(const this_type &other) const { return m_base < other.m_base || (m_base == other.m_base && m_position < other.m_position); }
    bool operator<=(const this_type &other) const { return m_base < other.m_base || (m_base == other.m_base && m_position <= other.m_position); }
    bool operator>(const this_type &other) const { return !(*this <= other); }
    bool operator>=(const this_type &other) const { return !(*this < other); }

private:
    /** Position in the base time period list. */
    base_type m_base;

    /** Position inside the current time period. */
    qint64 m_position;
};

inline QnTimePeriodListTimeIterator QnTimePeriodList::timeBegin() const {
    return QnTimePeriodListTimeIterator(begin(), 0);
}

inline QnTimePeriodListTimeIterator QnTimePeriodList::timeEnd() const {
    return QnTimePeriodListTimeIterator(end(), 0);
}

Q_DECLARE_METATYPE(QnTimePeriodList);

#endif // QN_TIME_PERIOD_LIST_H