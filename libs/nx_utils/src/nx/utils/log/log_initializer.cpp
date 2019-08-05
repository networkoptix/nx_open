#include "log_initializer.h"

#include <QtCore/QFile>

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
    std::set<Filter> filters,
    std::unique_ptr<AbstractWriter> writer)
{
    return LoggerBuilder::buildLogger(
        settings,
        applicationName,
        binaryPath,
        std::move(filters),
        std::move(writer));
}

void initializeGlobally(const nx::utils::ArgumentParser& arguments)
{
    log::Settings settings;
    settings.load(QnSettings(arguments), "log"/*, deafultCompatibilityLevel "none"*/);
    for (auto& logger: settings.loggers)
    {
        if (logger.logBaseName.isEmpty())
            logger.logBaseName = "-"; // < Default output is console.
    }

    if (auto logger = buildLogger(settings, QString()))
        setMainLogger(std::move(logger));

    lockConfiguration();
}

bool initializeFromConfigFile(
    const QString& configFileName,
    const QString& logsDirectory,
    const QString& applicationName,
    const QString& binaryPath,
    const QString& logFileNameSuffix)
{
    if (!QFile::exists(configFileName))
        return false;

    QSettings logConfig(configFileName, QSettings::IniFormat);
    Settings logSettings(&logConfig);
    logSettings.updateDirectoryIfEmpty(logsDirectory);

    if (!logFileNameSuffix.isEmpty())
    {
        for (auto& logger: logSettings.loggers)
        {
            if (logger.logBaseName.isEmpty())
                logger.logBaseName = "-"; // < Default output is console.
            else if (logger.logBaseName != '-')
                logger.logBaseName += logFileNameSuffix;
        }
    }

    if (auto logger = buildLogger(logSettings, applicationName, binaryPath))
        setMainLogger(std::move(logger));

    return true;
}

} // namespace log
} // namespace utils
} // namespace nx
