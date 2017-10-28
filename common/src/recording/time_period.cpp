#include "time_period.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>

#include <utils/math/math.h>
#include <utils/common/util.h>

#include <nx/fusion/fusion/fusion_adaptor.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/socket_common.h>
#include <nx/utils/datetime.h>

#include "time_period_list.h"

QN_FUSION_ADAPT_STRUCT(QnTimePeriod, (startTimeMs)(durationMs))
QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((QnTimePeriod), (ubjson)(xml)(csv_record))

namespace detail {
    QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((QnTimePeriod), (json))
}

namespace {
    const qint64 infiniteDuration = -1;
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

const qint64 QnTimePeriod::kMaxTimeValue = std::numeric_limits<qint64>::max();
const qint64 QnTimePeriod::kMinTimeValue = 0;

QnTimePeriod::QnTimePeriod() :
    startTimeMs(0),
    durationMs(0)
{}

QnTimePeriod::QnTimePeriod(qint64 startTimeMs, qint64 durationMs) :
    startTimeMs(startTimeMs),
    durationMs(durationMs)
{}

QnTimePeriod QnTimePeriod::fromInterval(qint64 startTimeMs
    , qint64 endTimeMs)
{
    NX_ASSERT(endTimeMs >= startTimeMs, Q_FUNC_INFO
        , "Start time could not be greater than end time");

    if (endTimeMs >= startTimeMs)
        return QnTimePeriod(startTimeMs, endTimeMs - startTimeMs);

    return QnTimePeriod();
}

void QnTimePeriod::clear()
{
    startTimeMs = 0;
    durationMs = 0;
}

qint64 QnTimePeriod::endTimeMs() const
{
    if (isInfinite())
        return DATETIME_NOW;

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
    if (timePeriod.isNull())
        return;

    if (isNull()) {
        startTimeMs = timePeriod.startTimeMs;
        durationMs = timePeriod.durationMs;
        return;
    }

    qint64 endPoint1 = startTimeMs + durationMs;
    qint64 endPoint2 = timePeriod.startTimeMs + timePeriod.durationMs;

    startTimeMs = qMin(startTimeMs, timePeriod.startTimeMs);
    if (durationMs == ::infiniteDuration || timePeriod.durationMs == ::infiniteDuration)
        durationMs = ::infiniteDuration;
    else
        durationMs = qMax(endPoint1, endPoint2) - startTimeMs;
}

QnTimePeriod QnTimePeriod::intersected(const QnTimePeriod &other) const
{
    if (isInfinite() && other.isInfinite())
        return QnTimePeriod(qMax(startTimeMs, other.startTimeMs), ::infiniteDuration);

    if (isInfinite()) {
        if (startTimeMs > other.startTimeMs + other.durationMs)
            return QnTimePeriod();
        if (startTimeMs < other.startTimeMs)
            return QnTimePeriod(other.startTimeMs, other.durationMs);
        return QnTimePeriod(startTimeMs, other.durationMs - (startTimeMs - other.startTimeMs));
    }

    if (other.isInfinite()) {
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

QnTimePeriod& QnTimePeriod::operator = (const QnTimePeriod &other)
{
    startTimeMs = other.startTimeMs;
    durationMs = other.durationMs;
    return *this;
}

bool QnTimePeriod::isNull() const {
    return startTimeMs == 0 && durationMs == 0;
}

bool QnTimePeriod::isInfinite() const {
    return durationMs == ::infiniteDuration;
}

qint64 QnTimePeriod::infiniteDuration() {
    return ::infiniteDuration;
}

bool QnTimePeriod::isEmpty() const {
    return durationMs == 0;
}

bool QnTimePeriod::isValid() const {
    return durationMs == ::infiniteDuration || durationMs > 0;
}

qint64 QnTimePeriod::distanceToTime(qint64 timeMs) const
{
    if (timeMs >= startTimeMs)
        return durationMs == -1 ? 0 : qMax(0ll, timeMs - (startTimeMs+durationMs));
    else
        return startTimeMs - timeMs;
}

void QnTimePeriod::truncate(qint64 timeMs)
{
    if (qBetween(startTimeMs, timeMs, endTimeMs()))
        durationMs = timeMs - startTimeMs;
}

void QnTimePeriod::truncateFront(qint64 timeMs)
{
    if (qBetween(startTimeMs, timeMs, endTimeMs()))
    {
        if (durationMs != infiniteDuration())
            durationMs = endTimeMs() - timeMs;
        startTimeMs = timeMs;
    }
}

QnTimePeriod QnTimePeriod::truncated(qint64 timeMs) const
{
    if (timeMs <= startTimeMs)
        return QnTimePeriod();

    return QnTimePeriod::fromInterval(
        startTimeMs,
        timeMs > endTimeMs() ? endTimeMs() : timeMs);
}

QnTimePeriod QnTimePeriod::truncatedFront(qint64 timeMs) const
{
    if (timeMs >= endTimeMs())
        return QnTimePeriod();

    return QnTimePeriod::fromInterval(
        timeMs < startTimeMs ?
        startTimeMs : timeMs,  endTimeMs());
}

bool operator==(const QnTimePeriod &first, const QnTimePeriod &other)
{
    return ((first.startTimeMs == other.startTimeMs)
        && (first.durationMs == other.durationMs));
}

QDebug operator<<(QDebug dbg, const QnTimePeriod &period) {
    const QString fmt = lit("dd MM yyyy hh:mm:ss");
    if (!period.isInfinite())
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
