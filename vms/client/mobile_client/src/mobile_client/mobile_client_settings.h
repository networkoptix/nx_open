// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>

#include <mobile_client/mobile_client_startup_parameters.h>
#include <nx/vms/client/mobile/application_context.h>
#include <nx/vms/client/mobile/maintenance/remote_log_session_data.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_structures.h>
#include <utils/common/property_storage.h>

class QnMobileClientSettings : public QnPropertyStorage
{
    Q_OBJECT
    using base_type = QnPropertyStorage;

public:
    enum Variable
    {
        SettingsVersion,

        LastUsedQuality,
        MaxFfmpegResolution,
        ShowCameraInfo,
        LiveVideoPreviews,

        StartupParameters,

        LogLevel,

        SavePasswords,

        AudioSettings,
        ServerTimeMode,

        // Push notifications properties
        UserPushSettings,

        RemoteLogSession,

        HolePunchingOption,

        ForceTrafficLogging,

        CustomCloudHost,

        IgnoreCustomization,

        SupportMetaOrganizations,

        // Beta features and settings flags.
        UseDownloadVideoFeature,

        UseMaxHardwareDecodersCount,

        EnableSoftwareDecoderFallback,
        ShowHowShareWorksNotification,

        VariableCount
    };

    explicit QnMobileClientSettings(QObject* parent = nullptr);

    void load();
    void save();

    bool isWritable() const;

protected:
    virtual void updateValuesFromSettings(
            QSettings* settings, const QList<int>& ids) override;
    virtual QVariant readValueFromSettings(
            QSettings* settings, int id, const QVariant& defaultValue) const override;
    virtual void writeValueToSettings(
            QSettings* settings, int id, const QVariant& value) const override;
    virtual UpdateStatus updateValue(int id, const QVariant& value) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VariableCount)
        QN_DECLARE_RW_PROPERTY(int,                         settingsVersion,            setSettingsVersion,         SettingsVersion,            0)

        QN_DECLARE_RW_PROPERTY(int,                         lastUsedQuality,            setLastUsedQuality,         LastUsedQuality,            0)
        QN_DECLARE_RW_PROPERTY(QSize,                       maxFfmpegResolution,        setMaxFfmpegResolution,     MaxFfmpegResolution,        QSize())
        QN_DECLARE_RW_PROPERTY(
            bool,
            showCameraInfo, setShowCameraInfo,
            ShowCameraInfo, false)
        QN_DECLARE_RW_PROPERTY(bool,
            showHowShareWorksNotification,
            setShowHowShareWorksNotification,
            ShowHowShareWorksNotification,
            true)
        QN_DECLARE_RW_PROPERTY(
            bool,
            liveVideoPreviews, setLiveVideoPreviews,
            LiveVideoPreviews, true)

        QN_DECLARE_RW_PROPERTY(
            QnMobileClientStartupParameters,
            startupParameters, setStartupParameters,
            StartupParameters, QnMobileClientStartupParameters())

        QN_DECLARE_RW_PROPERTY(QString,
            logLevel, setLogLevel,
            LogLevel, "none")

        QN_DECLARE_RW_PROPERTY(bool, savePasswords, setSavePasswords, SavePasswords, true)
        QN_DECLARE_RW_PROPERTY(QByteArray, audioSettings, setAudioSettings, AudioSettings, QByteArray())

        QN_DECLARE_RW_PROPERTY(bool, serverTimeMode, setServerTimeMode, ServerTimeMode, true)

        QN_DECLARE_RW_PROPERTY(
            nx::vms::client::mobile::details::UserPushSettings,
            userPushSettings, setUserPushSettings,
            UserPushSettings, nx::vms::client::mobile::details::UserPushSettings())

        QN_DECLARE_RW_PROPERTY(
            nx::vms::client::mobile::RemoteLogSessionData,
            remoteLogSessionData, setRemoteLogSessionData,
            RemoteLogSession, {})

        QN_DECLARE_RW_PROPERTY(
            bool,
            enableHolePunching, setEnableHolePunching,
            HolePunchingOption, false)

        QN_DECLARE_RW_PROPERTY(
            bool,
            forceTrafficLogging, setForceTrafficLogging,
            ForceTrafficLogging, false)

        QN_DECLARE_RW_PROPERTY(
            QString,
            customCloudHost, setCustomCloudHost,
            CustomCloudHost, {})

        QN_DECLARE_RW_PROPERTY(
            bool,
            ignoreCustomization, setIgnoreCustomization,
            IgnoreCustomization, false)

        QN_DECLARE_RW_PROPERTY(
            bool,
            supportMetaOrganizations, setSupportMetaOrganizations,
            SupportMetaOrganizations, false)

        QN_DECLARE_RW_PROPERTY(
            bool,
            useDownloadVideoFeature, setUseDownloadVideoFeature,
            UseDownloadVideoFeature, false)

        QN_DECLARE_RW_PROPERTY(
            bool,
            useMaxHardwareDecodersCount, setUseMaxHardwareDecodersCount,
            UseMaxHardwareDecodersCount, true)

        QN_DECLARE_RW_PROPERTY(
            bool,
            enableSoftwareDecoderFallback, setEnableSoftwareDecoderFallback,
            EnableSoftwareDecoderFallback, true)

    QN_END_PROPERTY_STORAGE()

private:
    QSettings* m_settings;
    bool m_loading;
};

#define qnSettings (nx::vms::client::mobile::appContext()->clientSettings())
