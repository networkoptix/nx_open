#include "datetime_formatter.h"

#include <map>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QLocale>

#include <nx/utils/literal.h>

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
    {Format::dd_MMMM_yyyy, lit("dd MMMM yyyy")},
    {Format::yyyy_MM_dd_hh_mm_ss, lit("yyyy-MM-dd hh:mm:ss")},
    {Format::dddd_d_MMMM_yyyy_hh_mm_ss, lit("dddd, d MMMM yyyy hh:mm:ss")},

    {Format::filename_date, lit("yyyy_MM_dd_hh_mm_ss")},
    {Format::filename_time, lit("hh_mm_ss")},
};

void DateTimeFormats::setFormats()
{
    QLocale locale = QLocale::system(); //< We now use OS locale for time instead of client locale.
    const bool amPm = locale.timeFormat().contains(lit("AP"), Qt::CaseInsensitive);

    formatStrings[Format::hh] = amPm ? lit("h AP") : lit("hh:00");
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

    formatStrings[Format::filename_date] = amPm
        ? lit("yyyy_MM_dd_hAP_mm_ss")
        : lit("yyyy_MM_dd_hh_mm_ss");
}

bool localeEverInited = false;

void checkInited()
{
    if (!localeEverInited)
        initLocale();
}

} // namespace

QString toString(const QDateTime& time, Format format)
{
    checkInited();
    return time.toString(formatStrings[format]);
}

QString toString(const QTime& time, Format format)
{
    checkInited();
    return time.toString(formatStrings[format]);
}

QString toString(const QDate& date, Format format)
{
    checkInited();
    return date.toString(formatStrings[format]);
}

QString toString(qint64 msSinceEpoch, Format format)
{
    checkInited();
    return QDateTime::fromMSecsSinceEpoch(msSinceEpoch).toString(formatStrings[format]);
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
