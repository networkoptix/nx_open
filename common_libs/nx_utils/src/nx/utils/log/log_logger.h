#pragma once

#include <set>

#include "log_writers.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API Logger
{
public:
    typedef std::function<void()> OnLevelChanged;

    /** Initializes log with minimal level and writers, no writers means std out and err. */
    Logger(
        Level defaultLevel = Level::none,
        std::unique_ptr<AbstractWriter> writer = nullptr,
        OnLevelChanged onLevelChanged = nullptr);

    /** Writes message to every writer if it is to be logged. */
    void log(Level level, const Tag& tag, const QString& message);

    /** Writes message to every writer unconditionally. */
    void logForced(Level level, const Tag& tag, const QString& message);

    /** Indicates whether this logger is going to log such a message. */
    bool isToBeLogged(Level level, const Tag& tag = {});

    /** Makes this logger log all messages with level <= defaultLevel. */
    Level defaultLevel() const;
    void setDefaultLevel(Level level);

    /** Custom levels for messages which tag starting with one of the filters. */
    LevelFilters levelFilters() const;
    void setLevelFilters(LevelFilters filters);

    /** @return Maximum possible log level, according to the current settings. */
    Level maxLevel() const;

    void setWriters(std::vector<std::unique_ptr<AbstractWriter>> writers = {});
    void setWriter(std::unique_ptr<AbstractWriter> writer);

    boost::optional<QString> filePath() const;

private:
    mutable QnMutex m_mutex;
    const OnLevelChanged m_onLevelChanged;
    Level m_defaultLevel;
    std::vector<std::unique_ptr<AbstractWriter>> m_writers;
    LevelFilters m_levelFilters;
};

} // namespace log
} // namespace utils
} // namespace nx
