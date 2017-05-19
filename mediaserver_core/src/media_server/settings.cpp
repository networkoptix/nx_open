
#include "settings.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QFileInfo>

#include <utils/common/app_info.h>
#include <media_server/media_server_app_info.h>

#ifndef _WIN32
static QString templateConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf.template").arg(QnAppInfo::linuxOrganizationName());
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

void MSSettings::initializeROSettingsFromConfFile( const QString& fileName )
{
    roSettingsInstance.reset( new QSettings( fileName, QSettings::IniFormat ) );
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
    roSettingsInstance.reset(new QSettings(
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
    return roSettingsInstance.get();
}

std::chrono::milliseconds MSSettings::hlsTargetDuration()
{
    const auto value =
        std::chrono::milliseconds(roSettings()->value(
            nx_ms_conf::HLS_TARGET_DURATION_MS,
            nx_ms_conf::DEFAULT_TARGET_DURATION_MS).toUInt());

    return value > std::chrono::milliseconds::zero()
        ? value
        : std::chrono::milliseconds(nx_ms_conf::DEFAULT_TARGET_DURATION_MS);
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

void MSSettings::initializeRunTimeSettingsFromConfFile( const QString& fileName )
{
    rwSettingsInstance.reset( new QSettings( fileName, QSettings::IniFormat ) );
}

void MSSettings::initializeRunTimeSettings()
{
    rwSettingsInstance.reset(new QSettings(
#ifndef _WIN32
        defaultConfigFileNameRunTime, QSettings::IniFormat
#else
        QSettings::SystemScope, QnAppInfo::organizationName(), QCoreApplication::applicationName()
#endif
    ));
}

QSettings* MSSettings::runTimeSettings()
{
    return rwSettingsInstance.get();
}
