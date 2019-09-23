#include "log_settings.h"

#include <QtCore/QSettings>

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/string.h>
#include <nx/utils/std/filesystem.h>

#include "log_message.h"
#include "assert.h"

namespace nx {
namespace utils {
namespace log {

void LoggerSettings::parse(const QString& str)
{
    const auto params = parseNameValuePairs<std::multimap>(
        str.toUtf8(),
        ',',
        GroupToken::doubleQuotes | GroupToken::squareBraces);
    for (const auto& param: params)
    {
        if (param.first == "file")
        {
            const auto filePath = std::filesystem::old_version_support::path(param.second.toStdString());

            logBaseName = QString::fromStdString(filePath.filename().string());
            if (filePath.has_parent_path())
                directory = QString::fromStdString(filePath.parent_path().string());
        }
        else if (param.first == "level")
        {
            level.parse(param.second);
        }
        else if (param.first == "dir")
        {
            directory = param.second;
        }
        else if (param.first == "maxBackupCount")
        {
            maxBackupCount = param.second.toInt();
        }
        else if (param.first == "maxFileSize")
        {
            maxFileSize = stringToBytes(param.second);
        }
    }
}

void LoggerSettings::updateDirectoryIfEmpty(const QString& logDirectory)
{
    if (directory.isEmpty())
        directory = logDirectory;
}

bool LoggerSettings::operator==(const LoggerSettings& right) const
{
    return level == right.level
        && directory == right.directory
        && maxFileSize == right.maxFileSize
        && maxBackupCount == right.maxBackupCount
        && logBaseName == right.logBaseName;
}

//-------------------------------------------------------------------------------------------------

Settings::Settings(QSettings* settings)
{
    NX_ASSERT(settings);
    if (!settings)
        return;

    const auto maxBackupCount = settings->value("logArchiveSize", 10).toUInt();
    const auto maxFileSize = settings->value("maxLogFileSize", 10 * 1024 * 1024).toUInt();

    for (const auto& group: settings->childGroups())
    {
        LoggerSettings logger;
        logger.logBaseName = group;
        logger.maxBackupCount = maxBackupCount;
        logger.maxFileSize = maxFileSize;
        logger.level.primary = Level::none;

        settings->beginGroup(group);
        for (const QString& levelKey: settings->childKeys())
        {
            const Level level = levelFromString(levelKey);
            const QVariant& value = settings->value(levelKey);

            switch (value.type())
            {
                case QVariant::String:
                {
                    const QString& valueString = value.toString().trimmed();
                    if (valueString == L'*')
                        logger.level.primary = level;
                    else
                        logger.level.filters[Filter(valueString)] = level;
                    break;
                }
                case QVariant::StringList:
                {
                    for (const QString& valueString: value.toStringList())
                        logger.level.filters[Filter(valueString.trimmed())] = level;
                    break;
                }
                default:
                    break;
            }
        }
        settings->endGroup();

        loggers.push_back(std::move(logger));
    }
}

void Settings::load(const QnSettings& settings, const QString& prefix)
{
    int logSettingCount = 0;

    // Parsing every prefix/logger argument.
    const auto allArgs = settings.allArgs();
    for (const auto& arg: allArgs)
    {
        if (!arg.first.startsWith(prefix))
            continue;
        ++logSettingCount;

        if (arg.first == lm("%1/logger").args(prefix).toQString())
        {
            LoggerSettings loggerSettings;
            loggerSettings.parse(arg.second);
            loggers.push_back(std::move(loggerSettings));
        }
    }

    // If there are more prefix/* arguments, adding one more logger.
    if ((int) loggers.size() < logSettingCount)
        loadCompatibilityLogger(settings, prefix);
}

void Settings::updateDirectoryIfEmpty(const QString& logDirectory)
{
    for (auto& logger: loggers)
        logger.updateDirectoryIfEmpty(logDirectory);
}

void Settings::loadCompatibilityLogger(
    const QnSettings& settings,
    const QString& prefix)
{
    LoggerSettings loggerSettings;

    const auto makeKey =
        [&prefix](const char* key) { return QString(lm("%1/%2").arg(prefix).arg(key)); };

    QString logLevelStr = settings.value(makeKey("logLevel")).toString();
    if (logLevelStr.isEmpty())
        logLevelStr = settings.value("log-level").toString();
    if (logLevelStr.isEmpty())
        logLevelStr = settings.value("ll").toString();
    loggerSettings.level.parse(logLevelStr);

    loggerSettings.directory = settings.value(makeKey("logDir")).toString();
    loggerSettings.maxBackupCount = (quint8)settings.value(makeKey("maxBackupCount"), 5).toInt();
    loggerSettings.maxFileSize = nx::utils::stringToBytes(
        settings.value(makeKey("maxFileSize")).toString(),
        loggerSettings.maxFileSize);

    loggerSettings.logBaseName = settings.value(makeKey("baseName")).toString();
    if (loggerSettings.logBaseName.isEmpty())
        loggerSettings.logBaseName = settings.value("log-file").toString();
    if (loggerSettings.logBaseName.isEmpty())
        loggerSettings.logBaseName = settings.value("lf").toString();

    loggers.push_back(std::move(loggerSettings));
}

} // namespace log
} // namespace utils
} // namespace nx
