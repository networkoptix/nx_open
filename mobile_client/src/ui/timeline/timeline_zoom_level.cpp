#include "timeline_zoom_level.h"

QVector<QString> QnTimelineZoomLevel::monthsNames;
int QnTimelineZoomLevel::maxMonthLength = 0;

bool QnTimelineZoomLevel::testTick(qint64 tick) const {
    if (isMonotonic())
        return tick % interval == 0;

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC);
    bool isStartOfDay = dateTime.time() == QTime(0, 0);
    if (!isStartOfDay)
        return false;

    QDate date = dateTime.date();
    if (date.day() != 1)
        return false;
    if (type == Months)
        return date.month() % interval == 0;

    if (dateTime.date().month() != 1)
        return false;

    return date.year() % interval == 0;
}

qint64 QnTimelineZoomLevel::nextTick(qint64 tick, int count) const {
    switch (type) {
    case Months:
        return QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC).addMonths(interval * count).toMSecsSinceEpoch();
    case Years:
        return QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC).addYears(interval * count).toMSecsSinceEpoch();
    default:
        return tick + interval * count;
    }
}

qint64 QnTimelineZoomLevel::alignTick(qint64 tick) const {
    if (isMonotonic())
        return tick - tick % interval;

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC);
    QDate date = dateTime.date();
    dateTime.setTime(QTime(0, 0));

    if (type == Months){
        dateTime.setDate(QDate(date.year(), date.month(), 1));
        return dateTime.toMSecsSinceEpoch();
    } else { // if (type == Years)
        dateTime.setDate(QDate(date.year(), 1, 1));
        return dateTime.toMSecsSinceEpoch();
    }
}

int QnTimelineZoomLevel::tickCount(qint64 start, qint64 end) const {
    if (start >= end)
        return 1;

    if (isMonotonic())
        return qMax(static_cast<int>((end - start) / interval), 1);

    QDate startDate = QDateTime::fromMSecsSinceEpoch(start, Qt::UTC).date();
    QDate endDate = QDateTime::fromMSecsSinceEpoch(end, Qt::UTC).date();

    if (type == Months)
        return qMax((12 - startDate.month()) + endDate.month() + 12 * (endDate.year() - startDate.year() - 1) - 1, 1);
    else // if (type == Years)
        return qMax((endDate.year() - startDate.year()) / static_cast<int>(interval), 1);
}

bool QnTimelineZoomLevel::isMonotonic() const {
    return type <= Days;
}


QString QnTimelineZoomLevel::baseValue(qint64 tick) const {
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC);
    switch (type) {
    case Milliseconds:
        return dateTime.toString(lit("z"));
    case Seconds:
        return dateTime.toString(lit("s"));
    case Minutes:
        return dateTime.toString(lit("h"));
    case Hours:
        return dateTime.toString(lit("h"));
    case Days:
        return dateTime.toString(lit("d"));
    case Months:
        return QString();
    case Years:
        return dateTime.toString(lit("yyyy"));
    }
    return QString();
}

QString QnTimelineZoomLevel::subValue(qint64 tick) const {
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC);
    switch (type) {
    case Minutes:
        return dateTime.toString(lit("mm"));
    case Hours:
        return dateTime.toString(lit("mm"));
    default:
        return QString();
    }
    return QString();
}

QString QnTimelineZoomLevel::suffix(qint64 tick) const {
    if (!suffixOverride.isEmpty())
        return suffixOverride;

    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(tick, Qt::UTC);
    switch (type) {
    case Days:
        return dateTime.toString(lit("MMMM"));
    case Months:
        return monthsNames[dateTime.date().month() - 1];
    case Milliseconds:
        return lit("ms");
    case Seconds:
        return lit("s");
    case Minutes:
    case Hours:
        return lit(":");
    default:
        return QString();
    }
}

int QnTimelineZoomLevel::maxTextWidth() const {
    switch (type) {
    case Milliseconds:
        return 6;
    case Seconds:
        return 4;
    case Minutes:
        return 5;
    case Hours:
        return 5;
    case Days:
        return 3 + maxMonthLength;
    case Months:
        return maxMonthLength;
    case Years:
        return 4;
    }
    return 0;
}
