
#include "settings.h"

#include <atomic>
#include <iostream>
#include <mutex>

#include <QtCore/QSettings>

#include <utils/common/app_info.h>


static std::mutex settingsMutex;

static void initializeSettingsFromConfFile( QSettings*& settingsInstance, const QString& fileName )
{
    if( settingsInstance )
        return;

    std::unique_lock<std::mutex> lk( settingsMutex );

    if( settingsInstance )
        return;

    settingsInstance = new QSettings( fileName, QSettings::IniFormat );
}

static QSettings* getSettingsInstance(
    QSettings*& settingsInstance
#ifndef _WIN32
    , const QString& defaultConfigFileName
#endif
    )
{
    if( settingsInstance )
        return settingsInstance;

    std::unique_lock<std::mutex> lk( settingsMutex );

    if( settingsInstance )
        return settingsInstance;

#ifndef _WIN32
    settingsInstance = new QSettings( defaultConfigFileName, QSettings::IniFormat );
#else
    settingsInstance = new QSettings( QSettings::SystemScope, QnAppInfo::organizationName(), QnAppInfo::applicationName() );
#endif

    return settingsInstance;
}



#ifndef _WIN32
static QString defaultConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf").arg(QnAppInfo::linuxOrganizationName());
static QString defaultConfigFileNameRunTime = QString("/opt/%1/mediaserver/etc/running_time.conf").arg(QnAppInfo::linuxOrganizationName());
#endif

static QSettings* roSettingsInstance = NULL;
static QSettings* rwSettingsInstance = NULL;

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
    initializeSettingsFromConfFile( roSettingsInstance, fileName );
}

QSettings* MSSettings::roSettings()
{
    return getSettingsInstance(
        roSettingsInstance
#ifndef _WIN32
        , defaultConfigFileName
#endif
        );
}

QString MSSettings::defaultRunTimeSettingsFilePath()
{
#ifdef _WIN32
    return "windows registry is used";
#else
    return defaultConfigFileNameRunTime;
#endif
}

void MSSettings::initializeRunTimeSettingsFromConfFile( const QString& fileName )
{
    initializeSettingsFromConfFile( rwSettingsInstance, fileName );
}

QSettings* MSSettings::runTimeSettings()
{
    return getSettingsInstance(
        rwSettingsInstance
#ifndef _WIN32
        , defaultConfigFileNameRunTime
#endif
        );
}
