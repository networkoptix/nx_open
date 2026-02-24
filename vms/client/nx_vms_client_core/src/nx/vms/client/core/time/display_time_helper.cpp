// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "display_time_helper.h"

#include <chrono>

#include <QtQml/QtQml>

#include <nx/vms/client/core/time/calendar_utils.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::core {

struct DisplayTimeHelper::Private
{
    Private(DisplayTimeHelper* owner);
    QDateTime getDateTime() const;
    void handleChanges();

    DisplayTimeHelper* const q;
    qint64 position = 0;
    QTimeZone timeZone{QTimeZone::LocalTime};
    QDateTime dateTime;
    nx::vms::time::FormatterPtr formatter;
};

DisplayTimeHelper::Private::Private(DisplayTimeHelper* owner):
    q(owner),
    position(0),
    timeZone(QTimeZone::LocalTime),
    dateTime(getDateTime()),
    formatter(nx::vms::time::Formatter::system())
{
}

QDateTime DisplayTimeHelper::Private::getDateTime() const
{
    return QDateTime::fromMSecsSinceEpoch(position, timeZone);
}

void DisplayTimeHelper::Private::handleChanges()
{
    dateTime = getDateTime();
    q->dateTimeChanged();
}

//--------------------------------------------------------------------------------------------------

void DisplayTimeHelper::registerQmlType()
{
    qmlRegisterType<DisplayTimeHelper>("Nx.Utils", 1, 0, "DisplayTimeHelper");
}

DisplayTimeHelper::DisplayTimeHelper(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    const auto updateDateTime = [this]() { d->handleChanges(); };
    connect(this, &DisplayTimeHelper::positionChanged, this, updateDateTime);
    connect(this, &DisplayTimeHelper::timeZoneChanged, this, updateDateTime);
    connect(this, &DisplayTimeHelper::localeChanged, this, updateDateTime);
    connect(this, &DisplayTimeHelper::is24HoursTimeFormatChanged, this, updateDateTime);
}

DisplayTimeHelper::~DisplayTimeHelper()
{
}

void DisplayTimeHelper::setPosition(qint64 value)
{
    value = std::clamp<qint64>(value, 0, std::numeric_limits<qint64>().max());
    if (d->position == value)
        return;

    d->position = value;
    emit positionChanged();
}

qint64 DisplayTimeHelper::position() const
{
    return d->position;
}

QTimeZone DisplayTimeHelper::timeZone() const
{
    return d->timeZone;
}

void DisplayTimeHelper::setTimeZone(const QTimeZone& value)
{
    if (d->timeZone == value)
        return;

    d->timeZone = value;
    emit timeZoneChanged();
}

void DisplayTimeHelper::setLocale(const QLocale& locale)
{
    if (d->formatter->locale() == locale)
        return;

    d->formatter = nx::vms::time::Formatter::custom(locale, is24HoursTimeFormat());
    emit localeChanged();
}

void DisplayTimeHelper::set24HoursTimeFormat(bool value)
{
    if (d->formatter->is24HoursTimeFormat() == value)
        return;

    d->formatter = nx::vms::time::Formatter::custom(locale(), value);
    emit is24HoursTimeFormatChanged();
}

bool DisplayTimeHelper::is24HoursTimeFormat() const
{
    return d->formatter->is24HoursTimeFormat();
}

qint64 DisplayTimeHelper::displayOffset() const
{
    const std::chrono::milliseconds displayTimeZoneUtcOffsetMs =
        std::chrono::seconds(QDateTime::currentDateTime(d->timeZone).offsetFromUtc());

    const std::chrono::milliseconds localTimeZoneUtcOffsetMs =
        std::chrono::seconds(QDateTime::currentDateTime().offsetFromUtc());

    return (displayTimeZoneUtcOffsetMs - localTimeZoneUtcOffsetMs).count();
}

QLocale DisplayTimeHelper::locale() const
{
    return d->formatter->locale();
}

QString DisplayTimeHelper::fullDate() const
{
    return d->formatter->toString(d->dateTime, nx::vms::time::Format::d_MMMM_yyyy);
}

QString DisplayTimeHelper::hours() const
{
    return d->formatter->toString(d->dateTime, nx::vms::time::Format::h);
}

QString DisplayTimeHelper::minutes() const
{
    return d->formatter->toString(d->dateTime, nx::vms::time::Format::m);
}

QString DisplayTimeHelper::seconds() const
{
    return d->formatter->toString(d->dateTime, nx::vms::time::Format::s);
}

QString DisplayTimeHelper::noonMark() const
{
    return d->formatter->toString(d->dateTime, nx::vms::time::Format::a);
}

} // namespace nx::vms::client::core
