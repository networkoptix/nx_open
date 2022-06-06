// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtGui/QColor>

#include <client/client_globals.h>
#include <client/client_model_types.h>
#include <client_core/local_connection_data.h>
#include <common/common_meta_types.h>
#include <core/resource/resource_display_info.h>
#include <health/system_health.h>
#include <nx/branding.h>
#include <nx/utils/qset.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/client/core/network/server_certificate_validation_level.h>
#include <nx/vms/client/core/system_logon/connection_data.h>
#include <nx/vms/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/vms/client/desktop/jsapi/auth_allowed_urls.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <utils/common/property_storage.h>

struct QnStartupParameters;
class QSettings;

class QnClientSettings: public QnPropertyStorage, public Singleton<QnClientSettings>
{
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable
    {
        /** Maximum number of items that is allowed on the scene. */
        MAX_SCENE_VIDEO_ITEMS,

        /** Maximum number of items that is allowed during the preview search. */
        MAX_PREVIEW_SEARCH_ITEMS,

        DOWNMIX_AUDIO,
        AUDIO_VOLUME,
        MEDIA_FOLDERS,
        WORKBENCH_STATES,
        LICENSE_WARNING_STATES,

        LAST_DATABASE_BACKUP_DIR,
        LAST_SCREENSHOT_DIR,
        LAST_RECORDING_DIR,
        LAST_EXPORT_DIR,
        LAST_DOWNLOAD_DIR,

        /** Last system we successfully connected to. */
        LAST_USED_CONNECTION,

        LOCALE,

        /** Latest known update info. */
        UPDATE_DELIVERY_INFO,

        /** Estimated update delivery date (in msecs since epoch). */
        UPDATE_DELIVERY_DATE,

        /** Do not show update notification for the selected version. */
        IGNORED_UPDATE_VERSION,

        TOUR_CYCLE_TIME,

        RESOURCE_INFO_LEVEL,

        TIME_MODE,

        CREATE_FULL_CRASH_DUMP,

        WORKBENCH_PANES,

        POPUP_SYSTEM_HEALTH,

        AUTO_START,

        /** Auto-login to the last connected server. */
        AUTO_LOGIN,

        /** Filter value for network connections in the statistics widget. */
        STATISTICS_NETWORK_FILTER,

        /** Last used value for the 'Keep aspect ratio' flag in the layout settings. */
        LAYOUT_KEEP_ASPECT_RATIO,

        /** Last used path for the layout backgrounds */
        BACKGROUNDS_FOLDER,

        /**
         * When "Info" mode is enabled on cameras, full info will be displayed even without hover.
         */
        SHOW_FULL_INFO,

        /**
         * "Info" mode will be enabled by default on newly opened cameras.
         */
        SHOW_INFO_BY_DEFAULT,

        /** Allow double buffering for openGL context */
        GL_DOUBLE_BUFFER,

        /** Allow blur on video items. */
        GL_BLUR,

        TIMESTAMP_CORNER,

        USER_IDLE_TIMEOUT_MSECS,

        /** Unique id for this PC for videowall construction. */
        PC_UUID,

        /** Full set of background options. */
        BACKGROUND_IMAGE,

        LOG_LEVEL,

        /** Initial and maximal live buffer lengths, in milliseconds. */
        INITIAL_LIVE_BUFFER_MSECS,
        MAXIMUM_LIVE_BUFFER_MSECS,

        LAST_LOCAL_CONNECTION_URL,
        KNOWN_SERVER_URLS,

        // Force client to reconnect only to the same server it was connected to.
        STICKY_RECONNECT,

        EXPORT_MEDIA_SETTINGS,
        EXPORT_LAYOUT_SETTINGS,
        EXPORT_BOOKMARK_SETTINGS,
        LAST_EXPORT_MODE,

        DETECTED_OBJECT_DISPLAY_SETTINGS,

        /** Version of the latest read and accepted EULA. */
        ACCEPTED_EULA_VERSION,
        ALLOW_MT_DECODING,
        FORCE_MT_DECODING,

        ALL_LAYOUTS_SELECTION_DIALOG_MODE, //< Tree mode in MultipleLayoutSelectionDialog.

        // Upload state for ServerUpdateTool.
        SYSTEM_UPDATER_STATE,

        SAVE_CREDENTIALS_ALLOWED,

        /** Enables restoration of the session-specific data. */
        RESTORE_USER_SESSION,

        // Automatically limit FPS.
        AUTO_FPS_LIMIT,

        SHOW_FISHEYE_CALIBRATION_GRID,

        // Play audio for all items at the same time if true.
        PLAY_AUDIO_FOR_ALL_ITEMS,

        /** Whether hardware video decoding is used, if available. */
        HARDWARE_DECODING_ENABLED,

        /** Whether old-style PTZ aim overlay is enabled. */
        PTZ_AIM_OVERLAY_ENABLED,

        /** [Support] Whether "Browse Logs" button is visible on Advanced page of Local Settings. */
        BROWSE_LOGS_VISIBLE,

        /** Maximum count of hardware decoders. */
        MAX_HARDWARE_DECODERS,

        /** Server certificate validation level. */
        SERVER_CERTIFICATE_VALIDATION_LEVEL,

        /** Mute audio output while two way audio is engaged. */
        MUTE_ON_AUDIO_TRANSMIT,

        /** Approved urls that have access to a session token using jsapi. */
        AUTH_ALLOWED_URLS,

        VARIABLE_COUNT
    };
    Q_ENUM(Variable)

    QnClientSettings(const QnStartupParameters& startupParameters, QObject* parent = nullptr);
    virtual ~QnClientSettings();

    void load();
    bool save();

    /**
     * @brief isWritable    Check if settings storage is available for writing.
     * @returns             True if settings can be saved.
     */
    bool isWritable() const;
    QSettings* rawSettings();

    /**
     * Execute settings migration.
     */
    void migrate();

    /** Get an IniConfig parameter value by its name. To be used from QML. */
    Q_INVOKABLE static QVariant iniConfigValue(const QString& paramName);

signals:
    void saved();

protected:
    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids) override;

    virtual QVariant readValueFromSettings(QSettings *settings, int id,
        const QVariant& defaultValue) const override;
    virtual void writeValueToSettings(QSettings *settings, int id,
        const QVariant& value) const override;

    virtual UpdateStatus updateValue(int id, const QVariant &value) override;

private:
    using ExportMediaSettings = nx::vms::client::desktop::ExportMediaPersistentSettings;
    using ExportLayoutSettings = nx::vms::client::desktop::ExportLayoutPersistentSettings;
    using AuthAllowedUrls = nx::vms::client::desktop::AuthAllowedUrls;

    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(int,                         maxSceneVideoItems,     setMaxSceneVideoItems,      MAX_SCENE_VIDEO_ITEMS,      24)
        QN_DECLARE_RW_PROPERTY(int,                         maxPreviewSearchItems,  setMaxPreviewSearchItems,   MAX_PREVIEW_SEARCH_ITEMS,   16)
        QN_DECLARE_RW_PROPERTY(bool,                        isAudioDownmixed,       setAudioDownmixed,          DOWNMIX_AUDIO,              false)
        QN_DECLARE_RW_PROPERTY(qreal,                       audioVolume,            setAudioVolume,             AUDIO_VOLUME,               1.0)
        QN_DECLARE_RW_PROPERTY(QStringList,                 mediaFolders,           setMediaFolders,            MEDIA_FOLDERS,              QStringList())
        QN_DECLARE_RW_PROPERTY(QString,                     lastDatabaseBackupDir,  setLastDatabaseBackupDir,   LAST_DATABASE_BACKUP_DIR,   QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastScreenshotDir,      setLastScreenshotDir,       LAST_SCREENSHOT_DIR,        QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastRecordingDir,       setLastRecordingDir,        LAST_RECORDING_DIR,         QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastExportDir,          setLastExportDir,           LAST_EXPORT_DIR,            QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastDownloadDir,        setLastDownloadDir,         LAST_DOWNLOAD_DIR,          QString())
        QN_DECLARE_RW_PROPERTY(QnWorkbenchStateList,        workbenchStates,        setWorkbenchStates,         WORKBENCH_STATES,           QnWorkbenchStateList())
        QN_DECLARE_RW_PROPERTY(QnLicenseWarningStateHash,   licenseWarningStates,   setLicenseWarningStates,    LICENSE_WARNING_STATES,     QnLicenseWarningStateHash())

        QN_DECLARE_RW_PROPERTY(nx::vms::client::core::ConnectionData,
            lastUsedConnection,
            setLastUsedConnection,
            LAST_USED_CONNECTION,
            {})
        QN_DECLARE_RW_PROPERTY(QString,
            lastLocalConnectionUrl,
            setLastLocalConnectionUrl,
            LAST_LOCAL_CONNECTION_URL,
            QString())

        QN_DECLARE_RW_PROPERTY(QString, locale, setLocale, LOCALE, nx::branding::defaultLocale())

        /* Updates-related settings */
        QN_DECLARE_RW_PROPERTY(nx::vms::api::SoftwareVersion, ignoredUpdateVersion, setIgnoredUpdateVersion,    IGNORED_UPDATE_VERSION,     {})
        QN_DECLARE_RW_PROPERTY(nx::vms::client::desktop::UpdateDeliveryInfo,
            updateDeliveryInfo,
            setUpdateDeliveryInfo,
            UPDATE_DELIVERY_INFO,
            nx::vms::client::desktop::UpdateDeliveryInfo())
        QN_DECLARE_RW_PROPERTY(qint64,                      updateDeliveryDate,     setUpdateDeliveryDate,      UPDATE_DELIVERY_DATE,       0)

        QN_DECLARE_RW_PROPERTY(int,                         tourCycleTime,          setTourCycleTime,           TOUR_CYCLE_TIME,            4000)

        // Level of resources detalization in the resources tree, notifications, etc. Includes
        // displaying cameras' ip addresses, user roles, videowall layouts.
        QN_DECLARE_RW_PROPERTY(Qn::ResourceInfoLevel, resourceInfoLevel, setResourceInfoLevel,
            RESOURCE_INFO_LEVEL, Qn::RI_NameOnly)

        QN_DECLARE_RW_PROPERTY(Qn::TimeMode,                timeMode,               setTimeMode,                TIME_MODE,                  Qn::ServerTimeMode)
        QN_DECLARE_R_PROPERTY (bool,                        createFullCrashDump,                                CREATE_FULL_CRASH_DUMP,     false)
        // This was renamed in 4.0 to prevent reuse of 3.x settings (WORKBENCH_PANES : paneSettings -> paneStateSettings).
        QN_DECLARE_RW_PROPERTY(QnPaneSettingsMap,           paneStateSettings,      setPaneStateSettings,       WORKBENCH_PANES,            Qn::defaultPaneSettings())
        QN_DECLARE_RW_PROPERTY(QSet<QnSystemHealth::MessageType>, popupSystemHealth, setPopupSystemHealth,      POPUP_SYSTEM_HEALTH,        nx::utils::toQSet(QnSystemHealth::allVisibleMessageTypes()))
        QN_DECLARE_RW_PROPERTY(bool,                        autoStart,              setAutoStart,               AUTO_START,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        autoLogin,              setAutoLogin,               AUTO_LOGIN,                 false)
        QN_DECLARE_R_PROPERTY (int,                         statisticsNetworkFilter,                            STATISTICS_NETWORK_FILTER,  1)
        QN_DECLARE_RW_PROPERTY(bool,                        layoutKeepAspectRatio,  setLayoutKeepAspectRatio,   LAYOUT_KEEP_ASPECT_RATIO,   true)
        QN_DECLARE_RW_PROPERTY(QString,                     backgroundsFolder,      setBackgroundsFolder,       BACKGROUNDS_FOLDER,         QString())
        QN_DECLARE_RW_PROPERTY(bool,                        showFullInfo,           setShowFullInfo,            SHOW_FULL_INFO,             false);
        QN_DECLARE_RW_PROPERTY(bool,                        showInfoByDefault,      setShowInfoByDefault,       SHOW_INFO_BY_DEFAULT,       false);
        QN_DECLARE_RW_PROPERTY(bool,                        isGlDoubleBuffer,       setGLDoubleBuffer,          GL_DOUBLE_BUFFER,           true)
        QN_DECLARE_RW_PROPERTY(bool,                        isGlBlurEnabled,        setGlBlurEnabled,           GL_BLUR,                    true)
        QN_DECLARE_RW_PROPERTY(quint64,                     userIdleTimeoutMSecs,   setUserIdleTimeoutMSecs,    USER_IDLE_TIMEOUT_MSECS,    0)
        QN_DECLARE_RW_PROPERTY(ExportMediaSettings,         exportMediaSettings,    setExportMediaSettings,     EXPORT_MEDIA_SETTINGS,      ExportMediaSettings())
        QN_DECLARE_RW_PROPERTY(ExportLayoutSettings,        exportLayoutSettings,   setExportLayoutSettings,    EXPORT_LAYOUT_SETTINGS,     ExportLayoutSettings())
        QN_DECLARE_RW_PROPERTY(ExportMediaSettings,         exportBookmarkSettings, setExportBookmarkSettings,  EXPORT_BOOKMARK_SETTINGS,   ExportMediaSettings({nx::vms::client::desktop::ExportOverlayType::bookmark}))
        QN_DECLARE_RW_PROPERTY(QString,                     lastExportMode,         setLastExportMode,          LAST_EXPORT_MODE,           "media")
        QN_DECLARE_RW_PROPERTY(QnBackgroundImage,           backgroundImage,        setBackgroundImage,         BACKGROUND_IMAGE,           QnBackgroundImage::defaultBackground())
        QN_DECLARE_RW_PROPERTY(QnUuid,                      pcUuid,                 setPcUuid,                  PC_UUID,                    QnUuid())
        QN_DECLARE_R_PROPERTY(bool,                         stickReconnectToServer,                             STICKY_RECONNECT,           false)
        QN_DECLARE_RW_PROPERTY(QString,                     logLevel,               setLogLevel,                LOG_LEVEL,                  "none")
        QN_DECLARE_RW_PROPERTY(int,                         initialLiveBufferMs,    setInitialLiveBufferMs,     INITIAL_LIVE_BUFFER_MSECS,  50)
        QN_DECLARE_RW_PROPERTY(int,                         maximumLiveBufferMs,    setMaximumLiveBufferMs,     MAXIMUM_LIVE_BUFFER_MSECS,  500)
        QN_DECLARE_RW_PROPERTY(QString,                     detectedObjectDisplaySettings, setDetectedObjectDisplaySettings, DETECTED_OBJECT_DISPLAY_SETTINGS, QString())

        // Was used earlier. Kept to migrate old settings.
        QN_DECLARE_RW_PROPERTY(QList<QUrl>,                 knownServerUrls,        setKnownServerUrls,         KNOWN_SERVER_URLS,          QList<QUrl>())
        QN_DECLARE_RW_PROPERTY(int,                         acceptedEulaVersion,    setAcceptedEulaVersion,     ACCEPTED_EULA_VERSION,      0)

        QN_DECLARE_RW_PROPERTY(bool, allLayoutsSelectionDialogMode, setAllLayoutsSelectionDialogMode, ALL_LAYOUTS_SELECTION_DIALOG_MODE, 0)
        QN_DECLARE_RW_PROPERTY(QString, systemUpdaterState, setSystemUpdaterState, SYSTEM_UPDATER_STATE, 0)

        // Disable mt decoding if false, enable auto select if true
        QN_DECLARE_R_PROPERTY(bool,                         allowMtDecoding,                                    ALLOW_MT_DECODING,          true)

        // Always force MT decoding if true
        QN_DECLARE_R_PROPERTY(bool,                         forceMtDecoding,                                    FORCE_MT_DECODING,          false)

        QN_DECLARE_R_PROPERTY(bool, saveCredentialsAllowed, SAVE_CREDENTIALS_ALLOWED, true)
        QN_DECLARE_RW_PROPERTY(bool, restoreUserSessionData, setRestoreUserSessionData, RESTORE_USER_SESSION, true)

        QN_DECLARE_RW_PROPERTY(bool, isAutoFpsLimit, setAutoFpsLimit, AUTO_FPS_LIMIT, false)
        QN_DECLARE_RW_PROPERTY(bool, isFisheyeCalibrationGridShown, setFisheyeCalibrationGridShown, SHOW_FISHEYE_CALIBRATION_GRID, false)

        QN_DECLARE_RW_PROPERTY(bool, playAudioForAllItems, setPlayAudioForAllItems, PLAY_AUDIO_FOR_ALL_ITEMS, false)

        QN_DECLARE_RW_PROPERTY(bool, isHardwareDecodingEnabled, setHardwareDecodingEnabled, HARDWARE_DECODING_ENABLED, false)
        QN_DECLARE_RW_PROPERTY(bool, isPtzAimOverlayEnabled, setPtzAimOverlayEnabled, PTZ_AIM_OVERLAY_ENABLED, false)
        QN_DECLARE_RW_PROPERTY(int, maxHardwareDecoders, setMaxHardwareDecoders, MAX_HARDWARE_DECODERS, 8)
        QN_DECLARE_R_PROPERTY(bool, isBrowseLogsVisible, BROWSE_LOGS_VISIBLE, true)

        QN_DECLARE_RW_PROPERTY(bool, muteOnAudioTransmit, setMuteOnAudioTransmit,
            MUTE_ON_AUDIO_TRANSMIT, true)

        QN_DECLARE_RW_PROPERTY(AuthAllowedUrls,
            authAllowedUrls,
            setAuthAllowedUrls,
            AUTH_ALLOWED_URLS,
            AuthAllowedUrls{})

    QN_END_PROPERTY_STORAGE()

    // Migration between 4.2 and 4.3.
    void migrateLastUsedConnection();
    // Migration between 4.3 and 4.3.
    void migrateMediaFolders();

private:
    const bool m_readOnly;
    QSettings* m_settings;
    bool m_loading = true;
};


#define qnSettings (QnClientSettings::instance())
