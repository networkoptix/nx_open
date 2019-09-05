#include "datetime_formatter.h"

#include <map>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QLocale>

#include <nx/utils/literal.h>
#include <nx/utils/std/optional.h>

namespace datetime {

namespace {

// This class is to define context for Qt Linguist only.
class DateTimeFormats
{
    Q_DECLARE_TR_FUNCTIONS(DateTimeFormats)

public:
    static void setFormats();
};

void removeTimezone(QString& source)
{
    source.remove(lit(" t"));
}

// These default values are used ONLY in emergency when proper values are not initialized.
std::map<Format, QString> formatStrings =
{
    {Format::h, lit("hh")},
    {Format::m, lit("mm")},
    {Format::s, lit("ss")},
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

void DateTimeFormats::setFormats()
{
    const auto locale = QLocale::system();
    formatStrings[Format::hh] = is24HoursTimeFormat() ? lit("hh:00") : lit("h AP");
    formatStrings[Format::hh_mm] = locale.timeFormat(QLocale::ShortFormat);
    formatStrings[Format::hh_mm_ss] = locale.timeFormat(QLocale::LongFormat);

    // Fix - we never want timezone in time string.
    removeTimezone(formatStrings[Format::hh_mm_ss]);

    formatStrings[Format::hh_mm_ss_zzz] = formatStrings[Format::hh_mm_ss] + lit(".zzz");

    formatStrings[Format::dd_MM] = tr("MM/dd"); //< Localizable
    formatStrings[Format::MMMM_yyyy] = tr("MMMM yyyy"); //< Localizable
    formatStrings[Format::dd_MM_yyyy] = locale.dateFormat(QLocale::ShortFormat);

    auto shortFormat = locale.dateTimeFormat(QLocale::ShortFormat);
    // QLocale::ShortFormat probably does not include seconds - add them!
    if (!shortFormat.contains("ss") && shortFormat.contains("mm"))
    {
        const int dividerPos = shortFormat.indexOf("mm") - 1;
        const auto divider = dividerPos >= 0 ? QChar(shortFormat[dividerPos]) : QChar(':');
        shortFormat.insert(dividerPos + 3, divider + QString("ss"));
    }
    formatStrings[Format::yyyy_MM_dd_hh_mm_ss] = shortFormat;

    formatStrings[Format::dddd_d_MMMM_yyyy_hh_mm_ss] = locale.dateTimeFormat(QLocale::LongFormat);

    // Fix - we never want timezone in time string.
    removeTimezone(formatStrings[Format::yyyy_MM_dd_hh_mm_ss]);
    removeTimezone(formatStrings[Format::dddd_d_MMMM_yyyy_hh_mm_ss]);

    formatStrings[Format::filename_date] = is24HoursTimeFormat()
        ? lit("yyyy_MM_dd_hh_mm_ss")
        : lit("yyyy_MM_dd_hAP_mm_ss");
}

bool localeEverInited = false;

void checkInited()
{
    if (!localeEverInited)
        initLocale();
}

QString getLocalizedHours(const QTime& value)
{
    if (datetime::is24HoursTimeFormat())
        return value.toString(QStringLiteral("hh"));

    const auto hours = value.hour() % 12;
    return QString::number(hours ? hours : 12);
}

QString getHoursTimeFormatMark(const QTime& value)
{
    if (datetime::is24HoursTimeFormat())
        return QString();

    // We don't localize AM/PM markers.
    return value.hour() < 12 ? "AM" : "PM";
}

std::optional<bool> is24HoursTimeFormatValue;

} // namespace

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

bool is24HoursTimeFormat()
{
    if (!is24HoursTimeFormatValue)
    {
        // We now use OS locale for time instead of client locale.
        const auto format = QLocale::system().timeFormat();
        is24HoursTimeFormatValue = !format.contains(lit("AP"), Qt::CaseInsensitive);
    };
    return *is24HoursTimeFormatValue;
}

void set24HoursTimeFormat(bool value)
{
    if (is24HoursTimeFormat() == value)
        return;

    is24HoursTimeFormatValue = value;
    DateTimeFormats::setFormats();
}

QString toString(const QDateTime& value, Format format)
{
    checkInited();
    switch (format)
    {
        case Format::h:
            return getLocalizedHours(value.time());
        case Format::a:
            return getHoursTimeFormatMark(value.time());
        default: //< Other formats are handled by usuall way.
            return value.toString(formatStrings[format]);
    }
}

QString toString(const QTime& time, Format format)
{
    checkInited();
    return toString(QDateTime(QDate(), time, Qt::UTC), format);
}

QString toString(const QDate& date, Format format)
{
    checkInited();
    return toString(QDateTime(date, QTime()), format);
}

QString toString(qint64 msSinceEpoch, Format format)
{
    checkInited();
    return toString(QDateTime::fromMSecsSinceEpoch(msSinceEpoch), format);
}

QString getFormatString(Format format)
{
    checkInited();
    return formatStrings[format];
}

void initLocale()
{
    DateTimeFormats::setFormats();
    localeEverInited = true;
}

} // namespace datetime
