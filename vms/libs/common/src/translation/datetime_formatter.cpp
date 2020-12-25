#include "datetime_formatter.h"

#include <map>

#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QLocale>

#include <nx/utils/literal.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace datetime {

namespace {

void removeTimezone(QString& source)
{
    source.remove(lit(" t"));
}

QString durationToString(std::chrono::milliseconds value, Format format)
{
    const auto hours =
        std::chrono::duration_cast<std::chrono::hours>(value);
    const auto minutes =
        std::chrono::duration_cast<std::chrono::minutes>(value - hours);
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(value - hours - minutes);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value - hours - minutes - seconds);

    QString result = QString::number(hours.count());
    if (format >= Format::hhh_mm)
        result += ":" + QString::number(minutes.count()).rightJustified(2, '0');
    if (format >= Format::hhh_mm_ss)
        result += ":" + QString::number(seconds.count()).rightJustified(2, '0');
    if (format >= Format::hhh_mm_ss_zzz)
        result += ":" + QString::number(milliseconds.count()).rightJustified(3, '0');
    return result;
}

bool isDurationFormat(Format format)
{
    return Format::duration_formats_begin <= format && format < Format::duration_formats_end;
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

void setFormats()
{
    QLocale locale = QLocale::system(); //< We now use OS locale for time instead of client locale.
    const bool amPm = locale.timeFormat().contains(lit("AP"), Qt::CaseInsensitive);

    formatStrings[Format::hh] = amPm ? lit("h AP") : lit("hh:00");
    formatStrings[Format::hh_mm] = locale.timeFormat(QLocale::ShortFormat);
    formatStrings[Format::hh_mm_ss] = locale.timeFormat(QLocale::LongFormat);

    // Fix - we never want timezone in time string.
    removeTimezone(formatStrings[Format::hh_mm_ss]);

    formatStrings[Format::hh_mm_ss_zzz] = formatStrings[Format::hh_mm_ss] + lit(".zzz");

    formatStrings[Format::dd_MM] = "MM/dd";
    formatStrings[Format::MMMM_yyyy] = "MMMM yyyy";
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
    NX_ASSERT(!isDurationFormat(format), "Inappropriate time format.");

    checkInited();
    return time.toString(formatStrings[format]);
}

QString toString(const QTime& time, Format format)
{
    NX_ASSERT(!isDurationFormat(format), "Inappropriate time format.");

    checkInited();
    return time.toString(formatStrings[format]);
}

QString toString(const QDate& date, Format format)
{
    NX_ASSERT(!isDurationFormat(format), "Inappropriate time format.");

    checkInited();
    return date.toString(formatStrings[format]);
}

QString toString(qint64 msSinceEpoch, Format format)
{
    if (isDurationFormat(format))
        return durationToString(std::chrono::milliseconds(msSinceEpoch), format);

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
    setFormats();
    localeEverInited = true;
}

} // namespace datetime
