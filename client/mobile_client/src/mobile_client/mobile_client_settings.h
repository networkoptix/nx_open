#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>
#include <nx/utils/log/log_level.h>
#include <mobile_client/mobile_client_meta_types.h>
#include <mobile_client/mobile_client_startup_parameters.h>
#include <client_core/local_connection_data.h>
#include <settings/last_connection.h>

using nx::client::mobile::settings::LastConnectionData;

class QnMobileClientSettings : public QnPropertyStorage, public Singleton<QnMobileClientSettings>
{
    Q_OBJECT
    using base_type = QnPropertyStorage;

public:
    enum Variable
    {
        SettingsVersion,

        LastUsedConnection,
        AutoLogin,
        LastUsedQuality,
        LiteMode,
        MaxFfmpegResolution,
        ShowCameraInfo,
        LiveVideoPreviews,

        BasePath,
        TestMode,
        InitialTest,
        WebSocketPort,

        StartupParameters,

        LogLevel,

        // Depracated properties
        SavedSessions,
        IsSettingsMigrated,
        LastUsedSessionId,
        SavePasswords,

        VariableCount
    };

    explicit QnMobileClientSettings(QObject* parent = nullptr);

    void load();
    void save();

    bool isWritable() const;

    bool isLiteClientModeEnabled() const;
    bool isAutoLoginEnabled() const;

protected:
    virtual void updateValuesFromSettings(
            QSettings* settings, const QList<int>& ids) override;
    virtual QVariant readValueFromSettings(
            QSettings* settings, int id, const QVariant& defaultValue) override;
    virtual void writeValueToSettings(
            QSettings* settings, int id, const QVariant& value) const override;
    virtual UpdateStatus updateValue(int id, const QVariant& value) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VariableCount)
        QN_DECLARE_RW_PROPERTY(int,                         settingsVersion,            setSettingsVersion,         SettingsVersion,            0)

        QN_DECLARE_RW_PROPERTY(
            LastConnectionData,
            lastUsedConnection, setLastUsedConnection,
            LastUsedConnection, LastConnectionData())

        QN_DECLARE_RW_PROPERTY(int,                         autoLoginMode,              setAutoLoginMode,           AutoLogin,                  (int) AutoLoginMode::Auto)
        QN_DECLARE_RW_PROPERTY(int,                         lastUsedQuality,            setLastUsedQuality,         LastUsedQuality,            0)
        QN_DECLARE_RW_PROPERTY(int,                         liteMode,                   setLiteMode,                LiteMode,                   (int)LiteModeType::LiteModeAuto)
        QN_DECLARE_RW_PROPERTY(QSize,                       maxFfmpegResolution,        setMaxFfmpegResolution,     MaxFfmpegResolution,        QSize())
        QN_DECLARE_RW_PROPERTY(
            bool,
            showCameraInfo, setShowCameraInfo,
            ShowCameraInfo, false)
        QN_DECLARE_RW_PROPERTY(
            bool,
            liveVideoPreviews, setLiveVideoPreviews,
            LiveVideoPreviews, true)

        QN_DECLARE_RW_PROPERTY(QString,                     basePath,                   setBasePath,                BasePath,                   lit("qrc:///"))
        QN_DECLARE_RW_PROPERTY(bool,                        testMode,                   setTestMode,                TestMode,                   false)
        QN_DECLARE_RW_PROPERTY(QString,                     initialTest,                setInitialTest,             InitialTest,                QString())
        QN_DECLARE_RW_PROPERTY(quint16,                     webSocketPort,              setWebSocketPort,           WebSocketPort,              0)

        QN_DECLARE_RW_PROPERTY(
            QnMobileClientStartupParameters,
            startupParameters, setStartupParameters,
            StartupParameters, QnMobileClientStartupParameters())

        QN_DECLARE_RW_PROPERTY(QString,
            logLevel, setLogLevel,
            LogLevel, lit("none"))

        // Deprecated properties
        QN_DECLARE_RW_PROPERTY(QString,                     lastUsedSessionId,          setLastUsedSessionId,       LastUsedSessionId,          QString())
        QN_DECLARE_RW_PROPERTY(bool,                        isSettingsMigrated,         setSettingsMigrated,        IsSettingsMigrated,         false)
        // \see QnMigratedSystemsFinder
        QN_DECLARE_RW_PROPERTY(QVariantList,                savedSessions,              setSavedSessions,           SavedSessions,              QVariantList())
        QN_DECLARE_RW_PROPERTY(bool, savePasswords, setSavePasswords, SavePasswords, true)
    QN_END_PROPERTY_STORAGE()

private:
    QSettings* m_settings;
    bool m_loading;
};

#define qnSettings (QnMobileClientSettings::instance())
