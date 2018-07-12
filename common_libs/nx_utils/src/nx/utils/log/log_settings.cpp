#include "log_settings.h"

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/string.h>

namespace nx {
namespace utils {
namespace log {

void LoggerSettings::load(const QnSettings& settings, const QString& prefix)
{
    const auto makeKey =
        [&prefix](const char* key) { return QString(lm("%1/%2").arg(prefix).arg(key)); };

    QString logLevelStr = settings.value(makeKey("logLevel")).toString();
    if (logLevelStr.isEmpty())
        logLevelStr = settings.value(makeKey("log-level")).toString();
    if (logLevelStr.isEmpty())
        logLevelStr = settings.value(makeKey("ll")).toString();
    level.parse(logLevelStr);

    directory = settings.value(makeKey("logDir")).toString();
    maxBackupCount = (quint8) settings.value(makeKey("maxBackupCount"), 5).toInt();
    maxFileSize = (quint32) nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(), maxFileSize);

    logBaseName = settings.value(makeKey("baseName")).toString();
    if (logBaseName.isEmpty())
        logBaseName = settings.value(makeKey("log-file")).toString();
    if (logBaseName.isEmpty())
        logBaseName = settings.value(makeKey("lf")).toString();
}

void LoggerSettings::updateDirectoryIfEmpty(const QString& dataDirectory)
{
    if (directory.isEmpty())
        directory = dataDirectory + "/log";
}

//-------------------------------------------------------------------------------------------------

void Settings::load(const QnSettings& settings, const QString& prefix)
{
    LoggerSettings loggerSettings;
    loggerSettings.load(settings, prefix);
    loggers.push_back(loggerSettings);
}

void Settings::updateDirectoryIfEmpty(const QString& dataDirectory)
{
    for (auto& logger: loggers)
        logger.updateDirectoryIfEmpty(dataDirectory);
}

} // namespace log
} // namespace utils
} // namespace nx
