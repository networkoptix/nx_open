#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QString>

/** Formats date and time using application-wide settings */
namespace datetime {

enum class Format
{
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
    dd_MMMM_yyyy,
    yyyy_MM_dd_hh_mm_ss, // ~=QLocale::DateTime::ShortFormat
    dddd_d_MMMM_yyyy_hh_mm_ss, // ~=QLocale::DateTime::LongFormat

    filename_date, // "yyyy_MMM_dd_hh_mm_ss"
    filename_time, // "hh_mm_ss"
};


QString toString(const QDateTime& time, Format format = Format::yyyy_MM_dd_hh_mm_ss);
QString toString(const QTime& time, Format format = Format::hh_mm_ss);
QString toString(const QDate& date, Format format = Format::dd_MM_yyyy);

QString toString(qint64 msSinceEpoch, Format format = Format::yyyy_MM_dd_hh_mm_ss);

QString getFormatString(Format format);

void initLocale();

} // namespace datetime
