#pragma once

#include <memory>

#include <QtCore/QString>

#include "abstract_logger.h"
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
        const std::set<Tag>& tags,
        std::unique_ptr<AbstractWriter> writer);

private:
    static std::unique_ptr<AbstractLogger> buildLogger(
        const LoggerSettings& settings,
        const std::set<Tag>& tags,
        std::unique_ptr<AbstractWriter> writer);

    static void printStartMessagesToLogger(
        AbstractLogger* logger,
        const LoggerSettings& settings,
        const QString& applicationName,
        const QString& binaryPath);
};

} // namespace log
} // namespace utils
} // namespace nx
