#include "date_time_formatter.h"

#include <QtCore/QString>
#include <QtCore/QLocale>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <nx/utils/log/assert.h>

/* Contents copied from Qt and modified for our needs. */

static QString qt_readEscapedFormatString(const QString &format, int *idx)
{
    int &i = *idx;

    NX_ASSERT(format.at(i) == QLatin1Char('\''));
    ++i;
    if (i == format.size())
        return QString();
    if (format.at(i).unicode() == '\'') { // "''" outside of a quoted stirng
        ++i;
        return QLatin1String("'");
    }

    QString result;

    while (i < format.size()) {
        if (format.at(i).unicode() == '\'') {
            if (i + 1 < format.size() && format.at(i + 1).unicode() == '\'') {
                // "''" inside of a quoted string
                result.append(QLatin1Char('\''));
                i += 2;
            } else {
                break;
            }
        } else {
            result.append(format.at(i++));
        }
    }
    if (i < format.size())
        ++i;

    return result;
}

static void getTimeFormatAPD(const QString &format, bool *containsAP, bool *containsD)
{
    *containsAP = *containsD = false;

    int i = 0;
    while (i < format.size()) {
        if (format.at(i).unicode() == '\'') {
            qt_readEscapedFormatString(format, &i);
            continue;
        }

        if (format.at(i).toLower().unicode() == 'a') {
            *containsAP = true;

            if(*containsD)
                return;
        }

        if (format.at(i).unicode() == 'd') {
            *containsD = true;

            if(*containsAP)
                return;
        }

        ++i;
    }
}

static int qt_repeatCount(const QString &s, int i)
{
    QChar c = s.at(i);
    int j = i + 1;
    while (j < s.size() && s.at(j) == c)
        ++j;
    return j - i;
}

static QString zeroPad(long value, int width)
{
    if ( value < 0 )
        return QString::number(qAbs(value)).rightJustified(width>0 ? width-1 : 0, QLatin1Char('0'), true).prepend(QLatin1Char('-'));
    else
        return QString::number(value).rightJustified(width, QLatin1Char('0'), true);
}

static QString timeZone()
{
#if defined(Q_OS_WINCE)
    TIME_ZONE_INFORMATION info;
    DWORD res = GetTimeZoneInformation(&info);
    if (res == TIME_ZONE_ID_UNKNOWN)
        return QString();
    return QString::fromWCharArray(info.StandardName);
#elif defined(Q_OS_WIN)
    _tzset();
# if defined(_MSC_VER) && _MSC_VER >= 1400
    size_t returnSize = 0;
    char timeZoneName[512];
    if (_get_tzname(&returnSize, timeZoneName, 512, 1))
        return QString();
    return QString::fromLocal8Bit(timeZoneName);
# else
    return QString::fromLocal8Bit(_tzname[1]);
# endif
#elif defined(Q_OS_VXWORKS)
    return QString();
#else
    tzset();
    return QString::fromLocal8Bit(tzname[1]);
#endif
}

QString QnDateTimeFormatter::dateTimeToString(const QString &format,
                                             const QDate *date,
                                             const QTime *time,
                                             const QLocale *q)
{
    NX_ASSERT(date || time);
    if ((date && !date->isValid()) || (time && !time->isValid()))
        return QString();
    
    bool format_am_pm;
    bool format_day;
    getTimeFormatAPD(format, &format_am_pm, &format_day);
    if(!time)
        format_am_pm = false;
    if(!date)
        format_day = false;


    enum { AM, PM } am_pm = AM;
    int hour12 = time ? time->hour() : -1;
    if (time) {
        if (hour12 == 0) {
            am_pm = AM;
            hour12 = 12;
        } else if (hour12 < 12) {
            am_pm = AM;
        } else if (hour12 == 12) {
            am_pm = PM;
        } else {
            am_pm = PM;
            hour12 -= 12;
        }
    }

    QString result;

    int i = 0;
    while (i < format.size()) {
        if (format.at(i).unicode() == '\'') {
            result.append(qt_readEscapedFormatString(format, &i));
            continue;
        }

        const QChar c = format.at(i);
        int repeat = qt_repeatCount(format, i);
        bool used = false;
        if (date) {
            switch (c.unicode()) {
            case 'y':
                used = true;
                if (repeat >= 4)
                    repeat = 4;
                else if (repeat >= 2)
                    repeat = 2;

                switch (repeat) {
                case 4:
                    result.append(zeroPad(date->year(), 4));
                    break;
                case 2:
                    result.append(zeroPad(date->year() % 100, 2));
                    break;
                default:
                    repeat = 1;
                    result.append(c);
                    break;
                }
                break;

            case 'M':
                used = true;
                repeat = qMin(repeat, 4);
                switch (repeat) {
                case 1:
                    result.append(QString::number(date->month()));
                    break;
                case 2:
                    result.append(zeroPad(date->month(), 2));
                    break;
                case 3:
                    result.append(format_day ? q->monthName(date->month(), QLocale::ShortFormat) : q->standaloneMonthName(date->month(), QLocale::ShortFormat));
                    break;
                case 4:
                    result.append(format_day ? q->monthName(date->month(), QLocale::LongFormat) : q->standaloneMonthName(date->month(), QLocale::LongFormat));
                    break;
                }
                break;

            case 'd':
                used = true;
                repeat = qMin(repeat, 4);
                switch (repeat) {
                case 1:
                    result.append(QString::number(date->day()));
                    break;
                case 2:
                    result.append(zeroPad(date->day(), 2));
                    break;
                case 3:
                    result.append(q->dayName(date->dayOfWeek(), QLocale::ShortFormat));
                    break;
                case 4:
                    result.append(q->dayName(date->dayOfWeek(), QLocale::LongFormat));
                    break;
                }
                break;

            default:
                break;
            }
        }
        if (!used && time) {
            switch (c.unicode()) {
            case 'h': {
                used = true;
                repeat = qMin(repeat, 2);
                const int hour = format_am_pm ? hour12 : time->hour();

                switch (repeat) {
                case 1:
                    result.append(QString::number(hour));
                    break;
                case 2:
                    result.append(zeroPad(hour, 2));
                    break;
                }
                break;
            }
            case 'H':
                used = true;
                repeat = qMin(repeat, 2);
                switch (repeat) {
                case 1:
                    result.append(QString::number(time->hour()));
                    break;
                case 2:
                    result.append(zeroPad(time->hour(), 2));
                    break;
                }
                break;

            case 'm':
                used = true;
                repeat = qMin(repeat, 2);
                switch (repeat) {
                case 1:
                    result.append(QString::number(time->minute()));
                    break;
                case 2:
                    result.append(zeroPad(time->minute(), 2));
                    break;
                }
                break;

            case 's':
                used = true;
                repeat = qMin(repeat, 2);
                switch (repeat) {
                case 1:
                    result.append(QString::number(time->second()));
                    break;
                case 2:
                    result.append(zeroPad(time->second(), 2));
                    break;
                }
                break;

            case 'a':
                used = true;
                if (i + 1 < format.length() && format.at(i + 1).unicode() == 'p') {
                    repeat = 2;
                } else {
                    repeat = 1;
                }
                result.append(am_pm == AM ? q->amText().toLower() : q->pmText().toLower());
                break;

            case 'A':
                used = true;
                if (i + 1 < format.length() && format.at(i + 1).unicode() == 'P') {
                    repeat = 2;
                } else {
                    repeat = 1;
                }
                result.append(am_pm == AM ? q->amText().toUpper() : q->pmText().toUpper());
                break;

            case 'z':
                used = true;
                if (repeat >= 3) {
                    repeat = 3;
                } else {
                    repeat = 1;
                }
                switch (repeat) {
                case 1:
                    result.append(QString::number(time->msec()));
                    break;
                case 3:
                    result.append(zeroPad(time->msec(), 3));
                    break;
                }
                break;

            case 't':
                used = true;
                repeat = 1;
                result.append(timeZone());
                break;
            default:
                break;
            }
        }
        if (!used) {
            result.append(QString(repeat, c));
        }
        i += repeat;
    }

    return result;
}
