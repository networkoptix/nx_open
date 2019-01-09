#include "datetime.h"

#include <QtCore/QDateTime>

namespace nx {
namespace utils {

using namespace std::chrono;

QString timestampToRfc2822(qint64 timestampMs)
{
    if (timestampMs == DATETIME_NOW)
        return "LIVE";

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::RFC2822Date);
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

QString timestampToDebugString(qint64 timestampMs)
{
    if (timestampMs == 0)
        return "0";

    if (timestampMs == DATETIME_NOW)
        return "LIVE";

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(timestampMs, Qt::UTC);
    return dateTime.toString("dd.MM.yyyy HH:mm:ss.zzz UTC");
}

QString timestampToDebugString(milliseconds timestamp)
{
    return timestampToDebugString(timestamp.count());
}

} // namespace utils
} // namespace nx
