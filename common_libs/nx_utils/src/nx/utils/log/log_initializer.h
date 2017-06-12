#pragma once

#include "log_logger.h"
#include "log_settings.h"

namespace nx { namespace utils { class ArgumentParser; } }

namespace nx {
namespace utils {
namespace log {

void NX_UTILS_API initialize(
    const Settings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& binaryPath = QString(),
    const QString& baseName = QLatin1String("log_file"),
    std::shared_ptr<Logger> logger = nullptr);

void NX_UTILS_API initializeGlobally(const nx::utils::ArgumentParser& arguments);

} // namespace log
} // namespace utils
} // namespace nx
