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
    const QString& dataDir,
    const QString& applicationName,
    const QString& binaryPath,
    const QString& baseName,
    std::shared_ptr<Logger> logger)
{
    if (!logger)
        logger = mainLogger();

    if (settings.defaultLevel == Level::undefined)
        return;

    // Can not be reinitialized if initialized globally.
    if (!isInitializedGlobally.load())
    {
        if (settings.defaultLevel == Level::none)
            return;

        logger->setDefaultLevel(settings.defaultLevel);
        logger->setLevelFilters(settings.levelFilters);
        if (baseName != QLatin1String("-"))
        {
            const auto logDir = settings.directory.isEmpty()
                ? (dataDir + QLatin1String("/log"))
                : settings.directory;

            File::Settings fileSettings;
            fileSettings.name = dataDir.isEmpty() ? baseName : (logDir + lit("/") + baseName);
            fileSettings.size = settings.maxFileSize;
            fileSettings.count = settings.maxBackupCount;

            logger->setWriter(std::make_unique<File>(fileSettings));
        }
        else
        {
            logger->setWriter(std::make_unique<StdOut>());
        }
    }

    const auto write = [&](const Message& message) { logger->log(Level::always, "START", message); };
    write(QByteArray(80, '='));
    write(lm("%1 started, version: %2, revision: %3").args(
        applicationName, AppInfo::applicationVersion(), AppInfo::applicationRevision()));

    if (!binaryPath.isEmpty())
        write(lm("Binary path: %1").arg(binaryPath));

    const auto filePath = logger->filePath();
    write(lm("Log level: %1, maxFileSize: %2, maxBackupCount: %3, file: %4").args(
        toString(settings.defaultLevel).toUpper(), nx::utils::bytesToString(settings.maxFileSize),
        settings.maxBackupCount, filePath ? *filePath : QString::fromUtf8("-")));
}

void initializeGlobally(const nx::utils::ArgumentParser& arguments)
{
    const auto logger = mainLogger();
    isInitializedGlobally = true;

    if (const auto value = arguments.get("log-level", "ll"))
    {
        const auto level = levelFromString(*value);
        NX_CRITICAL(level != Level::undefined);
        logger->setDefaultLevel(level);
    }

    if (const auto value = arguments.get("log-level-filters", "llf"))
        logger->setLevelFilters(levelFiltersFromString(*value));

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
