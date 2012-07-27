#ifndef QN_TIME_PERIOD_LIST_H
#define QN_TIME_PERIOD_LIST_H

#include "time_period.h"

#include <QtCore/QVector>

class QnTimePeriodList: public QVector<QnTimePeriod>
{
public:
    QnTimePeriodList(): QVector<QnTimePeriod>() {}

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
     * \param period                    Period to check.
     * \returns                         Whether the given period intersects with
     *                                  any of the periods in this time period list.
     */
    bool intersects(const QnTimePeriod &period) const {        
        const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);
        
        const_iterator lastPos = findNearestPeriod(period.endTimeMs(), true);
        if(lastPos != end())
            lastPos++;

        for (const_iterator pos = firstPos; pos != lastPos; ++pos)
            if (!pos->intersected(period).isEmpty())
                return true;
        return false;
    }
};

Q_DECLARE_METATYPE(QnTimePeriodList);

#endif // QN_TIME_PERIOD_LIST_H