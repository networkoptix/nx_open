#include "log_writers.h"

#include <android/log.h>

#include <QtCore/QCoreApplication>

namespace nx {
namespace utils {
namespace log {

static android_LogPriority toAndroidLogLevel(Level logLevel)
{
    switch (logLevel)
    {
        case Level::undefined:
            return ANDROID_LOG_UNKNOWN;

        case Level::none:
            return ANDROID_LOG_SILENT;

        case Level::always:
        case Level::info:
            return ANDROID_LOG_INFO;

        case Level::error:
            return ANDROID_LOG_ERROR;

        case Level::warning:
            return ANDROID_LOG_WARN;

        case Level::debug:
            return ANDROID_LOG_DEBUG;

        case Level::verbose:
            return ANDROID_LOG_VERBOSE;

        default:
            return ANDROID_LOG_DEFAULT;
    }
}

void StdOut::writeImpl(Level level, const QString& message)
{
    __android_log_print(
        toAndroidLogLevel(level),
        QCoreApplication::applicationName().toUtf8().constData(),
        "%s",
        str.toUtf8().constData());
}

} // namespace log
} // namespace utils
} // namespace nx
