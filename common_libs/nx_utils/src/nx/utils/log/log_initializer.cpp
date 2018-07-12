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

static std::atomic<bool> isInitializedGlobally(false);

//-------------------------------------------------------------------------------------------------

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
#if 1
    log::Settings logSettings;
    logSettings.load(arguments);
    setMainLogger(buildLogger(logSettings, QString()));

    // NOTE: Default log level is ensured by LevelSettings::primary default value.
#else
    const auto logger = mainLogger();
    isInitializedGlobally = true;

    bool isLogLevelSpecified = false;
    if (const auto value = arguments.get("log-level", "ll"))
    {
        LevelSettings level;
        level.parse(*value);
        logger->setDefaultLevel(level.primary);
        logger->setLevelFilters(level.filters);
        isLogLevelSpecified = true;
    }
    else
    {
        logger->setDefaultLevel(Level::none);
    }

    if (const auto value = arguments.get("log-file", "lf"))
    {
        File::Settings fileSettings;
        fileSettings.name = *value;
        fileSettings.size = 1024 * 1024 * 10;
        fileSettings.count = 5;

        logger->setWriter(std::make_unique<File>(fileSettings));

        if (!isLogLevelSpecified)
            logger->setDefaultLevel(kDefaultLevel);
    }
#endif
}

} // namespace log
} // namespace utils
} // namespace nx
