#pragma once

#include "abstract_logger.h"
#include "log_settings.h"

namespace nx { namespace utils { class ArgumentParser; } }

namespace nx {
namespace utils {
namespace log {

/**
 * Builds logger with all filtering specified in settings.
 * NOTE: If settings.loggers actually contains multiple elements,
 * then this function instantiates AggregateLogger that manages
 * multiple regular loggers.
 */
NX_UTILS_API std::unique_ptr<AbstractLogger> buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath = QString(),
    std::set<Filter> filters = {},
    std::unique_ptr<AbstractWriter> customWriter = nullptr);

void NX_UTILS_API initializeGlobally(const nx::utils::ArgumentParser& arguments);

} // namespace log
} // namespace utils
} // namespace nx
