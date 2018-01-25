#include "settings.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QFileInfo>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/settings.h>
#include <nx/utils/timer_manager.h>

#include <utils/common/app_info.h>
#include <media_server/media_server_app_info.h>
#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>

#ifndef _WIN32
static QString configDirectory = lit("/opt/%1/mediaserver/etc").arg(QnAppInfo::linuxOrganizationName());
static QString templateConfigFileName = QString("%1/mediaserver.conf.template").arg(configDirectory);
static QString defaultConfigFileName = QString("%1/mediaserver.conf").arg(configDirectory);
static QString defaultConfigFileNameRunTime = QString("%1/running_time.conf").arg(configDirectory);
#endif

MSSettings::MSSettings(
    const QString& roSettingsPath,
    const QString& rwSettingsPath)
{
    if (!roSettingsPath.isEmpty())
        initializeROSettingsFromConfFile(roSettingsPath);
    else
        initializeROSettings();

    if (!rwSettingsPath.isEmpty())
        initializeRunTimeSettingsFromConfFile(rwSettingsPath);
    else
        initializeRunTimeSettings();

    loadSettings();
}

QString MSSettings::defaultROSettingsFilePath()
{
#ifdef _WIN32
    return "windows registry is used";
#else
    return defaultConfigFileName;
#endif
}

void MSSettings::initializeROSettingsFromConfFile( const QString& fileName )
{
    m_roSettings.reset( new QSettings( fileName, QSettings::IniFormat ) );
}

void MSSettings::initializeROSettings()
{
#ifndef _WIN32
    QFileInfo defaultFileInfo(defaultConfigFileName);
    if (!defaultFileInfo.exists())
    {
        QFileInfo templateInfo(templateConfigFileName);
        if (templateInfo.exists())
        {
            QFile file(templateConfigFileName);
            file.rename(defaultConfigFileName);
        }
    }
#endif
    m_roSettings.reset(new QSettings(
#ifndef _WIN32
        defaultConfigFileName, QSettings::IniFormat
#else
        QSettings::SystemScope,
        QnAppInfo::organizationName(),
        QnServerAppInfo::applicationName()
#endif
    ));
}

QSettings* MSSettings::roSettings()
{
    return m_roSettings.get();
}

const QSettings* MSSettings::roSettings() const
{
    return m_roSettings.get();
}

QSettings* MSSettings::runTimeSettings()
{
    return m_rwSettings.get();
}

const QSettings* MSSettings::runTimeSettings() const
{
    return m_rwSettings.get();
}

QString MSSettings::getDataDirectory() const
{
    return m_dataDirectory;
}

std::chrono::milliseconds MSSettings::hlsTargetDuration() const
{
    const auto value =
        std::chrono::milliseconds(m_roSettings->value(
            nx_ms_conf::HLS_TARGET_DURATION_MS,
            nx_ms_conf::DEFAULT_TARGET_DURATION_MS).toUInt());

    return value > std::chrono::milliseconds::zero()
        ? value
        : std::chrono::milliseconds(nx_ms_conf::DEFAULT_TARGET_DURATION_MS);
}

std::chrono::milliseconds MSSettings::delayBeforeSettingMasterFlag() const
{
    return nx::utils::parseTimerDuration(
        m_roSettings->value(
            nx_ms_conf::DELAY_BEFORE_SETTING_MASTER_FLAG,
            nx_ms_conf::DEFAULT_DELAY_BEFORE_SETTING_MASTER_FLAG).toString());
}

nx::analytics::storage::Settings MSSettings::analyticEventsStorage() const
{
    return m_analyticEventsStorage;
}

void MSSettings::close()
{
    m_rwSettings->sync();
    m_roSettings->sync();
}

void MSSettings::reopen(const QString& roFile, const QString& rwFile)
{
    if (!roFile.isEmpty())
        m_roSettings.reset(new QSettings(roFile, QSettings::IniFormat));

    if (!rwFile.isEmpty())
        m_rwSettings.reset(new QSettings(rwFile, QSettings::IniFormat));
}

QString MSSettings::defaultRunTimeSettingsFilePath()
{
#ifdef _WIN32
    return "windows registry is used";
#else
    return defaultConfigFileNameRunTime;
#endif
}

QString MSSettings::defaultConfigDirectory()
{
    #ifdef _WIN32
        return "windows registry is used";
    #else
        return configDirectory;
    #endif
}

void MSSettings::initializeRunTimeSettingsFromConfFile( const QString& fileName )
{
    m_rwSettings.reset( new QSettings( fileName, QSettings::IniFormat ) );
}

void MSSettings::initializeRunTimeSettings()
{
    m_rwSettings.reset(new QSettings(
#ifndef _WIN32
        defaultConfigFileNameRunTime, QSettings::IniFormat
#else
        QSettings::SystemScope, QnAppInfo::organizationName(), QCoreApplication::applicationName()
#endif
    ));
}

void MSSettings::loadSettings()
{
    loadGeneralSettings();
    loadAnalyticEventsStorageSettings();
}

void MSSettings::loadGeneralSettings()
{
    m_dataDirectory = loadDataDirectory();
}

QString MSSettings::loadDataDirectory()
{
    const QString& dataDirFromSettings = m_roSettings->value("dataDir").toString();
    if (!dataDirFromSettings.isEmpty())
        return dataDirFromSettings;

#ifdef Q_OS_LINUX
    QString defVarDirName = QString("/opt/%1/mediaserver/var").arg(QnAppInfo::linuxOrganizationName());
    QString varDirName = m_roSettings->value("varDir", defVarDirName).toString();
    return varDirName;
#else
    const QStringList& dataDirList = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    return dataDirList.isEmpty() ? QString() : dataDirList[0];
#endif
}

void MSSettings::loadAnalyticEventsStorageSettings()
{
    QnSettings settings(m_roSettings.get());

    m_roSettings->beginGroup("analyticEventsStorage");
    m_analyticEventsStorage.dbConnectionOptions.maxConnectionCount = 20; //< TODO: #ak
    m_analyticEventsStorage.load(settings);
    if (m_analyticEventsStorage.dbConnectionOptions.dbName.isEmpty())
    {
        m_analyticEventsStorage.dbConnectionOptions.dbName =
            nx::network::url::normalizePath(getDataDirectory() + "/object_detection.sqlite");
    }
    m_roSettings->endGroup();
}
