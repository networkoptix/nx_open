#include "logger_builder.h"

#include <nx/utils/string.h>

#include "aggregate_logger.h"

namespace nx {
namespace utils {
namespace log {

namespace { static const QString kDefaultLogBaseName("log_file"); }

std::unique_ptr<AbstractLogger> LoggerBuilder::buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath,
    const std::set<Tag>& tags,
    std::unique_ptr<AbstractWriter> writer)
{
    std::vector<std::unique_ptr<AbstractLogger>> loggers;
    for (const auto& loggerSettings: settings.loggers)
    {
        auto logger = buildLogger(
            loggerSettings,
            tags,
            std::exchange(writer, nullptr));

        if (!logger)
            continue;

        logger->setSettings(loggerSettings);
        logger->setApplicationName(applicationName);
        logger->setBinaryPath(binaryPath);

        loggers.push_back(std::move(logger));
    }

    return std::make_unique<AggregateLogger>(std::move(loggers));
}

std::unique_ptr<Logger> LoggerBuilder::buildLogger(
    const LoggerSettings& settings,
    const std::set<Tag>& tags,
    std::unique_ptr<AbstractWriter> writer)
{
    if (settings.level.primary == Level::undefined || settings.level.primary == Level::notConfigured)
        return nullptr;

    auto logger = std::make_unique<Logger>(tags);

    logger->setDefaultLevel(settings.level.primary);
    logger->setLevelFilters(settings.level.filters);

    if (!writer)
    {
        const QString baseName = settings.logBaseName.isEmpty()
            ? kDefaultLogBaseName
            : settings.logBaseName;
        if (baseName != QLatin1String("-"))
        {
            File::Settings fileSettings;
            fileSettings.size = settings.maxFileSize;
            fileSettings.count = settings.maxBackupCount;
            fileSettings.name = settings.directory.isEmpty()
                ? baseName
                : (settings.directory + "/" + baseName);

            writer = std::make_unique<File>(fileSettings);
        }
        else
        {
            writer = std::make_unique<StdOut>();
        }
    }

    logger->setWriter(std::move(writer));

    return logger;
}

} // namespace log
} // namespace utils
} // namespace nx
