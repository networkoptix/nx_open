#pragma once

#include <set>

#include "log_writers.h"

namespace nx {
namespace utils {
namespace log {

enum class Level
{
    none,
    always,
    error,
    warning,
    info,
    debug,
    verbose
};

Level NX_UTILS_API levelFromString(const QString& levelString);
QString NX_UTILS_API toString(Level level);

class NX_UTILS_API Logger
{
public:
    /** Initializes log with minimal level and writers, no writers means std out and err. */
    Logger(Level level = Level::none, std::vector<std::unique_ptr<AbstractWriter>> writers = {});

    /** Writes message to all writer if level <= log level or tar is present in exceptions. */
    void log(Level level, const QString& tag, const QString& message);

    /** Indicates if is this logger going to log such message. */
    bool isToBeLogged(Level level, const QString& tag);

    Level defaultLevel();
    void setDefaultLevel(Level level);

    std::set<QString> exceptionFilters();
    void setExceptionFilters(std::set<QString> filters);

    void setWriters(std::vector<std::unique_ptr<AbstractWriter>> writers);

private:
    QnMutex m_mutex;
    Level m_defaultLevel = Level::none;
    std::vector<std::unique_ptr<AbstractWriter>> m_writers;
    std::set<QString> m_exceptionFilters;
};

} // namespace log
} // namespace utils
} // namespace nx
