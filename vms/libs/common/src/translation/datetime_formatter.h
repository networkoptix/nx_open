#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QString>

/** Formats date and time using application-wide settings */
namespace datetime {

enum class Format
{
    h, // Hours value in current system time format without noon mark (13 or 1, for example).
    m, // Minutes in the "mm" format.
    s, // Seconds in the "ss" format.
    a, // AM/PM mark if applicable by current locale settings or empty string otherwise.

    mm_ss,
    hh, // "13:00" or "8 pm"
    hh_mm,
    hh_mm_ss, // ~=QLocale::Time::ShortFormat
    hh_mm_ss_zzz,

    dd,
    MMM,
    yyyy,

    dd_MM,
    MMMM_yyyy,
    dd_MM_yyyy, // ~=QLocale::Date::ShortFormat
    d_MMMM_yyyy,
    dd_MMMM_yyyy,
    yyyy_MM_dd_hh_mm_ss, // ~=QLocale::DateTime::ShortFormat
    dddd_d_MMMM_yyyy_hh_mm_ss, // ~=QLocale::DateTime::LongFormat

    filename_date, // "yyyy_MMM_dd_hh_mm_ss"
    filename_time, // "hh_mm_ss"
};


qint64 systemDisplayOffset();

bool is24HoursTimeFormat();
void set24HoursTimeFormat(bool value);

QString toString(const QDateTime& time, Format format = Format::yyyy_MM_dd_hh_mm_ss);
QString toString(const QTime& time, Format format = Format::hh_mm_ss);
QString toString(const QDate& date, Format format = Format::dd_MM_yyyy);

QString toString(qint64 msSinceEpoch, Format format = Format::yyyy_MM_dd_hh_mm_ss);

QString getFormatString(Format format);

void initLocale();

} // namespace datetime
