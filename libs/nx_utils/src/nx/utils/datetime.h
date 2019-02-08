#pragma once

#include <chrono>

#include <QtCore/QString>

/** Time value for 'now'. */
static const qint64 DATETIME_NOW = std::numeric_limits<qint64>::max();

/** Time value for 'unknown' / 'invalid'. Same as AV_NOPTS_VALUE.

    avutil.h
    #define AV_NOPTS_VALUE          INT64_C(0x8000000000000000)
*/
static const qint64 DATETIME_INVALID = std::numeric_limits<qint64>::min();

namespace nx {
namespace utils {

QString NX_UTILS_API timestampToRfc2822(qint64 timestampMs);
QString NX_UTILS_API timestampToRfc2822(std::chrono::milliseconds timestamp);
QString NX_UTILS_API timestampToRfc2822(std::chrono::system_clock::time_point timestamp);

QString NX_UTILS_API timestampToDebugString(qint64 timestampMs);
QString NX_UTILS_API timestampToDebugString(std::chrono::milliseconds timestamp);

} // namespace utils
} // namespace nx
