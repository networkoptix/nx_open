// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logger_builder.h"

#include <nx/utils/string.h>

#include "aggregate_logger.h"
#include "log_main.h"

namespace nx::log {

namespace { static const QString kDefaultLogBaseName("log_file"); }

std::unique_ptr<AbstractLogger> LoggerBuilder::buildLogger(
    const Settings& settings,
    const QString& applicationName,
    const QString& binaryPath,
    const std::set<Filter>& filters,
    std::unique_ptr<AbstractWriter> writer)
{
    NX_ASSERT(!writer || settings.loggers.size() == 1,
        "Method semantics allows writer setup for the single logger only.");

    std::vector<std::unique_ptr<AbstractLogger>> loggers;
    for (const auto& loggerSettings: settings.loggers)
    {
        auto logger = buildLogger(
            loggerSettings,
            filters,
            std::exchange(writer, nullptr));

        if (!logger)
        {
            // Possible reasons: misconfiguration of command-line arguments, or a bug in the
            // calling code.
            NX_WARNING(NX_SCOPE_TAG, "Could not create logger %1", loggerSettings.logBaseName);
            continue;
        }

        logger->setSettings(loggerSettings);
        logger->setApplicationName(applicationName);
        logger->setBinaryPath(binaryPath);

        loggers.push_back(std::move(logger));
    }

    return std::make_unique<AggregateLogger>(std::move(loggers));
}

std::unique_ptr<Logger> LoggerBuilder::buildLogger(
    const LoggerSettings& settings,
    std::set<Filter> filters,
    std::unique_ptr<AbstractWriter> writer)
{
    if (settings.level.primary == Level::undefined
        || settings.level.primary == Level::notConfigured)
    {
        NX_WARNING(NX_SCOPE_TAG, "Attempt to create an unconfigured logger %1", settings.logBaseName);
        return nullptr;
    }

    // Validate RegExp filters.
    std::vector<QString> invalidFilters;
    auto levelFilters = settings.level.filters;
    for (auto iter = levelFilters.begin(); iter != levelFilters.end();)
    {
        if (!iter->first.isValid())
        {
            invalidFilters.push_back(iter->first.toString());
            iter = levelFilters.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    auto logger = std::make_unique<Logger>(std::move(filters));

    logger->setDefaultLevel(settings.level.primary);
    logger->setLevelFilters(levelFilters);

    if (!writer)
    {
        const QString baseName = settings.logBaseName.isEmpty()
            ? kDefaultLogBaseName
            : settings.logBaseName;
        if (baseName != QLatin1String("-"))
        {
            File::Settings fileSettings;
            fileSettings.maxVolumeSizeB = settings.maxVolumeSizeB;
            fileSettings.maxFileSizeB = settings.maxFileSizeB;
            fileSettings.maxFileTimePeriodS = settings.maxFileTimePeriodS;
            fileSettings.archivingEnabled = settings.archivingEnabled;
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

    for (const auto& expression: invalidFilters)
        writer->write(Level::error, nx::format("Regular expression %1 is invalid").arg(expression));

    logger->setWriter(std::move(writer));
    return logger;
}

} // namespace nx::log
