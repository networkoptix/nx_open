#include "log_initializer.h"

#include <nx/utils/app_info.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include "log_main.h"
#include "log_logger.h"

namespace {

static const QString kDefaultLogBaseName("log_file");

} // namespace

namespace nx {
namespace utils {
namespace log {

static std::atomic<bool> isInitializedGlobally(false);

std::unique_ptr<AbstractLogger> buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath,
    const std::set<Tag>& tags)
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

            logger->setWriter(std::make_unique<File>(fileSettings));
        }
        else
        {
            logger->setWriter(std::make_unique<StdOut>());
        }
    }

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

    return logger;
}

void initializeGlobally(const nx::utils::ArgumentParser& arguments)
{
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
}

} // namespace log
} // namespace utils
} // namespace nx
