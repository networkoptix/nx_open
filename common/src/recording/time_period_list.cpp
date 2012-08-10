#include "time_period_list.h"

QnTimePeriodList::const_iterator QnTimePeriodList::findNearestPeriod(qint64 timeMs, bool searchForward) const {
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

bool QnTimePeriodList::intersects(const QnTimePeriod &period) const {        
    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if(lastPos != end())
        lastPos++;

    for (const_iterator pos = firstPos; pos != lastPos; ++pos)
        if (!pos->intersected(period).isEmpty())
            return true;
    return false;
}

QnTimePeriodList QnTimePeriodList::intersected(const QnTimePeriod &period) const {
    QnTimePeriodList result;

    const_iterator firstPos = findNearestPeriod(period.startTimeMs, true);

    const_iterator lastPos = findNearestPeriod(period.endTimeMs(), false);
    if(lastPos != end())
        lastPos++;

    for (const_iterator pos = firstPos; pos != lastPos; ++pos) {
        QnTimePeriod period = pos->intersected(period);
        if(!period.isEmpty())
            result.push_back(period);
    }

    return result;
}

qint64 QnTimePeriodList::duration() const {
    if(isEmpty())
        return 0;

    if(back().durationMs == -1)
        return -1;

    qint64 result = 0;
    foreach(const QnTimePeriod &period, *this)
        result += period.durationMs;
    return result;
}

