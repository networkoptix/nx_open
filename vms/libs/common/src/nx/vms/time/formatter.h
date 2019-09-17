#pragma once

#include <QtCore/QLocale>
#include <QtCore/QString>
#include <QtCore/QDateTime>

namespace nx::vms::time {

enum Format
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

    MMMM_yyyy,
    dd_MM_yyyy, // ~=QLocale::Date::ShortFormat
    d_MMMM_yyyy,
    dd_MMMM_yyyy,
    yyyy_MM_dd_hh_mm_ss, // ~=QLocale::nx::vms::time::ShortFormat
    dddd_d_MMMM_yyyy_hh_mm_ss, // ~=QLocale::nx::vms::time::LongFormat

    filename_date, // "yyyy_MMM_dd_hh_mm_ss"
    filename_time, // "hh_mm_ss"
};

class Formatter;
using FormatterPtr = QSharedPointer<Formatter>;

// Formats date and time using specified locale and time format settings.
class Formatter
{
public:
    Formatter(const QLocale& locale, bool is24HoursFormat);

    static FormatterPtr system();
    static FormatterPtr custom(const QLocale& locale);
    static FormatterPtr custom(const QLocale& locale, bool is24HoursTimeFormat);

    static void forceSystemTimeFormat(bool is24HoursTimeFormat);

public:
    QLocale locale() const;
    bool is24HoursTimeFormat() const;

    QString toString(const QDateTime& time, Format format = Format::yyyy_MM_dd_hh_mm_ss) const;
    QString toString(const QTime& time, Format format = Format::hh_mm_ss) const;
    QString toString(const QDate& date, Format format = Format::dd_MM_yyyy) const;

    QString toString(qint64 msSinceEpoch, Format format = Format::yyyy_MM_dd_hh_mm_ss) const;

    QString getFormatString(Format format) const;

private:
    struct Private;
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

qint64 systemDisplayOffset();

QString getFormatString(
    Format format,
    FormatterPtr formatter = Formatter::system());

bool is24HoursTimeFormat(FormatterPtr formatter = Formatter::system());

} // namespace nx::vms::time
