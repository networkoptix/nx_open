// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

namespace nx::vms::time {

enum class Format
{
    h, // Hours value in current system time format without noon mark (13 or 1, for example).
    m, // Minutes in the "mm" format.
    s, // Seconds in the "ss" format.
    a, // AM/PM mark if applicable by current locale settings or empty string otherwise.

    hh, // "13:00" or "8 pm"
    hh_mm,
    hh_mm_ss, // ~=QLocale::Time::ShortFormat
    hh_mm_ss_zzz,

    dd,
    MMM,
    yyyy,

    MMMM_yyyy,
    dd_MM_yyyy, // ~=QLocale::Date::ShortFormat
    dd_MM_yy,
    d_MMMM_yyyy,
    dd_MMMM_yyyy,
    MMMM_d_yyyy,
    dd_MM_yyyy_hh_mm_ss, // ~=QLocale::DateTime::ShortFormat
    dd_MM_yy_hh_mm_ss,
    dddd_d_MMMM_yyyy_hh_mm_ss, // ~=QLocale::DateTime::LongFormat

    filename_date, // "yyyy_MMM_dd_hh_mm_ss"
    filename_time, // "hh_mm_ss"
};

class Formatter;
using FormatterPtr = QSharedPointer<Formatter>;
using FormatsHash = QHash<nx::vms::time::Format, QString>;

// Formats date and time using specified locale and time format settings.
class NX_VMS_COMMON_API Formatter
{
public:
    Formatter(const QLocale& locale, bool is24HoursFormat);

    static FormatterPtr system();
    static FormatterPtr custom(const QLocale& locale, bool is24HoursTimeFormat);

    static void forceSystemTimeFormat(bool is24HoursTimeFormat);

public:
    QLocale locale() const;
    bool is24HoursTimeFormat() const;

    // Is not able to accept format from duration_formats.
    QString toString(const QDateTime& time, Format format = Format::dd_MM_yyyy_hh_mm_ss) const;
    QString toString(const QTime& time, Format format = Format::hh_mm_ss) const;
    QString toString(const QDate& date, Format format = Format::dd_MM_yyyy) const;

    // Able to accept format from duration_formats.
    QString toString(qint64 msSinceEpoch, Format format = Format::dd_MM_yyyy_hh_mm_ss) const;

    QString getFormatString(Format format) const;

    struct NX_VMS_COMMON_API Private
    {
        Private(const QLocale& locale, bool is24Hoursformat);

        QString getLocalizedHours(const QTime& value);
        QString getHoursTimeFormatMark(const QTime& value);

        const QLocale locale;
        const bool is24HoursFormat;
        const FormatsHash formatStrings;
    };
    QScopedPointer<Private> d;
};

template<typename ValueType>
QString toString(
    const ValueType& value,
    FormatterPtr formatter = Formatter::system())
{
    return formatter->toString(value);
}

template<typename ValueType>
QString toString(
    const ValueType& value,
    Format format,
    FormatterPtr formatter = Formatter::system())
{
    return formatter->toString(value, format);
}

NX_VMS_COMMON_API qint64 systemDisplayOffset();

NX_VMS_COMMON_API QString getFormatString(
    Format format,
    FormatterPtr formatter = Formatter::system());

NX_VMS_COMMON_API bool is24HoursTimeFormat(FormatterPtr formatter = Formatter::system());

// Formats relative time in the past in human readable form like "just now", "3 minutes ago" etc.
NX_VMS_COMMON_API QString fromNow(std::chrono::seconds duration);

enum class Duration
{
    hh,
    hh_mm,
    hh_mm_ss,
    hh_mm_ss_zzz,
    mm,
    mm_ss,
    mm_ss_zzz,
};

NX_VMS_COMMON_API QString toDurationString(std::chrono::milliseconds value, Duration format);

} // namespace nx::vms::time
