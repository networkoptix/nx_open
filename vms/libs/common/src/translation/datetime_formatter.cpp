#include "datetime_formatter.h"

#include <map>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QLocale>

#include <nx/utils/literal.h>
#include <nx/utils/std/optional.h>

namespace {

void removeTimezone(QString& source)
{
    source.remove(lit(" t"));
}

//--------------------------------------------------------------------------------------------------

using FormatsHash = QHash<datetime::Format, QString>;
using Formatter = datetime::Formatter;
using FormatterPtr = datetime::FormatterPtr;

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
    using namespace datetime;

    FormatsHash result = defaultFormats();
    result[Format::hh] = is24HoursTimeFormat ? lit("hh:00") : lit("h AP");
    result[Format::hh_mm] = locale.timeFormat(QLocale::ShortFormat);
    result[Format::hh_mm_ss] = locale.timeFormat(QLocale::LongFormat);

    // This is fix - we never want timezone in time string.
    removeTimezone(result[Format::hh_mm_ss]);

    result[Format::hh_mm_ss_zzz] = result[Format::hh_mm_ss] + lit(".zzz");

    // Two formats belowcould has different numbers order and other distinctions
    // in some languages. So wee mark them translatable, but hide for translaters and
    // convert them manually.
    result[Format::dd_MM] = tr("MM/dd"); //< Localizable.
    result[Format::MMMM_yyyy] = tr("MMMM yyyy"); //< Localizable.

    result[Format::dd_MM_yyyy] = locale.dateFormat(QLocale::ShortFormat);

    auto shortFormat = locale.dateTimeFormat(QLocale::ShortFormat);

    // QLocale::ShortFormat probably does not include seconds - add them below.
    if (!shortFormat.contains("ss") && shortFormat.contains("mm"))
    {
        const int dividerPos = shortFormat.indexOf("mm") - 1;
        const auto divider = dividerPos >= 0 ? QChar(shortFormat[dividerPos]) : QChar(':');
        shortFormat.insert(dividerPos + 3, divider + QString("ss"));
    }

    result[Format::yyyy_MM_dd_hh_mm_ss] = shortFormat;

    result[Format::dddd_d_MMMM_yyyy_hh_mm_ss] = locale.dateTimeFormat(QLocale::LongFormat);

    // This is fix - we never want timezone in time string.
    removeTimezone(result[Format::yyyy_MM_dd_hh_mm_ss]);
    removeTimezone(result[Format::dddd_d_MMMM_yyyy_hh_mm_ss]);

    result[Format::filename_date] = is24HoursTimeFormat
        ? lit("yyyy_MM_dd_hh_mm_ss")
        : lit("yyyy_MM_dd_hAP_mm_ss");

    return result;
}

const FormatsHash& DateTimeFormats::defaultFormats()
{
    using namespace datetime;
    static const FormatsHash result =
    {
        {Format::h, lit("hh")},
        {Format::m, lit("mm")},
        {Format::s, lit("ss")},
        {Format::a, lit("a")},
        {Format::mm_ss, lit("mm:ss")},
        {Format::hh, lit("hh:00")},
        {Format::hh_mm, lit("hh:mm")},
        {Format::hh_mm_ss, lit("hh:mm:ss")},
        {Format::hh_mm_ss_zzz, lit("hh:mm:ss.zzz")},

        {Format::dd, lit("dd")},
        {Format::MMM, lit("MMM")},
        {Format::yyyy, lit("yyyy")},

        {Format::dd_MM, lit("dd-MM")},
        {Format::MMMM_yyyy, lit("MMMM yyyy")},
        {Format::dd_MM_yyyy, lit("dd-MM-yyyy")},
        {Format::d_MMMM_yyyy, lit("d MMMM yyyy")},
        {Format::dd_MMMM_yyyy, lit("dd MMMM yyyy")},
        {Format::yyyy_MM_dd_hh_mm_ss, lit("yyyy-MM-dd hh:mm:ss")},
        {Format::dddd_d_MMMM_yyyy_hh_mm_ss, lit("dddd, d MMMM yyyy hh:mm:ss")},

        {Format::filename_date, lit("yyyy_MM_dd_hh_mm_ss")},
        {Format::filename_time, lit("hh_mm_ss")},
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
            return !format.contains(lit("AP"), Qt::CaseInsensitive);
        }();

    return forced24HoursFormat ? *forced24HoursFormat : isSystem24HoursFormat;
}

} // namespace

//--------------------------------------------------------------------------------------------------

namespace datetime {

struct Formatter::Private
{
    Private(const QLocale& locale, bool is24Hoursformat);

    QString getLocalizedHours(const QTime& value);
    QString getHoursTimeFormatMark(const QTime& value);

    const QLocale locale;
    const bool is24HoursFormat;
    const FormatsHash formatStrings;
};

Formatter::Private::Private(const QLocale& locale, bool is24HoursFormat):
    locale(locale),
    is24HoursFormat(is24HoursFormat),
    formatStrings(DateTimeFormats::getFormats(locale, is24HoursFormat))
{
}

QString Formatter::Private::getLocalizedHours(const QTime& value)
{
    if (datetime::is24HoursTimeFormat())
        return value.toString(QStringLiteral("hh"));

    const auto hours = value.hour() % 12;
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
        default: //< Other formats are handled by usuall way.
            return value.toString(d->formatStrings[format]);
    }
}

QString Formatter::toString(const QTime& time, Format format) const
{
    return toString(QDateTime(QDate(), time, Qt::UTC), format);
}

QString Formatter::toString(const QDate& date, Format format) const
{
    return toString(QDateTime(date, QTime()), format);
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

} // namespace datetime
