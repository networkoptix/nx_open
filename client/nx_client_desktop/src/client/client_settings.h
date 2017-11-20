#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtGui/QColor>

#include <common/common_meta_types.h>

#include <health/system_health.h>

#include <utils/common/app_info.h>

#include <core/resource/resource_display_info.h>

#include <client/client_globals.h>
#include <client/client_model_types.h>
#include <client/client_connection_data.h>
#include <client_core/local_connection_data.h>

#include <ui/workbench/workbench_pane_settings.h>

#include <update/update_info.h>

#include <utils/common/property_storage.h>
#include <utils/common/software_version.h>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

#include <nx/client/desktop/export/settings/export_media_persistent_settings.h>
#include <nx/client/desktop/export/settings/export_layout_persistent_settings.h>

class QSettings;

class QnClientSettings: public QnPropertyStorage, public Singleton<QnClientSettings> {
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        /** Maximum number of items that is allowed on the scene. */
        MAX_SCENE_VIDEO_ITEMS,

        /** Maximum number of items that is allowed during the preview search. */
        MAX_PREVIEW_SEARCH_ITEMS,

        DOWNMIX_AUDIO,
        AUDIO_VOLUME,
        MEDIA_FOLDER,
        EXTRA_MEDIA_FOLDERS,
        WORKBENCH_STATES,
        LICENSE_WARNING_STATES,

        LAST_DATABASE_BACKUP_DIR,
        LAST_SCREENSHOT_DIR,
        LAST_RECORDING_DIR,
        LAST_EXPORT_DIR,

        LAST_USED_CONNECTION,
        CUSTOM_CONNECTIONS,

        LOCALE,

        EXTRA_PTZ_MAPPINGS_PATH,

        /** Url for get to updates.json. */
        UPDATE_FEED_URL,

        /** ??? // TODO: #dklychkov */
        ALTERNATIVE_UPDATE_SERVERS,


        /** Latest known update info. */
        LATEST_UPDATE_INFO,

        /** Estimated update delivery date (in msecs since epoch). */
        UPDATE_DELIVERY_DATE,

        /** Disable client updates. */
        NO_CLIENT_UPDATE,

        /** Do not show update notification for the selected version. */
        IGNORED_UPDATE_VERSION,

        TOUR_CYCLE_TIME,

        /** Show extra information in tree - ip addresses, user name, videowall screen layouts. */
        EXTRA_INFO_IN_TREE,

        TIME_MODE,

        CREATE_FULL_CRASH_DUMP,

        WORKBENCH_PANES,

        CLOCK_24HOUR,
        CLOCK_WEEKDAY,
        CLOCK_DATE,
        CLOCK_SECONDS,

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

        /** Allow double buffering for openGL context */
        GL_DOUBLE_BUFFER,

        /** Allow blur on video items. */
        GL_BLUR,

        /** Enable V-sync for OpenGL widgets */
        GL_VSYNC,

        TIMESTAMP_CORNER,

        USER_IDLE_TIMEOUT_MSECS,

        /** Light client mode - no animations, no background, no opacity, no notifications, 1 camera only allowed. */
        LIGHT_MODE,

        /** If does not equal -1 then its value will be copied to LIGHT_MODE.
         *  It should be set from command-line and it disables light mode auto detection. */
        LIGHT_MODE_OVERRIDE,

        /** Unique id for this PC for videowall construction. */
        PC_UUID,

        /** Full set of background options. */
        BACKGROUND_IMAGE,

        LOG_LEVEL,
        EC2_TRAN_LOG_LEVEL,

        /** Initial and maximal live buffer lengths, in milliseconds. */
        INITIAL_LIVE_BUFFER_MSECS,
        MAXIMUM_LIVE_BUFFER_MSECS,

        TIMELAPSE_SPEED,

        LAST_LOCAL_CONNECTION_URL,
        KNOWN_SERVER_URLS,

        EXPORT_MEDIA_SETTINGS,
        EXPORT_LAYOUT_SETTINGS,
        EXPORT_BOOKMARK_SETTINGS,
        LAST_EXPORT_MODE,

        VARIABLE_COUNT
    };

    QnClientSettings(bool forceLocalSettings, QObject *parent = NULL);
    virtual ~QnClientSettings();

    void load();
    void save();

    /**
     * @brief isWritable    Check if settings storage is available for writing.
     * @returns             True if settings can be saved.
     */
    bool isWritable() const;
    QSettings* rawSettings();

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
    using ExportMediaSettings = nx::client::desktop::ExportMediaPersistentSettings;
    using ExportLayoutSettings = nx::client::desktop::ExportLayoutPersistentSettings;

    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(int,                         maxSceneVideoItems,     setMaxSceneVideoItems,      MAX_SCENE_VIDEO_ITEMS,      24)
        QN_DECLARE_RW_PROPERTY(int,                         maxPreviewSearchItems,  setMaxPreviewSearchItems,   MAX_PREVIEW_SEARCH_ITEMS,   16)
        QN_DECLARE_RW_PROPERTY(bool,                        isAudioDownmixed,       setAudioDownmixed,          DOWNMIX_AUDIO,              false)
        QN_DECLARE_RW_PROPERTY(qreal,                       audioVolume,            setAudioVolume,             AUDIO_VOLUME,               1.0)
        QN_DECLARE_RW_PROPERTY(QString,                     mediaFolder,            setMediaFolder,             MEDIA_FOLDER,               QString())
        QN_DECLARE_RW_PROPERTY(QStringList,                 extraMediaFolders,      setExtraMediaFolders,       EXTRA_MEDIA_FOLDERS,        QStringList())
        QN_DECLARE_RW_PROPERTY(QString,                     lastDatabaseBackupDir,  setLastDatabaseBackupDir,   LAST_DATABASE_BACKUP_DIR,   QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastScreenshotDir,      setLastScreenshotDir,       LAST_SCREENSHOT_DIR,        QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastRecordingDir,       setLastRecordingDir,        LAST_RECORDING_DIR,         QString())
        QN_DECLARE_RW_PROPERTY(QString,                     lastExportDir,          setLastExportDir,           LAST_EXPORT_DIR,            QString())
        QN_DECLARE_RW_PROPERTY(QnWorkbenchStateList,        workbenchStates,        setWorkbenchStates,         WORKBENCH_STATES,           QnWorkbenchStateList())
        QN_DECLARE_RW_PROPERTY(QnLicenseWarningStateHash,   licenseWarningStates,   setLicenseWarningStates,    LICENSE_WARNING_STATES,     QnLicenseWarningStateHash())
        QN_DECLARE_RW_PROPERTY(QnConnectionData,            lastUsedConnection,     setLastUsedConnection,      LAST_USED_CONNECTION,       QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QUrl,                        lastLocalConnectionUrl, setLastLocalConnectionUrl,  LAST_LOCAL_CONNECTION_URL,  QUrl())
        QN_DECLARE_RW_PROPERTY(QnConnectionDataList,        customConnections,      setCustomConnections,       CUSTOM_CONNECTIONS,         QnConnectionDataList())
        QN_DECLARE_RW_PROPERTY(QString,                     extraPtzMappingsPath,   setExtraPtzMappingsPath,    EXTRA_PTZ_MAPPINGS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                     locale,                 setLocale,                  LOCALE,                     QnAppInfo::defaultLanguage())

        /* Updates-related settings */
        QN_DECLARE_RW_PROPERTY(QString,                     updateFeedUrl,          setUpdateFeedUrl,           UPDATE_FEED_URL,            QString())
        QN_DECLARE_RW_PROPERTY(QVariantList,                alternativeUpdateServers,   setAlternativeUpdateServers,    ALTERNATIVE_UPDATE_SERVERS, QVariantList())
        QN_DECLARE_RW_PROPERTY(QnSoftwareVersion,           ignoredUpdateVersion,   setIgnoredUpdateVersion,    IGNORED_UPDATE_VERSION,     QnSoftwareVersion())
        QN_DECLARE_RW_PROPERTY(QnUpdateInfo,                latestUpdateInfo,       setLatestUpdateInfo,        LATEST_UPDATE_INFO,         QnUpdateInfo())
        QN_DECLARE_RW_PROPERTY(qint64,                      updateDeliveryDate,     setUpdateDeliveryDate,      UPDATE_DELIVERY_DATE,       0)

        QN_DECLARE_RW_PROPERTY(bool,                        isClientUpdateDisabled, setClientUpdateDisabled,    NO_CLIENT_UPDATE,           false)
        QN_DECLARE_RW_PROPERTY(int,                         tourCycleTime,          setTourCycleTime,           TOUR_CYCLE_TIME,            4000)
        QN_DECLARE_RW_PROPERTY(Qn::ResourceInfoLevel,       extraInfoInTree,        setExtraInfoInTree,         EXTRA_INFO_IN_TREE,         Qn::RI_NameOnly)
        QN_DECLARE_RW_PROPERTY(Qn::TimeMode,                timeMode,               setTimeMode,                TIME_MODE,                  Qn::ServerTimeMode)
        QN_DECLARE_R_PROPERTY (bool,                        createFullCrashDump,                                CREATE_FULL_CRASH_DUMP,     false)
        QN_DECLARE_RW_PROPERTY(QnPaneSettingsMap,           paneSettings,           setPaneSettings,            WORKBENCH_PANES,            Qn::defaultPaneSettings())
        QN_DECLARE_RW_PROPERTY(bool,                        isClock24Hour,          setClock24Hour,             CLOCK_24HOUR,               true)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockWeekdayOn,       setClockWeekdayOn,          CLOCK_WEEKDAY,              false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockDateOn,          setClockDateOn,             CLOCK_DATE,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockSecondsOn,       setClockSecondsOn,          CLOCK_SECONDS,              true)
        QN_DECLARE_RW_PROPERTY(QSet<QnSystemHealth::MessageType>, popupSystemHealth, setPopupSystemHealth,      POPUP_SYSTEM_HEALTH,        QnSystemHealth::allVisibleMessageTypes().toSet())
        QN_DECLARE_RW_PROPERTY(bool,                        autoStart,              setAutoStart,               AUTO_START,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        autoLogin,              setAutoLogin,               AUTO_LOGIN,                 false)
        QN_DECLARE_R_PROPERTY (int,                         statisticsNetworkFilter,                            STATISTICS_NETWORK_FILTER,  1)
        QN_DECLARE_RW_PROPERTY(bool,                        layoutKeepAspectRatio,  setLayoutKeepAspectRatio,   LAYOUT_KEEP_ASPECT_RATIO,   true)
        QN_DECLARE_RW_PROPERTY(QString,                     backgroundsFolder,      setBackgroundsFolder,       BACKGROUNDS_FOLDER,         QString())
        QN_DECLARE_RW_PROPERTY(bool,                        isGlDoubleBuffer,       setGLDoubleBuffer,          GL_DOUBLE_BUFFER,           true)
        QN_DECLARE_RW_PROPERTY(bool,                        isGlBlurEnabled,        setGlBlurEnabled,           GL_BLUR,                    true)
        QN_DECLARE_RW_PROPERTY(bool,                        isVSyncEnabled,         setVSyncEnabled,            GL_VSYNC,                   true)
        QN_DECLARE_RW_PROPERTY(quint64,                     userIdleTimeoutMSecs,   setUserIdleTimeoutMSecs,    USER_IDLE_TIMEOUT_MSECS,    0)
        QN_DECLARE_RW_PROPERTY(ExportMediaSettings,         exportMediaSettings,    setExportMediaSettings,     EXPORT_MEDIA_SETTINGS,      ExportMediaSettings())
        QN_DECLARE_RW_PROPERTY(ExportLayoutSettings,        exportLayoutSettings,   setExportLayoutSettings,    EXPORT_LAYOUT_SETTINGS,     ExportLayoutSettings())
        QN_DECLARE_RW_PROPERTY(ExportMediaSettings,         exportBookmarkSettings, setExportBookmarkSettings,  EXPORT_BOOKMARK_SETTINGS,   ExportMediaSettings({nx::client::desktop::ExportOverlayType::bookmark}))
        QN_DECLARE_RW_PROPERTY(QString,                     lastExportMode,         setLastExportMode,          LAST_EXPORT_MODE,           lit("media"))
        QN_DECLARE_RW_PROPERTY(Qn::LightModeFlags,          lightMode,              setLightMode,               LIGHT_MODE,                 0)
        QN_DECLARE_RW_PROPERTY(QnBackgroundImage,           backgroundImage,        setBackgroundImage,         BACKGROUND_IMAGE,           QnBackgroundImage())
        QN_DECLARE_RW_PROPERTY(QnUuid,                      pcUuid,                 setPcUuid,                  PC_UUID,                    QnUuid())
        QN_DECLARE_RW_PROPERTY(QList<QUrl>,                 knownServerUrls,        setKnownServerUrls,         KNOWN_SERVER_URLS,          QList<QUrl>())
        QN_DECLARE_RW_PROPERTY(QString,                     logLevel,               setLogLevel,                LOG_LEVEL,                  QLatin1String("none"))
        QN_DECLARE_RW_PROPERTY(QString,                     ec2TranLogLevel,        setEc2TranLogLevel,         EC2_TRAN_LOG_LEVEL,         QLatin1String("none"))
        QN_DECLARE_RW_PROPERTY(int,                         initialLiveBufferMSecs, setInitialLiveBufferMSecs,  INITIAL_LIVE_BUFFER_MSECS,  300)
        QN_DECLARE_RW_PROPERTY(int,                         maximumLiveBufferMSecs, setMaximumLiveBufferMSecs,  MAXIMUM_LIVE_BUFFER_MSECS,  600)
        QN_DECLARE_RW_PROPERTY(int,                         timelapseSpeed,         setTimelapseSpeed,          TIMELAPSE_SPEED,            10)
    QN_END_PROPERTY_STORAGE()

    void migrateKnownServerConnections();

private:
    QSettings *m_settings;
    bool m_loading;
};


#define qnSettings (QnClientSettings::instance())
