// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aggregate_logger.h"

namespace nx::log {

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

void AggregateLogger::setSettings(const LoggerSettings& settings)
{
    for (auto& logger: m_loggers)
        logger->setSettings(settings);
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

cf::future<cf::unit> AggregateLogger::stopArchivingAsync()
{
    std::vector<cf::future<cf::unit>> futures;
    for (auto& logger: m_loggers)
    {
        futures.push_back(logger->stopArchivingAsync());
    }

    return cf::initiate(
        [futures = std::move(futures)]() mutable
        {
            for (auto& future: futures)
                future.get();
            return cf::unit();
        });
}

void AggregateLogger::writeLogHeader()
{
    for (auto& logger: m_loggers)
        logger->writeLogHeader();
}

const std::vector<std::unique_ptr<AbstractLogger>>& AggregateLogger::loggers() const
{
    return m_loggers;
}

} // namespace nx::log
