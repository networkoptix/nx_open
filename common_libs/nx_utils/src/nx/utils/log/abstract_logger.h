#pragma once

#include <functional>
#include <set>

#include <nx/utils/std/optional.h>

#include "log_writers.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API AbstractLogger
{
public:
    using OnLevelChanged = std::function<void()>;

    virtual ~AbstractLogger() = default;

    virtual std::set<Tag> tags() const = 0;

    /** Writes message to every writer if it is to be logged. */
    virtual void log(Level level, const Tag& tag, const QString& message) = 0;

    /** Writes message to every writer unconditionally. */
    virtual void logForced(Level level, const Tag& tag, const QString& message) = 0;

    /** Indicates whether this logger is going to log such a message. */
    virtual bool isToBeLogged(Level level, const Tag& tag = {}) = 0;

    /** Makes this logger log all messages with level <= defaultLevel. */
    virtual Level defaultLevel() const = 0;
    virtual void setDefaultLevel(Level level) = 0;

    /** Custom levels for messages which tag starting with one of the filters. */
    virtual LevelFilters levelFilters() const = 0;
    virtual void setLevelFilters(LevelFilters filters) = 0;

    /** @return Maximum possible log level, according to the current settings. */
    virtual Level maxLevel() const = 0;

    virtual void setOnLevelChanged(OnLevelChanged onLevelChanged) = 0;

    virtual std::optional<QString> filePath() const = 0;

    virtual void writeLogHeader() = 0;
};

} // namespace log
} // namespace utils
} // namespace nx
