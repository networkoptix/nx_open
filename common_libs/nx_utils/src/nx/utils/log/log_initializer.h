#pragma once

#include "log_logger.h"
#include "log_settings.h"

namespace nx { namespace utils { class ArgumentParser; } }

namespace nx {
namespace utils {
namespace log {

void NX_UTILS_API initialize(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath = QString(),
    std::shared_ptr<Logger> logger = nullptr,
    bool writeInitInformation = true);

void NX_UTILS_API initializeGlobally(const nx::utils::ArgumentParser& arguments);

} // namespace log
} // namespace utils
} // namespace nx
