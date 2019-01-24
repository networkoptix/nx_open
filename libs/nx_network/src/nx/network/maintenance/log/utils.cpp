#include "utils.h"

#include <nx/utils/log/log_level.h>

namespace nx::network::maintenance::log::utils {

using namespace nx::utils::log;

namespace {

std::map<Level, std::vector<nx::utils::log::Filter>> getEffectiveFiltersByLevel(
    const std::set<nx::utils::log::Filter>& effectiveFilters,
    const LevelFilters& levelFilters)
{
    std::map<Level, std::vector<nx::utils::log::Filter>> effectiveFiltersByLevel;
    for (const auto& filter: levelFilters)
    {
        auto& tags = effectiveFiltersByLevel[filter.second];
        if (effectiveFilters.find(filter.first) != effectiveFilters.end())
            tags.push_back(filter.first);
    }

    return effectiveFiltersByLevel;
}

} // namespace

LoggerSettings toLoggerSettings(const Logger& loggerInfo)
{
    LoggerSettings settings;
    settings.level.filters = toLevelFilters(loggerInfo.filters);
    settings.level.primary = loggerInfo.defaultLevel.empty()
        ? Level::none
        : levelFromString(loggerInfo.defaultLevel.c_str());

    size_t finalSlash = loggerInfo.path.find_last_of('/');
    if (finalSlash == std::string::npos)
    {
        settings.logBaseName = QString::fromStdString(loggerInfo.path);
    }
    else
    {
        settings.directory = QString::fromStdString(loggerInfo.path.substr(0, finalSlash));
        if (finalSlash < loggerInfo.path.size() - 1)
            settings.logBaseName = QString::fromStdString(loggerInfo.path.substr(finalSlash + 1));
    }

    return settings;
}

std::set<nx::utils::log::Filter> toFilters(const std::vector<Filter>& filters)
{
    std::set<nx::utils::log::Filter> logFilters;
    for (const auto& filter: filters)
    {
        // TODO: #Nate make sure you want regexp here.
        for (const auto& tag: filter.tags)
            logFilters.emplace(QString::fromStdString(tag), /*regexp*/ true); 
    }

    return logFilters;
}

std::set<nx::utils::log::Filter> toFilters(const nx::utils::log::LevelFilters& levelFilters)
{
    std::set<nx::utils::log::Filter> logFilters;
    for (const auto& [filter, level]: levelFilters)
        logFilters.insert(filter);

    return logFilters;
}

LevelFilters toLevelFilters(const std::vector<Filter>& filters)
{
    LevelFilters levelFilters;
    for (const auto& filter: filters)
    {
        for (const auto& tag: filter.tags)
        {
            // TODO: #Nate make sure you want regexp here.
            nx::utils::log::Filter logFilter(QString::fromStdString(tag), /*regexp*/ true);
            levelFilters.emplace(logFilter, levelFromString(filter.level.c_str()));
        }
    }

    return levelFilters;
}

std::vector<Filter> toEffectiveFilters(
    const std::set<nx::utils::log::Filter>& effectiveFilters,
    const LevelFilters& levelFilters)
{
    auto effectiveTagsByLevel = getEffectiveFiltersByLevel(effectiveFilters, levelFilters);

    std::vector<Filter> effectiveLocalFilters;
    for (const auto& element: effectiveTagsByLevel)
    {
        Filter filter;
        filter.level = toString(element.first).toStdString();

        for (const auto& tag: element.second)
            filter.tags.push_back(tag.toString().toStdString());

        effectiveLocalFilters.push_back(filter);
    }

    return effectiveLocalFilters;
}

Logger toLoggerInfo(
    const std::shared_ptr<AbstractLogger>& logger,
    const std::set<nx::utils::log::Filter>& effectiveFilters,
    int id)
{
    Logger loggerInfo;
    loggerInfo.id = id;

    auto filePath = logger->filePath();
    if (filePath.has_value())
        loggerInfo.path = filePath->toStdString();

    loggerInfo.filters = toEffectiveFilters(effectiveFilters, logger->levelFilters());

    loggerInfo.defaultLevel = toString(logger->defaultLevel()).toStdString();

    return loggerInfo;
}

} // namespace nx::network::maintenance::log::utils
