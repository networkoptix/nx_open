#include "log_initializer.h"

#include <nx/utils/app_info.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>

#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

static std::atomic<bool> isInitializedGlobaly(false);

void initialize(
    const Settings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& binaryPath,
    const QString& baseName,
    std::shared_ptr<Logger> logger)
{
    if (!logger)
        logger = main();

    if (settings.level == Level::undefined)
        return;

    // Can not be reinitialized if initialized globaly.
    if (!isInitializedGlobaly.load())
    {
        if (settings.level == Level::none)
            return;

        logger->setDefaultLevel(settings.level);
        logger->setExceptionFilters(settings.exceptionFilers);
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

    const auto write = [&](const Message& message) { logger->log(Level::always, QString(), message); };
    write(QByteArray(80, '='));
    write(Message("%1 started").str(applicationName));
    write(Message("Version: %1, Revision: %2").strs(
        AppInfo::applicationVersion(), AppInfo::applicationRevision()));

    if (!binaryPath.isEmpty())
        write(Message("Binary path: %1").arg(binaryPath));

    const auto filePath = logger->filePath();
    write(Message("Log level: %1, maxFileSize: %2, maxBackupCount: %3, file: %4").strs(
        settings.level, nx::utils::bytesToString(settings.maxFileSize), settings.maxBackupCount,
        filePath ? *filePath : QString::fromUtf8("-")));
}

void initializeGlobaly(const nx::utils::ArgumentParser& arguments)
{
    const auto logger = main();
    isInitializedGlobaly = true;

    if (const auto value = arguments.get("log-level", "ll"))
    {
        const auto level = levelFromString(*value);
        NX_CRITICAL(level != Level::undefined);
        logger->setDefaultLevel(level);
    }

    if (const auto value = arguments.get("log-exception-filters", "lef"))
    {
        std::set<QString> filters;
        for (const auto f: value->split(QLatin1String(",")))
            filters.insert(f);

        logger->setExceptionFilters(filters);
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
