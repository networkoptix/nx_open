#include "logger_builder.h"

#include <nx/utils/app_info.h>
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
    for (const auto& loggerSettings : settings.loggers)
    {
        auto logger = buildLogger(
            loggerSettings,
            tags,
            std::exchange(writer, nullptr));

        if (!logger)
            continue;

        printStartMessagesToLogger(
            logger.get(),
            loggerSettings,
            applicationName,
            binaryPath);
        loggers.push_back(std::move(logger));
    }

    return std::make_unique<AggregateLogger>(std::move(loggers));
}

std::unique_ptr<AbstractLogger> LoggerBuilder::buildLogger(
    const LoggerSettings& settings,
    const std::set<Tag>& tags,
    std::unique_ptr<AbstractWriter> writer)
{
    if (settings.level.primary == Level::undefined || settings.level.primary == Level::notConfigured)
        return nullptr;

    auto logger = std::make_unique<Logger>(tags);

    // Cannot be reinitialized if initialized globally.
    // Why can't I set writer to my custom logger if mainLogger has already been initialized? What's the point?
    //if (!isInitializedGlobally.load())
    {
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
    }

    return logger;
}

void LoggerBuilder::printStartMessagesToLogger(
    AbstractLogger* logger,
    const LoggerSettings& settings,
    const QString& applicationName,
    const QString& binaryPath)
{
    const nx::utils::log::Tag kStart(QLatin1String("START"));
    const auto write = [&](const Message& message) { logger->log(Level::always, kStart, message); };
    write(QByteArray(80, '='));
    write(lm("%1 started, version: %2, revision: %3").args(
        applicationName, AppInfo::applicationVersion(), AppInfo::applicationRevision()));

    if (!binaryPath.isEmpty())
        write(lm("Binary path: %1").arg(binaryPath));

    const auto filePath = logger->filePath();
    write(lm("Log level: %1").arg(settings.level));
    write(lm("Log file size: %2, backup count: %3, file: %4").args(
        nx::utils::bytesToString(settings.maxFileSize), settings.maxBackupCount,
        filePath ? *filePath : QString("-")));
}

} // namespace log
} // namespace utils
} // namespace nx
