// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <QtCore/QDateTime>
#include <QtCore/QString>

/** Time value for 'now'. */
// TODO: #lbusygin change it to kLivePosition, it will help to avoid conversion problem ms -> usec
static const qint64 DATETIME_NOW = std::numeric_limits<qint64>::max();

/** Time value for 'unknown' / 'invalid'. Same as AV_NOPTS_VALUE.

    avutil.h
    #define AV_NOPTS_VALUE          INT64_C(0x8000000000000000)
*/
static const qint64 DATETIME_INVALID = std::numeric_limits<qint64>::min();

namespace nx::utils {

QString NX_UTILS_API timestampToRfc2822(qint64 timestampMs);
QString NX_UTILS_API timestampToRfc2822(std::chrono::milliseconds timestamp);
QString NX_UTILS_API timestampToRfc2822(std::chrono::system_clock::time_point timestamp);
QString NX_UTILS_API timestampToISO8601(std::chrono::microseconds timestamp);

QString NX_UTILS_API timestampToDebugString(qint64 timestampMs,
    const QString& format = QString());
QString NX_UTILS_API timestampToDebugString(std::chrono::milliseconds timestamp,
    const QString& format = QString());

/**
 * Convert QDateTime to HTTP header date format according to RFC 1123#5.2.14.
 */
NX_UTILS_API std::string formatDateTime(const QDateTime& value);

/**
 * Parses the http Date according to RFC 2616#3.3 Date/Time Formats.
 *
 * NOTE: There are 3 different formats, though 2 and 3 are deprecated by HTTP 1.1 standard
 * The order in which parsing is attempted is:
 * 1. Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 * 2. Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850
 * 3. Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 */
NX_UTILS_API QDateTime parseDateToQDateTime(const std::string_view& str);

NX_UTILS_API std::string formatDateTime(std::chrono::system_clock::time_point tp);

NX_UTILS_API std::chrono::system_clock::time_point parseDateTime(std::string_view datetime);

NX_UTILS_API std::string formatDateTime(std::chrono::steady_clock::time_point tp);

/**
 * @param dateTime Can be one of following:
 * - millis since since 1970-01-01
 * - date in ISO format (YYYY-MM-DDTHH:mm:ss)
 * - special value "now". In this case DATETIME_NOW is returned
 * - negative value. In this case value returned "as is"
 * @return msec since epoch
*/
NX_UTILS_API qint64 parseDateTimeMsec(const QString& dateTimeStr);

/** Same as parseDateTimeMsec(), but return usec. */
NX_UTILS_API qint64 parseDateTimeUsec(const QString& dateTimeStr);

} // namespace nx::utils
