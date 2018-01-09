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
    if (!logger)
        logger = mainLogger();

    if (settings.level == Level::undefined)
        return;

    // Can not be reinitialized if initialized globally.
    if (!isInitializedGlobally.load())
    {
        logger->setDefaultLevel(settings.level);
        logger->setExceptionFilters(settings.exceptionFilers);
        if (baseName != QLatin1String("-"))
        {
            File::Settings fileSettings;
            fileSettings.size = settings.maxFileSize;
            fileSettings.count = settings.maxBackupCount;
            fileSettings.name = settings.directory.isEmpty()
                ? baseName : (settings.directory + lit("/") + baseName);

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
    write(lm("Log level: %1, file size: %2, backup count: %3, file: %4").args(
        toString(settings.level).toUpper(), nx::utils::bytesToString(settings.maxFileSize),
        settings.maxBackupCount, filePath ? *filePath : QString::fromUtf8("-")));

    if (!settings.exceptionFilers.empty())
        write(lm("Filters: %1").container(settings.exceptionFilers));
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
