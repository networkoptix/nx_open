#include "log_level.h"

#include "assert.h"
#include "log_message.h"

#include <nx/utils/literal.h>

namespace nx {
namespace utils {
namespace log {

bool operator<=(Level left, Level right)
{
    return static_cast<int>(left) <= static_cast<int>(right);
}

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == lit("none") || level == lit("n"))
        return Level::none;

    if (level == lit("always") || level == lit("a"))
        return Level::always;

    if (level == lit("error") || level == lit("e"))
        return Level::error;

    if (level == lit("warning") || level == lit("w"))
        return Level::warning;

    if (level == lit("info") || level == lit("i"))
        return Level::info;

    if (level == lit("debug") || level == lit("debug1") || level == lit("d"))
        return Level::debug;

    if (level == lit("verbose") || level == lit("debug2") || level == lit("v"))
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

LevelFilters levelFiltersFromString(const QString& levelFilters)
{
    LevelFilters filters;
    for (const auto& group: levelFilters.splitRef(';', QString::SkipEmptyParts))
    {
        const auto parts = group.split('-');
        if (parts.size() == 0 || parts.size() > 2)
        {
            qWarning() << Q_FUNC_INFO << "wrong format in group" << group << "in" << levelFilters;
            continue;
        }

        auto level = Level::verbose;
        if (parts.size() == 2)
        {
            const auto levelPart = parts[1].toString();
            const auto parsedLevel = levelFromString(levelPart);
            if (parsedLevel == Level::undefined)
            {
                qWarning() << Q_FUNC_INFO << "invalid level" << levelPart << "in group" << group
                    << "in" << levelFilters;
                continue;
            }

            level = parsedLevel;
        }

        const auto nameParts = parts[0].split(',');
        for (const auto& part: nameParts)
        {
            const auto name = part.toString();
            if (filters.count(name))
            {
                qWarning() << Q_FUNC_INFO << "duplicate name" << name << "in group" << group
                    << "in" << levelFilters;
                continue;
            }

            filters.emplace(name, level);
        }
    }

    return filters;
}

} // namespace log
} // namespace utils
} // namespace nx
