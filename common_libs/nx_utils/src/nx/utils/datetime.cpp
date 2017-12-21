#include "datetime.h"

#include <QtCore/QDateTime>

namespace nx {
namespace utils {

QString timestampToRfc2822(qint64 timestampMs)
{
    return timestampMs == DATETIME_NOW
        ? lit("LIVE")
        : QDateTime::fromMSecsSinceEpoch(timestampMs).toString(Qt::RFC2822Date);
}

} // namespace utils
} // namespace nx
