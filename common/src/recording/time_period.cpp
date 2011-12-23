#include "time_period.h"
#include "utils/common/util.h"

bool operator < (const QnTimePeriod& first, const QnTimePeriod& other) 
{
    return first.startTimeMs < other.startTimeMs;
}

bool operator < (qint64 first, const QnTimePeriod& other) 
{ 
    return first < other.startTimeMs; 
}

bool operator < (const QnTimePeriod& other, qint64 first) 
{ 
    return other.startTimeMs < first;
}

void QnTimePeriod::clear()
{
    startTimeMs = 0;
    durationMs = 0;
}

bool QnTimePeriod::containPeriod(const QnTimePeriod& timePeriod) const
{
    return startTimeMs <= timePeriod.startTimeMs && (startTimeMs + durationMs >= timePeriod.startTimeMs + timePeriod.durationMs);
}

bool QnTimePeriod::containTime(qint64 timeMs) const
{
    return qnBetween(timeMs, startTimeMs, startTimeMs+durationMs);
}


void QnTimePeriod::addPeriod(const QnTimePeriod& timePeriod)
{
    qint64 endPoint1 = startTimeMs + durationMs;
    qint64 endPoint2 = timePeriod.startTimeMs + timePeriod.durationMs;
    startTimeMs = qMin(startTimeMs, timePeriod.startTimeMs);
    durationMs = qMax(endPoint1, endPoint2) - startTimeMs;
}

QnTimePeriodList QnTimePeriod::mergeTimePeriods(QVector<QnTimePeriodList> periods)
{
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        for (int i = 0; i < periods.size(); ++i) {
            if (!periods[i].isEmpty() && periods[i][0].startTimeMs < minStartTime) {
                minIndex = i;
                minStartTime = periods[i][0].startTimeMs;
            }
        }

        if (minIndex >= 0)
        {
            // add chunk to merged data
            if (result.isEmpty()) {
                result << periods[minIndex][0];
            }
            else {
                QnTimePeriod& last = result.last();
                if (last.startTimeMs <= minStartTime && last.startTimeMs+last.durationMs >= minStartTime && periods[minIndex][0].durationMs != -1)
                    last.durationMs = qMax(last.durationMs, minStartTime + periods[minIndex][0].durationMs - last.startTimeMs);
                else {
                    result << periods[minIndex][0];
                    if (periods[minIndex][0].durationMs == -1)
                        break;
                }
            } 
            periods[minIndex].pop_front();
        }
    }
    return result;
}
