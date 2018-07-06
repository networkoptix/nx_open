#pragma once

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

} // namespace utils
} // namespace nx
