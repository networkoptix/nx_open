// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "display_time_helper.h"

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
    qint64 displayOffset = 0;
    QDateTime dateTime;
    nx::vms::time::FormatterPtr formatter;
};

DisplayTimeHelper::Private::Private(DisplayTimeHelper* owner):
    q(owner),
    position(0),
    displayOffset(0),
    dateTime(getDateTime()),
    formatter(nx::vms::time::Formatter::system())
{
}

QDateTime DisplayTimeHelper::Private::getDateTime() const
{
    return QDateTime::fromMSecsSinceEpoch(position + displayOffset);
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
    connect(this, &DisplayTimeHelper::displayOffsetChanged, this, updateDateTime);
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

void DisplayTimeHelper::setDisplayOffset(qint64 value)
{
    value = std::clamp<qint64>(
        value, CalendarUtils::kMinDisplayOffset, CalendarUtils::kMaxDisplayOffset);

    if (d->displayOffset == value)
        return;

    d->displayOffset = value;
    emit displayOffsetChanged();
}

qint64 DisplayTimeHelper::displayOffset() const
{
    return d->displayOffset;
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
