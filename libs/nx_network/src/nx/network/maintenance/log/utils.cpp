#include "utils.h"

#include <nx/utils/log/log_level.h>

namespace nx::network::maintenance::log::utils {

using namespace nx::utils::log;

namespace {

std::map<Level, std::vector<Tag>> getEffectiveTagsByLevel(
    const std::set<Tag>& effectiveTags,
    const LevelFilters& levelFilters)
{
    std::map<Level, std::vector<Tag>> effectiveTagsByLevel;
    for (const auto& filter : levelFilters)
    {
        auto& tags = effectiveTagsByLevel[filter.second];
        if (effectiveTags.find(filter.first) != effectiveTags.end())
            tags.push_back(filter.first);
    }

    return effectiveTagsByLevel;
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

std::set<Tag> toTags(const std::vector<Filter>& filters)
{
    std::set<Tag> tags;
    for (const auto& filter : filters)
    {
        for (const auto& tag : filter.tags)
            tags.emplace(tag);
    }

    return tags;
}

LevelFilters toLevelFilters(const std::vector<Filter>& filters)
{
    LevelFilters levelFilters;
    for (const auto& filter : filters)
    {
        for (const auto& tag : filter.tags)
            levelFilters.emplace(tag, levelFromString(filter.level.c_str()));
    }

    return levelFilters;
}

std::vector<Filter> toEffectiveFilters(
    const std::set<Tag>& effectiveTags,
    const LevelFilters& levelFilters)
{
    auto effectiveTagsByLevel = getEffectiveTagsByLevel(effectiveTags, levelFilters);

    std::vector<Filter> effectiveFilters;
    for (const auto& element : effectiveTagsByLevel)
    {
        Filter filter;
        filter.level = toString(element.first).toStdString();

        for (const auto& tag : element.second)
            filter.tags.push_back(tag.toString().toStdString());

        effectiveFilters.push_back(filter);
    }

    return effectiveFilters;
}

Logger toLoggerInfo(
    const std::shared_ptr<AbstractLogger>& logger,
    const std::set<Tag>& effectiveTags,
    int id)
{
    Logger loggerInfo;
    loggerInfo.id = id;

    auto filePath = logger->filePath();
    if (filePath.has_value())
         loggerInfo.path = filePath->toStdString();

    loggerInfo.filters = toEffectiveFilters(effectiveTags, logger->levelFilters());

    loggerInfo.defaultLevel = toString(logger->defaultLevel()).toStdString();

    return loggerInfo;
}

} // namespace nx::network::maintenance::log::utils