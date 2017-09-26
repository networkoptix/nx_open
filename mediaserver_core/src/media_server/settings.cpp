
#include "settings.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>

#include <QtCore/QCoreApplication>
#include <QtCore/QSettings>
#include <QtCore/QFileInfo>

#include <nx/utils/timer_manager.h>

#include <utils/common/app_info.h>
#include <media_server/media_server_app_info.h>
#include <media_server/media_server_module.h>

#ifndef _WIN32
static QString templateConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf.template").arg(QnAppInfo::linuxOrganizationName());
static QString defaultConfigFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf").arg(QnAppInfo::linuxOrganizationName());
static QString defaultConfigFileNameRunTime = QString("/opt/%1/mediaserver/etc/running_time.conf").arg(QnAppInfo::linuxOrganizationName());
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
