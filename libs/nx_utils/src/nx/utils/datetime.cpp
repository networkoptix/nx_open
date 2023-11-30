// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "datetime.h"

#include <QtCore/QDateTime>

#include "nx_utils_ini.h"

namespace nx {
namespace utils {

using namespace std::chrono;

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

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::UTC);
    return dateTime.toString(format.isEmpty() ? defaultFormat : format);
}

QString timestampToDebugString(milliseconds timestamp, const QString& format)
{
    return timestampToDebugString(timestamp.count(), format);
}

} // namespace utils
} // namespace nx
