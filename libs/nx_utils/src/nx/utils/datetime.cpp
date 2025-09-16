// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "datetime.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include <QtCore/QTimeZone>

#ifdef _WIN32
    #include <time.h>
#endif

#include "nx_utils_ini.h"
#include "std_string_utils.h"
#include "string.h"

namespace nx::utils {

using namespace std::chrono;

namespace {

static constexpr const char* kWeekDays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
static constexpr const char* kMonths[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Cross-platform timegm replacement
std::time_t timegmCrossPlatform(struct tm* tm)
{
#ifdef _WIN32
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
}

// Forward declarations for lock-free parsing functions
std::int64_t parseRfc1123DateLockFree(const std::string_view& str);
std::int64_t parseRfc850DateLockFree(const std::string_view& str);
std::int64_t parseAnsiCDateLockFree(const std::string_view& str);

QDateTime parseRfc1123Date(const std::string_view& str)
{
    static constexpr const char* kTemplate = "%3s, %d %3s %d %d:%d:%d";
    char weekday[4], monthStr[4];
    int day, year, hour, min, sec;
    if (sscanf(str.data(), kTemplate, weekday, &day, monthStr, &year, &hour, &min, &sec) != 7)
        return QDateTime();

    for (int month = 0; month < 12; ++month)
    {
        if (strncmp(monthStr, kMonths[month], 3) == 0)
            return QDateTime(QDate(year, month + 1, day), QTime(hour, min, sec), QTimeZone::UTC);
    }
    return QDateTime();
}

// Sunday, 06-Nov-94 08:49:37 GMT (RFC 850, obsoleted by RFC 1036)
QDateTime parseRfc850Date(const std::string_view& str)
{
    static constexpr char kRfc850Template[] = "dddd, dd-MMM-yy hh:mm:ss";
    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()), kRfc850Template);
}

// Sun Nov  6 08:49:37 1994  (ANSI C's asctime() format)
QDateTime parseAnsiCDate(const std::string_view& str)
{
    // QDateTime::fromString() requires exact format match, so two templates are required:
    // Non padded if the day of the month is >= 10, and padded if day of the month is < 10
    static constexpr char kAnsiCTemplate[]       = "ddd MMM d hh:mm:ss yyyy";
    static constexpr char kAnsiCTemplatePadded[] = "ddd MMM  d hh:mm:ss yyyy";

    if (str.size() < 9)
        return QDateTime();

    return QLocale(QLocale::C).toDateTime(
        QByteArray::fromRawData(str.data(), str.size()),
        str[8] == ' ' ? kAnsiCTemplatePadded : kAnsiCTemplate);
}

// Lock-free parsing helper functions
std::int64_t parseRfc1123DateLockFree(const std::string_view& str)
{
    // Format: "Sun, 06 Nov 1994 08:49:37"
    // Expected length: 25 characters
    if (str.length() < 25)
        return -1;

    struct tm tm = {};
    char monthStr[4] = {};

    // Parse: "Sun, 06 Nov 1994 08:49:37"
    if (sscanf(str.data(),
            "%*3s, %2d %3s %4d %2d:%2d:%2d",
            &tm.tm_mday,
            monthStr,
            &tm.tm_year,
            &tm.tm_hour,
            &tm.tm_min,
            &tm.tm_sec)
        != 6)
        return -1;

    // Convert month name to number
    static constexpr const char* kMonths[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    tm.tm_mon = -1;
    for (int i = 0; i < 12; ++i)
    {
        if (strncmp(monthStr, kMonths[i], 3) == 0)
        {
            tm.tm_mon = i;
            break;
        }
    }
    if (tm.tm_mon == -1)
        return -1;

    tm.tm_year -= 1900; // tm_year is years since 1900

    // Convert to time_t and then to milliseconds
    std::time_t timeT = timegmCrossPlatform(&tm);
    if (timeT == -1)
        return -1;

    return static_cast<std::int64_t>(timeT) * 1000;
}

std::int64_t parseRfc850DateLockFree(const std::string_view& str)
{
    // Format: "Sunday, 06-Nov-94 08:49:37"
    // Expected length: 28 characters
    if (str.length() < 28)
        return -1;

    struct tm tm = {};
    char monthStr[4] = {};
    int year;

    // Parse: "Sunday, 06-Nov-94 08:49:37"
    if (sscanf(str.data(),
            "%*[^,], %2d-%3s-%2d %2d:%2d:%2d",
            &tm.tm_mday,
            monthStr,
            &year,
            &tm.tm_hour,
            &tm.tm_min,
            &tm.tm_sec)
        != 6)
        return -1;

    // Convert month name to number
    static constexpr const char* kMonths[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    tm.tm_mon = -1;
    for (int i = 0; i < 12; ++i)
    {
        if (strncmp(monthStr, kMonths[i], 3) == 0)
        {
            tm.tm_mon = i;
            break;
        }
    }
    if (tm.tm_mon == -1)
        return -1;

    // Convert 2-digit year to 4-digit year
    if (year < 50)
        tm.tm_year = year + 2000 - 1900; // 00-49 -> 2000-2049
    else
        tm.tm_year = year + 1900 - 1900; // 50-99 -> 1950-1999

    // Convert to time_t and then to milliseconds
    std::time_t timeT = timegmCrossPlatform(&tm);
    if (timeT == -1)
        return -1;

    return static_cast<std::int64_t>(timeT) * 1000;
}

std::int64_t parseAnsiCDateLockFree(const std::string_view& str)
{
    // Format: "Sun Nov  6 08:49:37 1994" or "Sun Nov 16 08:49:37 1994"
    // Expected length: 24 or 25 characters
    if (str.length() < 24)
        return -1;

    struct tm tm = {};
    char monthStr[4] = {};

    // Try parsing with space-padded day first
    if (sscanf(str.data(),
            "%*3s %3s %2d %2d:%2d:%2d %4d",
            monthStr,
            &tm.tm_mday,
            &tm.tm_hour,
            &tm.tm_min,
            &tm.tm_sec,
            &tm.tm_year)
        != 6)
    {
        // Try parsing with single-digit day
        if (sscanf(str.data(),
                "%*3s %3s %d %2d:%2d:%2d %4d",
                monthStr,
                &tm.tm_mday,
                &tm.tm_hour,
                &tm.tm_min,
                &tm.tm_sec,
                &tm.tm_year)
            != 6)
            return -1;
    }

    // Convert month name to number
    static constexpr const char* kMonths[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    tm.tm_mon = -1;
    for (int i = 0; i < 12; ++i)
    {
        if (strncmp(monthStr, kMonths[i], 3) == 0)
        {
            tm.tm_mon = i;
            break;
        }
    }
    if (tm.tm_mon == -1)
        return -1;

    tm.tm_year -= 1900; // tm_year is years since 1900

    // Convert to time_t and then to milliseconds
    std::time_t timeT = timegmCrossPlatform(&tm);
    if (timeT == -1)
        return -1;

    return static_cast<std::int64_t>(timeT) * 1000;
}

} // anonymous namespace

QString timestampToRfc2822(qint64 timestampMs)
{
    if (timestampMs == DATETIME_NOW)
        return "LIVE";

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::RFC2822Date);
}

QString timestampToISO8601(std::chrono::microseconds timestamp)
{
    if (timestamp == std::chrono::microseconds::zero())
        return {};
    return QDateTime::fromMSecsSinceEpoch(
        std::chrono::duration_cast<milliseconds>(timestamp).count())
        .toString(Qt::DateFormat::ISODate);
}

QString timestampToRfc2822(milliseconds timestamp)
{
    return timestampToRfc2822(timestamp.count());
}

QString timestampToRfc2822(std::chrono::system_clock::time_point timestamp)
{
    return timestampToRfc2822(
        duration_cast<milliseconds>(timestamp.time_since_epoch()));
}

QString timestampToDebugString(qint64 timestampMs, const QString& format)
{
    if (timestampMs == 0)
        return "0";

    if (timestampMs == DATETIME_NOW)
        return "DATETIME_NOW";

    if (timestampMs == DATETIME_INVALID)
        return "DATETIME_INVALID";

    static const qint64 kMaxTimeMs = 4'102'444'800'000; //< 2100.01.01.
    if (timestampMs > kMaxTimeMs)
        timestampMs /= 1000; //< Most probably time is in microseconds.

    const QString defaultFormat = nx::utils::ini().debugTimeRepresentation;

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::UTC);
    return dateTime.toString(format.isEmpty() ? defaultFormat : format);
}

QString timestampToDebugString(milliseconds timestamp, const QString& format)
{
    return timestampToDebugString(timestamp.count(), format);
}

std::string formatDateTime(const QDateTime& value)
{
    // Starts with Monday to correspond to QDate::dayOfWeek() return value: 1 for Monday, 7 for Sunday

    if (value.isNull() || !value.isValid())
        return std::string();

    QDateTime utc = value.toUTC();
    const QDate& date = utc.date();
    const QTime& time = utc.time();

    char strDateBuf[256];
    snprintf(strDateBuf, sizeof(strDateBuf), "%s, %02d %s %d %02d:%02d:%02d GMT",
        kWeekDays[date.dayOfWeek() - 1],
        date.day(),
        kMonths[date.month() - 1],
        date.year(),
        time.hour(),
        time.minute(),
        time.second());

    return std::string(strDateBuf);
}

QDateTime parseDateToQDateTime(const std::string_view& str)
{
    auto date = nx::utils::trim(str);

    // Sanity check. parseAnsiCDate() has hard-coded array access.
    if (date.length() < 8)
        return QDateTime();

    // If the string ends with " GMT" it is either RFC 1123 or RFC 850
    if (nx::utils::endsWith(date, " GMT"))
    {
        // QDateTime::fromString() fails if string contains " GMT"
        // because "M" is used as a formatter.
        date = date.substr(0, date.length() - 4);
        QDateTime parsedDate;

        // RFC 1123 date: first three characters are abbreviated day of the week, like "Sun",
        // followed by a comma
        if (date[3] == ',')
            parsedDate = parseRfc1123Date(date);

        if (!parsedDate.isValid())
            parsedDate = parseRfc850Date(date);

        if (parsedDate.isValid())
        {
            parsedDate.setTimeZone(QTimeZone::UTC);
            return parsedDate;
        }
    }

    // Fall back to ANSI C date.
    return parseAnsiCDate(date);
}

std::int64_t parseDateToMillisSinceEpoch(const std::string_view& str)
{
    auto date = nx::utils::trim(str);

    // Sanity check
    if (date.length() < 8)
        return -1;

    // If the string ends with " GMT" it is either RFC 1123 or RFC 850
    if (nx::utils::endsWith(date, " GMT"))
    {
        // Remove " GMT" suffix
        date = date.substr(0, date.length() - 4);
        std::int64_t result = -1;

        // RFC 1123 date: first three characters are abbreviated day of the week, like "Sun",
        // followed by a comma
        if (date.length() >= 4 && date[3] == ',')
            result = parseRfc1123DateLockFree(date);

        if (result == -1)
            result = parseRfc850DateLockFree(date);

        if (result != -1)
            return result;
    }

    // Fall back to ANSI C date
    return parseAnsiCDateLockFree(date);
}

std::string formatDateTime(std::chrono::system_clock::time_point tp)
{
    const auto msecsSinceEpoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());

    QDateTime dt;
    dt.setMSecsSinceEpoch(static_cast<qint64>(msecsSinceEpoch.count()));

    return formatDateTime(dt);
}

std::chrono::system_clock::time_point parseDateTime(std::string_view datetime)
{
    auto dt = parseDateToQDateTime(datetime);
    if (!dt.isValid())
        return {};

    auto msecsSinceEpoch = dt.toMSecsSinceEpoch();

    return std::chrono::system_clock::time_point() +
        std::chrono::milliseconds(static_cast<std::size_t>(msecsSinceEpoch));
}

std::string formatDateTime(std::chrono::steady_clock::time_point tp)
{
    static const std::pair<std::chrono::system_clock::time_point,
        std::chrono::steady_clock::time_point> baseTimePoints{
        std::chrono::system_clock::now(),
        std::chrono::steady_clock::now()};
    return formatDateTime(baseTimePoints.first +
        std::chrono::duration_cast<system_clock::duration>(tp - baseTimePoints.second));
}

microseconds parseDateTimeUsec(const QString& dateTimeStr)
{
    if (dateTimeStr.toLower().trimmed() == "now")
        return microseconds{DATETIME_NOW};

    if (!dateTimeStr.startsWith('-')
        && (dateTimeStr.contains('T') || dateTimeStr.contains('-')))
    {
        const auto dt = QDateTime::fromString(trimAndUnquote(dateTimeStr), Qt::ISODateWithMs);
        if (dt.isValid())
            return duration_cast<microseconds>(milliseconds{dt.toMSecsSinceEpoch()});
    }

    const auto timestampMsec = dateTimeStr.toLongLong();
    if (timestampMsec == DATETIME_NOW)
        return microseconds{DATETIME_NOW};

    return duration_cast<microseconds>(milliseconds{timestampMsec});
}

milliseconds parseDateTimeMsec(const QString& dateTimeStr)
{
    const auto usecSinceEpoch = parseDateTimeUsec(dateTimeStr);
    if (usecSinceEpoch.count() < 0 || usecSinceEpoch == microseconds{DATETIME_NOW})
        return milliseconds{usecSinceEpoch.count()}; //< Special values are returned "as is".

    return duration_cast<milliseconds>(microseconds{usecSinceEpoch});
}

// Explicit template instantiations for common duration types
template std::string formatDateTimeLockFree(const std::chrono::seconds& durationSinceEpoch);
template std::string formatDateTimeLockFree(const std::chrono::milliseconds& durationSinceEpoch);
template std::string formatDateTimeLockFree(const std::chrono::microseconds& durationSinceEpoch);
template std::string formatDateTimeLockFree(const std::chrono::nanoseconds& durationSinceEpoch);

} // namespace nx::utils
