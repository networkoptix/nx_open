// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "formatter.h"

#include <map>
#include <optional>
#include <set>

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QString>

#include <nx/utils/log/assert.h>

namespace {

void removeTimezone(QString& source)
{
    source.remove(" t");
}

QString getShortFormatWithSeconds(const QLocale& locale)
{
    auto result = locale.dateTimeFormat(QLocale::ShortFormat);
    if (result.contains("ss") || !result.contains("mm"))
        return result;
    const int dividerPos = result.indexOf("mm") - 1;
    const auto divider = dividerPos > 0 ? QChar(result[dividerPos]) : QChar(':');
    result.insert(dividerPos + 3, divider + QString("ss"));
    return result;
}

struct TimeFormat
{
    Q_DECLARE_TR_FUNCTIONS(TimeFormat);
};

//--------------------------------------------------------------------------------------------------

using FormatsHash = QHash<nx::vms::time::Format, QString>;
using Formatter = nx::vms::time::Formatter;
using FormatterPtr = nx::vms::time::FormatterPtr;

// This class is to define context for Qt Linguist only.
class DateTimeFormats
{
    Q_DECLARE_TR_FUNCTIONS(DateTimeFormats)

public:
    static FormatsHash getFormats(const QLocale& locale, bool is24HoursTimeFormat);

private:
    static const FormatsHash& defaultFormats();
};

FormatsHash DateTimeFormats::getFormats(const QLocale& locale, bool is24HoursTimeFormat)
{
    using namespace nx::vms::time;

    FormatsHash result = defaultFormats();
    result[Format::hh] = is24HoursTimeFormat ? "hh:00" : "h AP";
    result[Format::hh_mm] = locale.timeFormat(QLocale::ShortFormat);
    result[Format::hh_mm_ss] = locale.timeFormat(QLocale::LongFormat);

    // This is fix - we never want timezone in time string.
    removeTimezone(result[Format::hh_mm_ss]);

    result[Format::hh_mm_ss_zzz] = is24HoursTimeFormat ? "hh:mm:ss.zzz" : "h:mm:ss.zzz AP";
    result[Format::MMMM_yyyy] = "MMMM yyyy";

    auto shortDate = locale.dateFormat(QLocale::ShortFormat);
    result[Format::dd_MM_yyyy] = shortDate;

    shortDate.replace("yyyy", "yy");
    result[Format::dd_MM_yy] = shortDate;

    auto shortDateTime = getShortFormatWithSeconds(locale);
    result[Format::dd_MM_yyyy_hh_mm_ss] = shortDateTime;

    shortDateTime.replace("yyyy", "yy");
    result[Format::dd_MM_yy_hh_mm_ss] = shortDateTime;

    result[Format::dddd_d_MMMM_yyyy_hh_mm_ss] = locale.dateTimeFormat(QLocale::LongFormat);

    // This is fix - we never want timezone in time string.
    removeTimezone(result[Format::dd_MM_yyyy_hh_mm_ss]);
    removeTimezone(result[Format::dddd_d_MMMM_yyyy_hh_mm_ss]);

    result[Format::filename_date] = is24HoursTimeFormat
        ? "yyyy_MM_dd_hh_mm_ss"
        : "yyyy_MM_dd_hAP_mm_ss";

    return result;
}

const FormatsHash& DateTimeFormats::defaultFormats()
{
    using namespace nx::vms::time;
    static const FormatsHash result =
    {
        {Format::h, "hh"},
        {Format::m, "mm"},
        {Format::s, "ss"},
        {Format::a, "a"},
        {Format::hh, "hh:00"},
        {Format::hh_mm, "hh:mm"},
        {Format::hh_mm_ss, "hh:mm:ss"},
        {Format::hh_mm_ss_zzz, "hh:mm:ss.zzz"},

        {Format::dd, "dd"},
        {Format::MMM, "MMM"},
        {Format::yyyy, "yyyy"},

        {Format::MMMM_yyyy, "MMMM yyyy"},
        {Format::dd_MM_yyyy, "dd-MM-yyyy"},
        {Format::dd_MM_yyyy, "dd-MM-yy"},
        {Format::d_MMMM_yyyy, "d MMMM yyyy"},
        {Format::dd_MMMM_yyyy, "dd MMMM yyyy"},
        {Format::MMMM_d_yyyy, "MMMM d, yyyy"},
        {Format::dd_MM_yyyy_hh_mm_ss, "dd-MM-yyyy hh:mm:ss"},
        {Format::dd_MM_yy_hh_mm_ss, "dd-MM-yy hh:mm:ss"},
        {Format::dddd_d_MMMM_yyyy_hh_mm_ss, "dddd, d MMMM yyyy hh:mm:ss"},

        {Format::filename_date, "yyyy_MM_dd_hh_mm_ss"},
        {Format::filename_time, "hh_mm_ss"},
    };

    return result;
}

//--------------------------------------------------------------------------------------------------

class SystemFormatterHolder
{
public:
    static FormatterPtr get();
    static void forceTimeFormat(bool is24HoursTimeFormatValue);

private:
    static bool is24HoursTimeFormat();

    static std::optional<bool> forced24HoursFormat;
    static FormatterPtr systemFormatter;
};

std::optional<bool> SystemFormatterHolder::forced24HoursFormat;
FormatterPtr SystemFormatterHolder::systemFormatter(
    Formatter::custom(QLocale::system(), SystemFormatterHolder::is24HoursTimeFormat()));

FormatterPtr SystemFormatterHolder::get()
{
    return systemFormatter;
}

void SystemFormatterHolder::forceTimeFormat(bool is24HoursTimeFormatValue)
{
    forced24HoursFormat = is24HoursTimeFormatValue;
    if (SystemFormatterHolder::get()->is24HoursTimeFormat() != is24HoursTimeFormatValue)
        systemFormatter = Formatter::custom(QLocale::system(), is24HoursTimeFormatValue);
}

bool SystemFormatterHolder::is24HoursTimeFormat()
{
    static const bool isSystem24HoursFormat =
        []()
        {
            const auto format = QLocale::system().timeFormat();
            return !format.contains("AP", Qt::CaseInsensitive);
        }();

    return forced24HoursFormat ? *forced24HoursFormat : isSystem24HoursFormat;
}

} // namespace

//--------------------------------------------------------------------------------------------------

namespace nx::vms::time {

Formatter::Private::Private(const QLocale& locale, bool is24HoursFormat):
    locale(locale),
    is24HoursFormat(is24HoursFormat),
    formatStrings(DateTimeFormats::getFormats(locale, is24HoursFormat))
{
}

QString Formatter::Private::getLocalizedHours(const QTime& value)
{
    if (is24HoursFormat)
        return value.toString(QStringLiteral("hh"));

    const auto hours = value.hour() % 12;
    // Am/PM notation suppose that 0 am(pm) time should be shown as 12 am(pm).
    return QString::number(hours ? hours : 12);
}

QString Formatter::Private::getHoursTimeFormatMark(const QTime& value)
{
    if (is24HoursFormat)
        return QString();

    // We don't localize AM/PM markers.
    return value.hour() < 12 ? "AM" : "PM";
}

//--------------------------------------------------------------------------------------------------

FormatterPtr Formatter::system()
{
    return SystemFormatterHolder::get();
}

FormatterPtr Formatter::custom(const QLocale& locale, bool is24HoursTimeFormat)
{
    return FormatterPtr(new Formatter(locale, is24HoursTimeFormat));
}

void Formatter::forceSystemTimeFormat(bool is24HoursTimeFormat)
{
    SystemFormatterHolder::forceTimeFormat(is24HoursTimeFormat);
}

Formatter::Formatter(const QLocale& locale, bool is24HoursFormat):
    d(new Private(locale, is24HoursFormat))
{
}

QLocale Formatter::locale() const
{
    return d->locale;
}

bool Formatter::is24HoursTimeFormat() const
{
    return d->is24HoursFormat;
}

QString Formatter::toString(const QDateTime& value, Format format) const
{
    switch (format)
    {
        case Format::h:
            return d->getLocalizedHours(value.time());
        case Format::a:
            return d->getHoursTimeFormatMark(value.time());
        default: //< Other formats are handled by usual way.
            return d->locale.toString(value, d->formatStrings[format]);
    }
}

QString Formatter::toString(const QTime& time, Format format) const
{
    return toString(QDateTime(QDate::currentDate(), time, Qt::UTC), format);
}

QString Formatter::toString(const QDate& date, Format format) const
{
    return toString(QDateTime(date, QTime(0, 0)), format);
}

QString Formatter::toString(qint64 msSinceEpoch, Format format) const
{
    return toString(QDateTime::fromMSecsSinceEpoch(msSinceEpoch), format);
}

QString Formatter::getFormatString(Format format) const
{
    return d->formatStrings[format];
}

//--------------------------------------------------------------------------------------------------

QString toDurationString(std::chrono::milliseconds value, Duration format)
{
    bool withHours = false;
    bool withMinutes = false;
    bool withSeconds = false;
    bool withMs = false;

    switch (format)
    {
        case Duration::hh_mm_ss_zzz:
            withMs = true;
            [[fallthrough]];
        case Duration::hh_mm_ss:
            withSeconds = true;
            [[fallthrough]];
        case Duration::hh_mm:
            withMinutes = true;
            [[fallthrough]];
        case Duration::hh:
            withHours = true;
            break;
        case Duration::mm_ss_zzz:
            withMs = true;
            [[fallthrough]];
        case Duration::mm_ss:
            withSeconds = true;
            [[fallthrough]];
        case Duration::mm:
            withMinutes = true;
            break;
    }

    const auto hours = withHours
        ? std::chrono::duration_cast<std::chrono::hours>(value)
        : std::chrono::hours(0);
    const auto minutes =
        std::chrono::duration_cast<std::chrono::minutes>(value - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(value - hours - minutes);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value - hours - minutes - seconds);

    QString result;
    if (withHours)
    {
        result = QString::number(hours.count());
        if (withMinutes)
            result += ":" + QString::number(minutes.count()).rightJustified(2, '0');
    }
    else
    {
        result = QString::number(minutes.count());
    }
    if (withSeconds)
        result += ":" + QString::number(seconds.count()).rightJustified(2, '0');
    if (withMs)
        result += "." + QString::number(milliseconds.count()).rightJustified(3, '0');
    return result;
}

qint64 systemDisplayOffset()
{
    static const qint64 result =
        []()
        {
            const auto current = QDateTime::currentDateTime();
            const auto utc = QDateTime(current.date(), current.time(), Qt::UTC);
            return current.secsTo(utc) * 1000;
        }();

    return result;
}

QString getFormatString(Format format, FormatterPtr formatter)
{
    return formatter->getFormatString(format);
}

bool is24HoursTimeFormat(FormatterPtr formatter)
{
    return formatter->is24HoursTimeFormat();
}

QString fromNow(std::chrono::seconds duration)
{
    using namespace std::chrono;

    const auto numSeconds = duration_cast<seconds>(duration);
    if (numSeconds.count() < 60)
        return TimeFormat::tr("just now");

    const auto numMinutes = duration_cast<minutes>(duration);
    if (numMinutes.count() < 60)
        return TimeFormat::tr("%n minute(s) ago", "", numMinutes.count());

    const auto numHours = duration_cast<hours>(duration);
    if (numHours.count() < 24)
        return TimeFormat::tr("%n hour(s) ago", "", numHours.count());

    const auto numDays = duration_cast<days>(duration);
    if (numDays.count() < 2)
        return TimeFormat::tr("yesterday");

    if (numDays.count() < 7)
        return TimeFormat::tr("%n day(s) ago", "", numDays.count());

    if (numDays.count() == 7)
        return TimeFormat::tr("a week ago");

    const auto msecsSinceEpoch =
        QDateTime::currentMSecsSinceEpoch() - duration_cast<milliseconds>(duration).count();

    if (msecsSinceEpoch < 0) //< This should not happen but we still need to show correct time.
        return TimeFormat::tr("%n day(s) ago", "", numDays.count());

    return toString(QDateTime::fromMSecsSinceEpoch(msecsSinceEpoch), Format::d_MMMM_yyyy);
}

} // namespace nx::vms::time
