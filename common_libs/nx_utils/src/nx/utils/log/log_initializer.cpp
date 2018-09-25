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
    bool isDefaultLoggerUsed = true;
    bool defaultLoggerModified = false;

    if (const auto value = arguments.get("log-file", "lf"))
    {
        log::Settings logSettings;
        logSettings.loggers.resize(1);
        logSettings.loggers.front().logBaseName = *value;
        setMainLogger(buildLogger(logSettings, QString()));
        isDefaultLoggerUsed = false;
        defaultLoggerModified = true;
    }

    if (const auto value = arguments.get("log-level", "ll"))
    {
        LevelSettings level;
        level.parse(*value);
        mainLogger()->setDefaultLevel(level.primary);
        mainLogger()->setLevelFilters(level.filters);
        defaultLoggerModified = true;
    }
    else if (isDefaultLoggerUsed)
    {
        mainLogger()->setDefaultLevel(Level::none);
    }

    log::Settings logSettings;
    logSettings.load(QnSettings(arguments));
    if (!logSettings.loggers.empty())
    {
        auto logger = buildLogger(logSettings, QString());
        if (!defaultLoggerModified)
            setMainLogger(std::move(logger));
        else
            addLogger(std::move(logger));
    }
}

} // namespace log
} // namespace utils
} // namespace nx
