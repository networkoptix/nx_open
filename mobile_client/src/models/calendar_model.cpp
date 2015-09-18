#include "calendar_model.h"

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
            return QDateTime(date, QTime(0, 0, 0));
        }
    };
}

class QnCalendarModelPrivate {
public:
    QnCalendarModelPrivate(QnCalendarModel *parent) :
        parent(parent),
        populated(false),
        mondayIsFirstDay(true),
        chunkProvider(nullptr)
    {
        QDate date = QDate::currentDate();
        year = date.year();
        month = date.month();
        populate();
    }

    QnCalendarModel *parent;

    bool populated;

    QList<CalendarDay> days;
    int year;
    int month;

    bool mondayIsFirstDay;

    QnCameraChunkProvider *chunkProvider;

    QDate firstDate(int year, int month) const;
    int indexOf(const QDate &date) const;

    void populate();
    void updateArchiveInfo();
};

QnCalendarModel::QnCalendarModel(QObject *parent) :
    QAbstractListModel(parent),
    d(new QnCalendarModelPrivate(this))
{
}

QnCalendarModel::~QnCalendarModel() {
}

int QnCalendarModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return d->days.size();
}

QVariant QnCalendarModel::data(const QModelIndex &index, int role) const {
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return QVariant();

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
    return d->year;
}

void QnCalendarModel::setYear(int year) {
    if (d->year == year)
        return;

    beginResetModel();

    d->year = year;
    d->populate();

    endResetModel();

    emit yearChanged();
}

int QnCalendarModel::month() const {
    return d->month;
}

void QnCalendarModel::setMonth(int month) {
    if (d->month == month)
        return;

    beginResetModel();

    d->month = month;
    d->populate();

    endResetModel();

    emit monthChanged();
}

bool QnCalendarModel::mondayIsFirstDay() const {
    return d->mondayIsFirstDay;
}

void QnCalendarModel::setMondayIsFirstDay(bool mondayIsFirstDay) {
    if (d->mondayIsFirstDay == mondayIsFirstDay)
        return;

    beginResetModel();

    d->mondayIsFirstDay = mondayIsFirstDay;

    d->populate();

    endResetModel();

    emit mondayIsFirstDayChanged();
}

QnCameraChunkProvider *QnCalendarModel::chunkProvider() const {
    return d->chunkProvider;
}

void QnCalendarModel::setChunkProvider(QnCameraChunkProvider *chunkProvider) {
    if (d->chunkProvider == chunkProvider)
        return;

    if (d->chunkProvider)
        disconnect(d->chunkProvider, nullptr, this, nullptr);

    d->chunkProvider = chunkProvider;

    if (d->chunkProvider)
        connect(d->chunkProvider, &QnCameraChunkProvider::timePeriodsUpdated, this, [this](){ d->updateArchiveInfo(); });

    d->updateArchiveInfo();
    emit dataChanged(index(0), index(rowCount()), QVector<int>() << HasArchiveRole);

    emit chunkProviderChanged();
}

QDate QnCalendarModelPrivate::firstDate(int year, int month) const {
    QDate date(year, month, 1);
    int dayOfWeek = date.dayOfWeek();
    if (mondayIsFirstDay)
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

    QnTimePeriodList timePeriods = chunkProvider->timePeriods();

    int i = indexOf(QDate(year, month, 1));
    while (i < days.size()) {
        auto it = timePeriods.findNearestPeriod(days[i].toDateTime().toMSecsSinceEpoch(), true);

        if (it == timePeriods.constEnd()) {
            while (i < days.size())
                days[i++].hasArchive = false;
            break;
        }

        QDate startDate = QDateTime::fromMSecsSinceEpoch(it->startTimeMs).date();
        QDate endDate = it->isInfinite() ? QDateTime::currentDateTime().date() : QDateTime::fromMSecsSinceEpoch(it->endTimeMs()).date();

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
