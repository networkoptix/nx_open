#pragma once

#include <map>

#include <QtCore/QSettings>

#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

//!Able to take settings from \a QSettings class (win32 registry or ini file) or from command line arguments
/*!
    Value defined as command line argument has preference over registry
*/
class QnSettings
{
public:
    QnSettings(const QString& applicationName_, const QString& moduleName_);

    void parseArgs(int argc, const char* argv[]);
    QVariant value(
        const QString& key,
        const QVariant& defaultValue = QVariant()) const;

    const QString applicationName;
    const QString moduleName;

private:
    QSettings m_systemSettings;
    nx::utils::ArgumentParser m_args;
};

class QnLogSettings
{
public:
    void load(const QnSettings& settings, QString prefix = QLatin1String("log"));

    void initLog(
        const QString& dataDir,
        const QString& applicationName,
        const QString& baseName = QLatin1String("log_file"),
        int id = QnLog::MAIN_LOG_ID) const;

private:
    QnLogLevel m_level = cl_logUNKNOWN;
    QString m_dir;
    quint32 m_maxFileSize = 1;
    quint8 m_maxBackupCount = 1;
};
