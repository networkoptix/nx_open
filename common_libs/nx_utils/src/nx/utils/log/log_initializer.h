#pragma once

#include "abstract_logger.h"
#include "log_settings.h"

namespace nx { namespace utils { class ArgumentParser; } }

namespace nx {
namespace utils {
namespace log {

NX_UTILS_API std::unique_ptr<AbstractLogger> buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath = QString(),
    const std::set<Tag>& tags = {},
    std::unique_ptr<AbstractWriter> customWriter = nullptr);

void NX_UTILS_API initializeGlobally(const nx::utils::ArgumentParser& arguments);

} // namespace log
} // namespace utils
} // namespace nx
