#include "calendar_model.h"

#include <QtCore/QLocale>
#include <QtCore/QTimeZone>

#include "camera/camera_chunk_provider.h"

namespace {
    struct CalendarDay {
        QDate date;
        bool hasArchive;

        CalendarDay(const QDate &date) :
            date(date),
            hasArchive(false)
        {
        }

        QDateTime toDateTime() const {
            return QDateTime(date, QTime(0, 0, 0), Qt::UTC);
        }
    };

QDate utcDateFromMilliseconds(qint64 timeMs)
{
    return QDateTime::fromMSecsSinceEpoch(timeMs, Qt::UTC).date();
}

}

class QnCalendarModelPrivate {
    Q_DECLARE_PUBLIC(QnCalendarModel)
public:
    QnCalendarModelPrivate(QnCalendarModel *parent) :
        q_ptr(parent),
        populated(false),
        chunkProvider(nullptr)
    {
        QDate date = QDate::currentDate();
        year = date.year();
        month = date.month();
        populate();
    }

    QnCalendarModel *q_ptr;

    bool populated;

    QList<CalendarDay> days;
    int year;
    int month;

    QLocale locale;

    QnCameraChunkProvider *chunkProvider;

    QDate firstDate(int year, int month) const;
    int indexOf(const QDate &date) const;

    void populate();
    void updateArchiveInfo();
};

QnCalendarModel::QnCalendarModel(QObject *parent) :
    QAbstractListModel(parent),
    d_ptr(new QnCalendarModelPrivate(this))
{
}

QnCalendarModel::~QnCalendarModel() {
}

int QnCalendarModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    Q_D(const QnCalendarModel);
    return d->days.size();
}

QVariant QnCalendarModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

    Q_D(const QnCalendarModel);

    const CalendarDay &day = d->days[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        return day.date.month() == d->month ? QString::number(day.date.day()) : QString();
    case DayRole:
        return day.date.day();
    case DateRole:
        return day.date;
    case HasArchiveRole:
        return day.hasArchive;
    }

    return QVariant();
}

QHash<int, QByteArray> QnCalendarModel::roleNames() const {
    QHash<int, QByteArray> roleNames = QAbstractListModel::roleNames();
    roleNames[DayRole] = "day";
    roleNames[DateRole] = "date";
    roleNames[HasArchiveRole] = "hasArchive";
    return roleNames;
}

int QnCalendarModel::year() const {
    Q_D(const QnCalendarModel);
    return d->year;
}

void QnCalendarModel::setYear(int year) {
    Q_D(QnCalendarModel);

    if (d->year == year)
        return;

    beginResetModel();

    d->year = year;
    d->populate();

    endResetModel();

    emit yearChanged();
}

int QnCalendarModel::month() const {
    Q_D(const QnCalendarModel);
    return d->month;
}

void QnCalendarModel::setMonth(int month) {
    Q_D(QnCalendarModel);

    if (d->month == month)
        return;

    beginResetModel();

    d->month = month;
    d->populate();

    endResetModel();

    emit monthChanged();
}

QnCameraChunkProvider *QnCalendarModel::chunkProvider() const {
    Q_D(const QnCalendarModel);
    return d->chunkProvider;
}

bool QnCalendarModel::isCurrent(const QDateTime& value, int dayIndex)
{
    Q_D(QnCalendarModel);
    return value.date() == d->days[dayIndex].date;
}

void QnCalendarModel::setChunkProvider(QnCameraChunkProvider *chunkProvider) {
    Q_D(QnCalendarModel);

    if (d->chunkProvider == chunkProvider)
        return;

    if (d->chunkProvider)
        disconnect(d->chunkProvider, nullptr, this, nullptr);

    d->chunkProvider = chunkProvider;

    if (d->chunkProvider) {
        connect(d->chunkProvider, &QnCameraChunkProvider::timePeriodsUpdated, this, [this, d](){
            beginResetModel();
            d->updateArchiveInfo();
            endResetModel();
            // emit dataChanged(index(0), index(rowCount()), QVector<int>() << HasArchiveRole);
        });
    }

    /* For some reason QML does not react to model changes :(
     * So we have to reset model. */
    beginResetModel();
    d->updateArchiveInfo();
    endResetModel();
    // emit dataChanged(index(0), index(rowCount()), QVector<int>() << HasArchiveRole);

    emit chunkProviderChanged();
}

QLocale QnCalendarModel::locale() const
{
    Q_D(const QnCalendarModel);
    return d->locale;
}

void QnCalendarModel::setLocale(QLocale locale)
{
    Q_D(QnCalendarModel);

    if (d->locale == locale)
        return;

    beginResetModel();

    d->locale = locale;

    d->populate();

    endResetModel();

    emit localeChanged();
}

QDate QnCalendarModelPrivate::firstDate(int year, int month) const {
    QDate date(year, month, 1);
    int dayOfWeek = date.dayOfWeek();
    if (locale.firstDayOfWeek() == Qt::Monday)
        date = date.addDays(1 - dayOfWeek);
    else if (dayOfWeek < Qt::Sunday)
        date = date.addDays(-dayOfWeek);
    return date;
}

int QnCalendarModelPrivate::indexOf(const QDate &date) const {
    for (int i = 0; i < days.size(); ++i) {
        if (days[i].date == date)
            return i;
    }
    return -1;
}

void QnCalendarModelPrivate::populate() {
    days.clear();

    QDate date = firstDate(year, month);

    if (!date.isValid())
        return;

    /* first row will contain the first day of this month */
    for (int day = 0; day < 7; ++day) {
        days.append(CalendarDay(date));
        date = date.addDays(1);
    }

    /* add weeks till month ended */
    while (date.month() == month) {
        days.append(CalendarDay(date));
        date = date.addDays(1);
    }

    updateArchiveInfo();
}

void QnCalendarModelPrivate::updateArchiveInfo() {
    if (!chunkProvider) {
        for (CalendarDay &day : days)
            day.hasArchive = false;

        return;
    }

    QnTimePeriodList timePeriods = chunkProvider->timePeriods(Qn::RecordingContent);

    int i = indexOf(QDate(year, month, 1));
    while (i < days.size()) {
        auto it = timePeriods.findNearestPeriod(days[i].toDateTime().toMSecsSinceEpoch(), true);

        if (it == timePeriods.constEnd()) {
            while (i < days.size())
                days[i++].hasArchive = false;
            break;
        }

        const auto startDate = utcDateFromMilliseconds(it->startTimeMs);
        const auto endDate = it->isInfinite()
            ? QDateTime::currentDateTimeUtc().date()
            : utcDateFromMilliseconds(it->endTimeMs());

        while (i < days.size() && days[i].date < startDate)
            days[i++].hasArchive = false;

        while (i < days.size() && days[i].date <= endDate)
            days[i++].hasArchive = true;

        if (it->isInfinite()) {
            while (i < days.size())
                days[i++].hasArchive = false;
        }
    }
}
