#include "aggregate_logger.h"

namespace nx {
namespace utils {
namespace log {

AggregateLogger::AggregateLogger(
    std::vector<std::unique_ptr<AbstractLogger>> loggers)
    :
    m_loggers(std::move(loggers))
{
}

std::set<Filter> AggregateLogger::filters() const
{
    std::set<Filter> filters;

    for (auto& logger: m_loggers)
    {
        auto loggerFilters = logger->filters();
        filters.insert(loggerFilters.begin(), loggerFilters.end());
    }

    return filters;
}

void AggregateLogger::log(Level level, const Tag& tag, const QString& message)
{
    for (auto& logger: m_loggers)
    {
        if (logger->isToBeLogged(level, tag))
            logger->logForced(level, tag, message);
    }
}

void AggregateLogger::logForced(Level level, const Tag& tag, const QString& message)
{
    for (auto& logger: m_loggers)
    {
        if (logger->isToBeLogged(level, tag))
            logger->logForced(level, tag, message);
    }
}

bool AggregateLogger::isToBeLogged(Level level, const Tag& tag)
{
    for (auto& logger: m_loggers)
    {
        if (logger->isToBeLogged(level, tag))
            return true;
    }

    return false;
}

Level AggregateLogger::defaultLevel() const
{
    Level defaultLevel = Level::none;
    for (auto& logger: m_loggers)
        defaultLevel = std::max<Level>(defaultLevel, logger->defaultLevel());

    return defaultLevel;
}

void AggregateLogger::setDefaultLevel(Level level)
{
    for (auto& logger: m_loggers)
        logger->setDefaultLevel(level);
}

LevelFilters AggregateLogger::levelFilters() const
{
    LevelFilters levelFilters;

    for (auto& logger: m_loggers)
    {
        auto loggerLevelFilters = logger->levelFilters();
        levelFilters.insert(loggerLevelFilters.begin(), loggerLevelFilters.end());
    }

    return levelFilters;
}

void AggregateLogger::setLevelFilters(LevelFilters filters)
{
    for (auto& logger: m_loggers)
        logger->setLevelFilters(filters);
}

Level AggregateLogger::maxLevel() const
{
    Level maxLevel = Level::none;
    for (auto& logger: m_loggers)
        maxLevel = std::max<Level>(maxLevel, logger->maxLevel());

    return maxLevel;
}

void AggregateLogger::setOnLevelChanged(OnLevelChanged onLevelChanged)
{
    for (auto& logger: m_loggers)
        logger->setOnLevelChanged(onLevelChanged);
}

std::optional<QString> AggregateLogger::filePath() const
{
    for (auto& logger: m_loggers)
    {
        if (logger->filePath())
            return logger->filePath();
    }

    return std::nullopt;
}

void AggregateLogger::writeLogHeader()
{
    for (auto& logger: m_loggers)
        logger->writeLogHeader();
}

} // namespace log
} // namespace utils
} // namespace nx
