#pragma once

#include <memory>

#include <QtCore/QString>

#include "log_logger.h"
#include "log_settings.h"
#include "log_writers.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API LoggerBuilder
{
public:
    static std::unique_ptr<AbstractLogger> buildLogger(
        const Settings& settings,
        const QString& applicationName,
        const QString& binaryPath,
        std::set<Filter> filters,
        std::unique_ptr<AbstractWriter> writer);

private:
    static std::unique_ptr<Logger> buildLogger(
        const LoggerSettings& settings,
        std::set<Filter> filters,
        std::unique_ptr<AbstractWriter> writer);
};

} // namespace log
} // namespace utils
} // namespace nx
