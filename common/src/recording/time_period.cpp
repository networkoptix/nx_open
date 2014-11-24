#include "time_period.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>

#include <utils/math/math.h>
#include <utils/common/util.h>
#include <utils/serialization/json_functions.h>
#include <utils/fusion/fusion_adaptor.h>

QN_FUSION_ADAPT_STRUCT(QnTimePeriod, (startTimeMs)(durationMs))

namespace detail {
    QN_FUSION_DEFINE_FUNCTIONS(QnTimePeriod, (json), static)
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
    return startTimeMs <= timePeriod.startTimeMs && endTimeMs() >= timePeriod.endTimeMs();
}

bool QnTimePeriod::contains(qint64 timeMs) const
{
    return qBetween(startTimeMs, timeMs, endTimeMs());
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
    const QString fmt = lit("hh:mm:ss");
    if (period.durationMs >= 0)
        dbg.nospace() << "" << QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(fmt)
                      << " - " << QDateTime::fromMSecsSinceEpoch(period.startTimeMs + period.durationMs).toString(fmt);
    else
        dbg.nospace() << QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(fmt)
                      << " - Now";
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
