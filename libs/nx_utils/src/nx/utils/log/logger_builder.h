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
    /**
     * Create set of loggers, working in the cooperative mode and combined to a single aggregate
     * logger.
     * @param settings Settings for all group loggers.
     * @param applicationName Application name for the log header.
     * @param binaryPath Path to the application executable for the log header.
     * @param filters Set of filters, which will be used for exclusive mode logger selection.
     * @param writer Special writer, which will be added to the first logger (only).
     */
    static std::unique_ptr<AbstractLogger> buildLogger(
        const Settings& settings,
        const QString& applicationName,
        const QString& binaryPath,
        const std::set<Filter>& filters,
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
