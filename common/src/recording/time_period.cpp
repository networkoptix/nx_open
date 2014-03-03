#include "time_period.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>

#include <utils/math/math.h>
#include <utils/common/util.h>
#include <utils/common/json.h>

#include "time_period_list.h"

namespace detail {
    QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnTimePeriod, (startTimeMs)(durationMs), static)
}


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

qint64 QnTimePeriod::endTimeMs() const
{
    if (durationMs == -1)
        return DATETIME_NOW;
    else
        return startTimeMs + durationMs;
}

bool QnTimePeriod::contains(const QnTimePeriod &timePeriod) const
{
    return startTimeMs <= timePeriod.startTimeMs && (startTimeMs + durationMs >= timePeriod.startTimeMs + timePeriod.durationMs);
}

bool QnTimePeriod::contains(qint64 timeMs) const
{
    return qBetween(startTimeMs, timeMs, durationMs != -1 ? startTimeMs + durationMs : DATETIME_NOW);
}

void QnTimePeriod::addPeriod(const QnTimePeriod &timePeriod)
{
    qint64 endPoint1 = startTimeMs + durationMs;
    qint64 endPoint2 = timePeriod.startTimeMs + timePeriod.durationMs;

    startTimeMs = qMin(startTimeMs, timePeriod.startTimeMs);
    if (durationMs == -1 || timePeriod.durationMs == -1)
        durationMs = -1;
    else
        durationMs = qMax(endPoint1, endPoint2) - startTimeMs;
}

QnTimePeriod QnTimePeriod::intersected(const QnTimePeriod &other) const
{
    if (durationMs == -1 && other.durationMs == -1)
        return QnTimePeriod(qMax(startTimeMs, other.startTimeMs), -1);

    if (durationMs == -1) {
        if (startTimeMs > other.startTimeMs + other.durationMs)
            return QnTimePeriod();
        if (startTimeMs < other.startTimeMs)
            return QnTimePeriod(other.startTimeMs, other.durationMs);
        return QnTimePeriod(startTimeMs, other.durationMs - (startTimeMs - other.startTimeMs));
    }

    if (other.durationMs == -1) {
        if (other.startTimeMs > startTimeMs + durationMs)
            return QnTimePeriod();
        if (other.startTimeMs < startTimeMs)
            return QnTimePeriod(startTimeMs, durationMs);
        return QnTimePeriod(other.startTimeMs, durationMs - (other.startTimeMs - startTimeMs));
    }

    if (other.startTimeMs > startTimeMs + durationMs || startTimeMs > other.startTimeMs + other.durationMs)
        return QnTimePeriod();

    qint64 start = qMax(startTimeMs, other.startTimeMs);
    qint64 end = qMin(startTimeMs + durationMs, other.startTimeMs + other.durationMs);
    return QnTimePeriod(start, end - start);
}

bool QnTimePeriod::intersects(const QnTimePeriod &other) const 
{
    return !intersected(other).isEmpty();
}

QnTimePeriodList QnTimePeriod::aggregateTimePeriods(const QnTimePeriodList &periods, int detailLevelMs)
{
    QnTimePeriodList result;
    if (periods.isEmpty())
        return result;
    result << periods[0];

    for (int i = 1; i < periods.size(); ++i)
    {
        QnTimePeriod& last = result.last();
        if(last.durationMs == -1)
            break;

        if (last.startTimeMs + last.durationMs + detailLevelMs > periods[i].startTimeMs) {
            if(periods[i].durationMs == -1) {
                last.durationMs = -1;
            } else {
                last.durationMs = qMax(last.durationMs, periods[i].startTimeMs + periods[i].durationMs - last.startTimeMs);
            }
        } else {
            result << periods[i];
        }
    }

    return result;
}

QnTimePeriodList QnTimePeriod::mergeTimePeriods(const QVector<QnTimePeriodList>& periods)
{
    if(periods.size() == 1)
        return periods[0];

    QVector<int> minIndexes;
    minIndexes.resize(periods.size());
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        for (int i = 0; i < periods.size(); ++i) 
        {
            int startIdx = minIndexes[i];
            if (startIdx < periods[i].size() && periods[i][startIdx].startTimeMs < minStartTime) {
                minIndex = i;
                minStartTime = periods[i][startIdx].startTimeMs;
            }
        }

        if (minIndex >= 0)
        {
            int& startIdx = minIndexes[minIndex];
            // add chunk to merged data
            if (result.isEmpty()) {
                result << periods[minIndex][startIdx];
            }
            else {
                QnTimePeriod& last = result.last();
                if (periods[minIndex][startIdx].durationMs == -1) 
                {
                    if (last.durationMs == -1)
                        last.startTimeMs = qMin(last.startTimeMs, periods[minIndex][startIdx].startTimeMs);
                    else if (periods[minIndex][startIdx].startTimeMs > last.startTimeMs+last.durationMs)
                        result << periods[minIndex][startIdx];
                    else 
                        last.durationMs = -1;
                    break;
                }
                else if (last.startTimeMs <= minStartTime && last.startTimeMs+last.durationMs >= minStartTime)
                    last.durationMs = qMax(last.durationMs, minStartTime + periods[minIndex][startIdx].durationMs - last.startTimeMs);
                else {
                    result << periods[minIndex][startIdx];
                }
            } 
            startIdx++;
        }
    }
    return result;
}

QByteArray QnTimePeriod::serialize() const
{
    QByteArray rez;
    qint64 val = htonll(startTimeMs);
    rez.append((const char*) &val, sizeof(val));
    val = htonll(durationMs);
    rez.append((const char*) &val, sizeof(val));
    return rez;
}

QnTimePeriod& QnTimePeriod::deserialize(const QByteArray& data)
{
    if (data.size() == 16)
    {
        quint64* val = (quint64*) data.data();
        startTimeMs = ntohll(val[0]);
        durationMs = ntohll(val[1]);
    }
    return *this;
}

bool QnTimePeriod::operator==(const QnTimePeriod &other) const
{
    return startTimeMs == other.startTimeMs && durationMs == other.durationMs;
}

QDebug operator<<(QDebug dbg, const QnTimePeriod &period) {
    if (period.durationMs >= 0)
        dbg.nospace() << "QnTimePeriod(" << QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(lit("dd hh:mm"))
                      << " - " << QDateTime::fromMSecsSinceEpoch(period.startTimeMs + period.durationMs).toString(lit("dd hh:mm")) << ')';
    else
        dbg.nospace() << "QnTimePeriod(" << QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(lit("dd hh:mm"))
                      << " - Now)";
    return dbg.space();
}

void serialize(QnJsonContext *ctx, const QnTimePeriod &value, QJsonValue *target) {
    detail::serialize(ctx, value, target);
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnTimePeriod *target) {
    if(value.type() == QJsonValue::Null) {
        *target = QnTimePeriod();
        return false;
    }
    return detail::deserialize(ctx, value, target);
}
