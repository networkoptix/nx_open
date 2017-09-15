#include "log_initializer.h"

#include <nx/utils/app_info.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>

#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

static std::atomic<bool> isInitializedGlobally(false);

void initialize(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath,
    const QString& baseName,
    std::shared_ptr<Logger> logger)
{
    if (settings.level.primary == Level::undefined || settings.level.primary == Level::notConfigured)
        return;

    if (!logger)
        logger = mainLogger();

    // Can not be reinitialized if initialized globally.
    if (!isInitializedGlobally.load())
    {
        logger->setDefaultLevel(settings.level.primary);
        logger->setLevelFilters(settings.level.filters);
        if (baseName != QLatin1String("-"))
        {
            File::Settings fileSettings;
            fileSettings.size = settings.maxFileSize;
            fileSettings.count = settings.maxBackupCount;
            fileSettings.name = settings.directory.isEmpty()
                ? baseName
                : (settings.directory + lit("/") + baseName);

            logger->setWriter(std::make_unique<File>(fileSettings));
        }
        else
        {
            logger->setWriter(std::make_unique<StdOut>());
        }
    }

    const nx::utils::log::Tag kStart(lit("START"));
    const auto write = [&](const Message& message) { logger->log(Level::always, kStart, message); };
    write(QByteArray(80, '='));
    write(lm("%1 started, version: %2, revision: %3").args(
        applicationName, AppInfo::applicationVersion(), AppInfo::applicationRevision()));

    if (!binaryPath.isEmpty())
        write(lm("Binary path: %1").arg(binaryPath));

    const auto filePath = logger->filePath();
    write(lm("Log level: %1").arg(settings.level));
    write(lm("Log maxFileSize: %2, maxBackupCount: %3, file: %4").args(
        nx::utils::bytesToString(settings.maxFileSize), settings.maxBackupCount,
        filePath ? *filePath : lit("-")));
}

void initializeGlobally(const nx::utils::ArgumentParser& arguments)
{
    const auto logger = mainLogger();
    isInitializedGlobally = true;

    if (const auto value = arguments.get("log-level", "ll"))
    {
        LevelSettings level;
        level.parse(*value);
        logger->setDefaultLevel(level.primary);
        logger->setLevelFilters(level.filters);
    }

    if (const auto value = arguments.get("log-file", "lf"))
    {
        File::Settings fileSettings;
        fileSettings.name = *value;
        fileSettings.size = 1024 * 1024 * 10;
        fileSettings.count = 5;

        logger->setWriter(std::make_unique<File>(fileSettings));
    }
}

} // namespace log
} // namespace utils
} // namespace nx
