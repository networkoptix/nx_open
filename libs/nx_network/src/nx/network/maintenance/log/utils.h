#pragma once

#include <nx/utils/log/log_settings.h>
#include <nx/utils/log/abstract_logger.h>

#include "logger.h"

namespace nx::network::maintenance::log::utils {

NX_NETWORK_API nx::utils::log::LoggerSettings toLoggerSettings(const Logger& loggerInfo);

NX_NETWORK_API std::set<nx::utils::log::Tag> toTags(const std::vector<Filter>& filters);

NX_NETWORK_API std::set<nx::utils::log::Tag> toTags(const nx::utils::log::LevelFilters& levelFilters);

NX_NETWORK_API nx::utils::log::LevelFilters toLevelFilters(const std::vector<Filter>& filters);

NX_NETWORK_API std::vector<Filter> toEffectiveFilters(
    const std::set<nx::utils::log::Tag>& effectiveTsags,
    const nx::utils::log::LevelFilters& levelFilters);

NX_NETWORK_API Logger toLoggerInfo(
    const std::shared_ptr<nx::utils::log::AbstractLogger>& logger,
    const std::set<nx::utils::log::Tag>& effectiveTags,
    int id);

} // namespace nx::network::maintenance::log::utils