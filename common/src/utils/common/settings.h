#pragma once

#include <map>

#include <QtCore/QSettings>

#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

/**
 * Able to take settings from QSettings class (win32 registry or linux ini file) or from
 *  command line arguments.
 *
 * Value defined as command line argument has preference over registry.
 *
 * example.reg
 *      [pathToModule/section]
 *      "item" = "value"
 *
 * example.ini
 *      [section]
 *      item = value
 *
 * arguments: --section/item=value
 */
class QnSettings
{
public:
    QnSettings(
        const QString& applicationName,
        const QString& moduleName,
        QSettings::Scope scope = QSettings::SystemScope);

    void parseArgs(int argc, const char* argv[]);
    bool contains(const QString& key) const;
    QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const;

    const QString getApplicationName() const { return m_applicationName; }
    const QString getModuleName() const { return m_moduleName; }

private:
    const QString m_applicationName;
    const QString m_moduleName;

    QSettings m_systemSettings;
    nx::utils::ArgumentParser m_args;
};

class QnLogSettings
{
public:
    #ifdef _DEBUG
        static constexpr QnLogLevel kDefaultLogLevel = cl_logDEBUG1;
    #else
        static constexpr QnLogLevel kDefaultLogLevel = cl_logINFO;
    #endif

    QnLogLevel level = kDefaultLogLevel;
    QString directory = QString(); //< dataDir/log
    quint32 maxFileSize = nx::utils::stringToBytesConst("10M");
    quint8 maxBackupCount = 5;

    /** Rewrites values from settings if specified */
    void load(const QnSettings& settings, const QString& prefix = QLatin1String("log"));
};

void initializeQnLog(
    const QnLogSettings& settings,
    const QString& dataDir,
    const QString& applicationName,
    const QString& baseName = QLatin1String("log_file"),
    int id = QnLog::MAIN_LOG_ID);
