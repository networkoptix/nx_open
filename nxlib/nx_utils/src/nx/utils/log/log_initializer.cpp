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
    log::Settings logSettings;
    logSettings.load(QnSettings(arguments));
    const auto mainSettings =
        [&logSettings]()
        {
            if (logSettings.loggers.empty())
                logSettings.loggers.push_back(LoggerSettings());

            return logSettings.loggers.front();
        };

    if (const auto value = arguments.get("log-file", "lf"))
        mainSettings().logBaseName = *value;

    if (const auto value = arguments.get("log-level", "ll"))
        mainSettings().level.parse(*value);

    if (logSettings.loggers.empty())
    {
        if (auto logger = buildLogger(logSettings, QString()))
            setMainLogger(std::move(logger));
    }
}

} // namespace log
} // namespace utils
} // namespace nx
