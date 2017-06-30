#include "log_level.h"

#include "assert.h"
#include "log_message.h"

namespace nx {
namespace utils {
namespace log {

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == QLatin1String("none"))
        return Level::none;

    if (level == QLatin1String("always"))
        return Level::always;

    if (level == QLatin1String("error"))
        return Level::error;

    if (level == QLatin1String("warning"))
        return Level::warning;

    if (level == QLatin1String("info"))
        return Level::info;

    if (level == QLatin1String("debug") || level == QLatin1String("debug1"))
        return Level::debug;

    if (level == QLatin1String("verbose") || level == QLatin1String("debug2"))
        return Level::verbose;

    return Level::undefined;
}

QString toString(Level level)
{
    switch (level)
    {
        case Level::undefined:
            return QLatin1String("undefined");

        case Level::none:
            return QLatin1String("none");

        case Level::always:
            return QLatin1String("always");

        case Level::error:
            return QLatin1String("error");

        case Level::warning:
            return QLatin1String("warning");

        case Level::info:
            return QLatin1String("info");

        case Level::debug:
            return QLatin1String("debug");

        case Level::verbose:
            return QLatin1String("verbose");
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return QLatin1String("unknown");
}

} // namespace log
} // namespace utils
} // namespace nx
