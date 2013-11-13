
#include "settings.h"

#include <atomic>
#include <mutex>

#include <QtCore/QSettings>

#include "version.h"


static std::mutex settingsMutex;

static void initializeSettingsFromConfFile( std::atomic<QSettings*>& settingsInstance, const QString& fileName )
{
    if( settingsInstance.load() )
        return;

    std::unique_lock<std::mutex> lk( settingsMutex );

    if( settingsInstance.load() )
        return;

    settingsInstance = new QSettings( fileName, QSettings::IniFormat );
}

static QSettings* getSettingsInstance(
    std::atomic<QSettings*>& settingsInstance
#ifndef _WIN32
    , const QString& defaultConfigFileName
#endif
    )
{
    if( settingsInstance.load() )
        return settingsInstance;

    std::unique_lock<std::mutex> lk( settingsMutex );

    if( settingsInstance.load() )
        return settingsInstance;

#ifndef _WIN32
    settingsInstance = new QSettings( defaultConfigFileName, QSettings::IniFormat );
#else
    settingsInstance = new QSettings( QSettings::SystemScope, QN_ORGANIZATION_NAME, QN_APPLICATION_NAME );
#endif

    return settingsInstance;
}



#ifndef _WIN32
static QString defaultConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf").arg(VER_LINUX_ORGANIZATION_NAME);
static QString defaultConfigFileNameRunTime = QString("/opt/%1/mediaserver/etc/running_time.conf").arg(VER_LINUX_ORGANIZATION_NAME);
#endif

static std::atomic<QSettings*> roSettingsInstance;
static std::atomic<QSettings*> rwSettingsInstance;

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
