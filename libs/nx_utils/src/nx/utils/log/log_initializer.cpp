#include "log_initializer.h"

#include <nx/utils/argument_parser.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/std/cpp14.h>

#include "logger_builder.h"
#include "log_main.h"
#include "log_logger.h"

namespace nx {
namespace utils {
namespace log {

std::unique_ptr<AbstractLogger> buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath,
    const std::set<Tag>& tags,
    std::unique_ptr<AbstractWriter> writer)
{
    return LoggerBuilder::buildLogger(
        settings,
        applicationName,
        binaryPath,
        tags,
        std::move(writer));
}

void initializeGlobally(const nx::utils::ArgumentParser& arguments)
{
    log::Settings settings;
    settings.load(QnSettings(arguments), "log"/*, deafultCompatibilityLevel "none"*/);
    for (auto& logger: settings.loggers)
    {
        if (logger.logBaseName.isEmpty())
            logger.logBaseName = "-"; // < Default output is console.
    }

    if (auto logger = buildLogger(settings, QString()))
        setMainLogger(std::move(logger));

    lockConfiguration();
}

} // namespace log
} // namespace utils
} // namespace nx
