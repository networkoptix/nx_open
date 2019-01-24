#pragma once

#include <nx/utils/log/log_settings.h>
#include <nx/utils/log/abstract_logger.h>

#include "logger.h"

namespace nx::network::maintenance::log::utils {

NX_NETWORK_API nx::utils::log::LoggerSettings toLoggerSettings(const Logger& loggerInfo);

NX_NETWORK_API std::set<nx::utils::log::Filter> toFilters(const std::vector<Filter>& filters);

/**
 * TODO: #Nate
 * This method is semantically strange and not recommended for usage. LevelFilters mechanism is
 * much more powerful than simple filters, so it cannot be converted 1-to-1.
 */
NX_NETWORK_API std::set<nx::utils::log::Filter> toFilters(
    const nx::utils::log::LevelFilters& levelFilters);

NX_NETWORK_API nx::utils::log::LevelFilters toLevelFilters(const std::vector<Filter>& filters);

NX_NETWORK_API std::vector<Filter> toEffectiveFilters(
    const std::set<nx::utils::log::Filter>& effectiveFilters,
    const nx::utils::log::LevelFilters& levelFilters);

NX_NETWORK_API Logger toLoggerInfo(
    const std::shared_ptr<nx::utils::log::AbstractLogger>& logger,
    const std::set<nx::utils::log::Filter>& effectiveFilters,
    int id);

} // namespace nx::network::maintenance::log::utils