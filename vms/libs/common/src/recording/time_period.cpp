#include "time_period.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QtEndian>

#include <utils/math/math.h>
#include <utils/common/util.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/datetime.h>

QN_FUSION_ADAPT_STRUCT(QnTimePeriod, (startTimeMs)(durationMs))
QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((QnTimePeriod), (ubjson)(xml)(csv_record))

namespace detail {

QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES((QnTimePeriod), (json))

} // namespace detail

static_assert(QnTimePeriod::kMaxTimeValue == DATETIME_NOW);

void QnTimePeriod::clear()
{
    startTimeMs = kMinTimeValue;
    durationMs = kMinTimeValue;
}

qint64 QnTimePeriod::endTimeMs() const
{
    if (isInfinite())
        return kMaxTimeValue;

    return startTimeMs + durationMs;
}

void QnTimePeriod::setEndTimeMs(qint64 value)
{
    durationMs = value != kMaxTimeValue
        ? std::max(kMinTimeValue, value - startTimeMs)
        : kInfiniteDuration;
}

bool QnTimePeriod::isLeftIntersection(const QnTimePeriod& other) const
{
    return other.startTimeMs < startTimeMs
        && other.endTimeMs() > startTimeMs
        && other.endTimeMs() < endTimeMs();
}

bool QnTimePeriod::isRightIntersection(const QnTimePeriod& other) const
{
    return other.startTimeMs > startTimeMs
        && other.startTimeMs < endTimeMs()
        && other.endTimeMs() > endTimeMs();
}

bool QnTimePeriod::isContainedIn(const QnTimePeriod& other) const
{
    return startTimeMs >= other.startTimeMs && endTimeMs() <= other.endTimeMs();
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

    if (isNull())
    {
        startTimeMs = timePeriod.startTimeMs;
        durationMs = timePeriod.durationMs;
        return;
    }

    // End time must be calculated before start time is changed but applied after that.
    const auto endpoint = std::max(endTimeMs(), timePeriod.endTimeMs());
    startTimeMs = std::min(startTimeMs, timePeriod.startTimeMs);
    setEndTimeMs(endpoint);
}

QnTimePeriod QnTimePeriod::intersected(const QnTimePeriod &other) const
{
    qint64 start = std::max(startTimeMs, other.startTimeMs);
    qint64 end = std::min(endTimeMs(), other.endTimeMs());

    if (start > end)
        return QnTimePeriod();

    if (end == kMaxTimeValue)
        return QnTimePeriod(start, kInfiniteDuration);

    return QnTimePeriod(start, end - start);
}

bool QnTimePeriod::intersects(const QnTimePeriod &other) const
{
    return !intersected(other).isEmpty();
}

QByteArray QnTimePeriod::serialize() const
{
    QByteArray rez;
    qint64 val = qToBigEndian(startTimeMs);
    rez.append((const char*) &val, sizeof(val));
    val = qToBigEndian(durationMs);
    rez.append((const char*) &val, sizeof(val));
    return rez;
}

QnTimePeriod& QnTimePeriod::deserialize(const QByteArray& data)
{
    if (data.size() == 16)
    {
        auto val = (qint64*) data.data();
        startTimeMs = qFromBigEndian(val[0]);
        durationMs = qFromBigEndian(val[1]);
    }
    return *this;
}

void QnTimePeriod::setStartTime(std::chrono::milliseconds value)
{
    startTimeMs = value.count();
}

std::chrono::milliseconds QnTimePeriod::startTime() const
{
    return std::chrono::milliseconds(startTimeMs);
}

void QnTimePeriod::setEndTime(std::chrono::milliseconds value)
{
    setEndTimeMs(value.count());
}

std::chrono::milliseconds QnTimePeriod::endTime() const
{
    return std::chrono::milliseconds(endTimeMs());
}

void QnTimePeriod::setDuration(std::chrono::milliseconds value)
{
    durationMs = value.count();
}

std::chrono::milliseconds QnTimePeriod::duration() const
{
    return std::chrono::milliseconds(durationMs);
}

qint64 QnTimePeriod::distanceToTime(qint64 timeMs) const
{
    if (timeMs >= startTimeMs)
        return durationMs == -1 ? 0 : std::max(0ll, timeMs - (startTimeMs+durationMs));
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
        if (durationMs != kInfiniteDuration)
            durationMs = endTimeMs() - timeMs;
        startTimeMs = timeMs;
    }
}

QnTimePeriod QnTimePeriod::truncated(qint64 timeMs) const
{
    if (qBetween(startTimeMs, timeMs, endTimeMs()))
    {
        return QnTimePeriod::fromInterval(
            startTimeMs,
            timeMs > endTimeMs() ? endTimeMs() : timeMs);
    }

    return *this;
}

QnTimePeriod QnTimePeriod::truncatedFront(qint64 timeMs) const
{
    if (qBetween(startTimeMs, timeMs, endTimeMs()))
    {
        return QnTimePeriod::fromInterval(
            timeMs < startTimeMs ?
            startTimeMs : timeMs, endTimeMs());
    }

    return *this;
}

qint64 QnTimePeriod::bound(qint64 timeMs) const
{
    return qBound(startTimeMs, timeMs, endTimeMs() - 1);
}

void PrintTo(const QnTimePeriod& period, ::std::ostream* os)
{
    const QString fmt = "%1 - %2";
    QString result;
    if (!period.isInfinite())
        result = fmt.arg(period.startTimeMs).arg(period.endTimeMs());
    else
        result = fmt.arg(period.startTimeMs).arg("Inf");
    *os << result.toStdString();
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
