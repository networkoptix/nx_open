
#include "settings.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>

#include <utils/common/app_info.h>


#ifndef _WIN32
static QString defaultConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf").arg(QnAppInfo::linuxOrganizationName());
static QString defaultConfigFileNameRunTime = QString("/opt/%1/mediaserver/etc/running_time.conf").arg(QnAppInfo::linuxOrganizationName());
#endif

QString MSSettings::defaultROSettingsFilePath()
{
#ifdef _WIN32
    return "windows registry is used";
#else
    return defaultConfigFileName;
#endif
}

static std::unique_ptr<QSettings> roSettingsInstance;
static std::once_flag roSettings_onceFlag;

void MSSettings::initializeROSettingsFromConfFile( const QString& fileName )
{
    std::call_once(
        roSettings_onceFlag,
        [fileName](){ roSettingsInstance.reset( new QSettings( fileName, QSettings::IniFormat ) ); } );
}

QSettings* MSSettings::roSettings()
{
    std::call_once(
        roSettings_onceFlag,
        [](){
            roSettingsInstance.reset( new QSettings(
#ifndef _WIN32
                defaultConfigFileName, QSettings::IniFormat
#else
                QSettings::SystemScope, QnAppInfo::organizationName(), QCoreApplication::applicationName()
#endif
            ) );
        } );

    return roSettingsInstance.get();
}

QString MSSettings::defaultRunTimeSettingsFilePath()
{
#ifdef _WIN32
    return "windows registry is used";
#else
    return defaultConfigFileNameRunTime;
#endif
}

static std::unique_ptr<QSettings> rwSettingsInstance;
static std::once_flag rwSettings_onceFlag;

void MSSettings::initializeRunTimeSettingsFromConfFile( const QString& fileName )
{
    std::call_once(
        rwSettings_onceFlag,
        [fileName](){ rwSettingsInstance.reset( new QSettings( fileName, QSettings::IniFormat ) ); } );
}

QSettings* MSSettings::runTimeSettings()
{
    std::call_once(
        rwSettings_onceFlag,
        [](){
            rwSettingsInstance.reset( new QSettings(
#ifndef _WIN32
                defaultConfigFileNameRunTime, QSettings::IniFormat
#else
                QSettings::SystemScope, QnAppInfo::organizationName(), QCoreApplication::applicationName()
#endif
            ) );
        } );

    return rwSettingsInstance.get();
}
