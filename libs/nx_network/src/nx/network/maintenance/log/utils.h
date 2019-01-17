#pragma once

#include <nx/utils/log/log_settings.h>
#include <nx/utils/log/abstract_logger.h>

#include "logger.h"

namespace nx::network::maintenance::log::utils {

nx::utils::log::LoggerSettings toLoggerSettings(const Logger& loggerInfo);

std::set<nx::utils::log::Tag> toTags(const std::vector<Filter>& filters);

nx::utils::log::LevelFilters toLevelFilters(const std::vector<Filter>& filters);

std::vector<Filter> toEffectiveFilters(
    const std::set<nx::utils::log::Tag>& effectiveTsags,
    const nx::utils::log::LevelFilters& levelFilters);

Logger toLoggerInfo(
    const std::shared_ptr<nx::utils::log::AbstractLogger>& logger,
    const std::set<nx::utils::log::Tag>& effectiveTags,
    int id);

} // namespace nx::network::maintenance::log::utils