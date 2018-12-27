#include "settings.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QFileInfo>

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/deprecated_settings.h>
#include <nx/utils/timer_manager.h>

#include <utils/common/app_info.h>
#include <media_server/media_server_app_info.h>
#include <media_server/media_server_module.h>
#include <media_server/serverutil.h>

namespace {

static QString configDirectory = lit("/opt/%1/mediaserver/etc").arg(QnAppInfo::linuxOrganizationName());
static QString templateConfigFileName = QString("%1/mediaserver.conf.template").arg(configDirectory);

} // namespace

QString MSSettings::defaultConfigFileName(QString("%1/mediaserver.conf").arg(configDirectory));
QString MSSettings::defaultConfigFileNameRunTime(QString("%1/running_time.conf").arg(configDirectory));

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

    m_settings.attach(m_roSettings);
    loadAnalyticEventsStorageSettings();
}

void MSSettings::setServerModule(QnMediaServerModule* serverModule)
{
    m_settings.setServerModule(serverModule);
}

void MSSettings::initializeROSettingsFromConfFile(const QString& fileName)
{
    m_roSettings.reset(new QSettings(fileName, QSettings::IniFormat));
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

nx::vms::server::Settings* MSSettings::mutableSettings()
{
    return &m_settings;
}

const nx::vms::server::Settings& MSSettings::settings() const
{
    return m_settings;
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

void MSSettings::syncRoSettings() const
{
    m_roSettings->sync();
}

nx::analytics::storage::Settings MSSettings::analyticEventsStorage() const
{
    return m_analyticEventsStorage;
}

void MSSettings::close()
{
    syncRoSettings();
    m_rwSettings->sync();
}

QString MSSettings::defaultConfigDirectory()
{
    #ifdef _WIN32
        return "windows registry is used";
    #else
        return configDirectory;
    #endif
}

void MSSettings::initializeRunTimeSettingsFromConfFile(const QString& fileName)
{
    m_rwSettings.reset(new QSettings(fileName, QSettings::IniFormat));
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

void MSSettings::loadAnalyticEventsStorageSettings()
{
    QnSettings settings(m_roSettings.get());

    m_roSettings->beginGroup("analyticEventsStorage");
    m_analyticEventsStorage.dbConnectionOptions.maxConnectionCount = 20; //< TODO: #ak
    m_analyticEventsStorage.load(settings);
    if (m_analyticEventsStorage.dbConnectionOptions.dbName.isEmpty())
    {
        m_analyticEventsStorage.dbConnectionOptions.dbName =
            nx::network::url::normalizePath(m_settings.dataDir() + "/object_detection.sqlite");
    }
    m_roSettings->endGroup();
}
