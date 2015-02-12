#ifndef QN_CLIENT_SETTINGS_H
#define QN_CLIENT_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <utils/common/uuid.h>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>
#include <utils/common/software_version.h>
#include <utils/common/singleton.h>

#include <client/client_globals.h>
#include <client/client_connection_data.h>
#include <client/client_model_types.h>
#include <common/common_meta_types.h>

class QSettings;
class QNetworkAccessManager;
class QNetworkReply;

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
        OPEN_LAYOUTS_ON_LOGIN,
        SOFTWARE_YUV,
        USER_WORKBENCH_STATES,
        LICENSE_WARNING_STATES,

        LAST_DATABASE_BACKUP_DIR,
        LAST_SCREENSHOT_DIR,
        LAST_RECORDING_DIR,
        LAST_EXPORT_DIR,

        DEFAULT_CONNECTION,
        LAST_USED_CONNECTION,
        CUSTOM_CONNECTIONS,

        DEBUG_COUNTER,

        EXTRA_TRANSLATIONS_PATH,
        TRANSLATION_PATH, 

        EXTRA_PTZ_MAPPINGS_PATH,

        UPDATE_FEED_URL,

        /** Check for updates automatically. */
        AUTO_CHECK_FOR_UPDATES,

        /** Disable client updates. */
        NO_CLIENT_UPDATE,

        /** Do not show update notification for the selected version. */
        IGNORED_UPDATE_VERSION,

        /** Do not show save confirmation when closing unsaved layouts. */
        IGNORE_UNSAVED_LAYOUTS,

        SHOWCASE_URL,
        SHOWCASE_ENABLED,

        SETTINGS_URL,

        TOUR_CYCLE_TIME,
        IP_SHOWN_IN_TREE,

        TIME_MODE,

        DEV_MODE,

        TREE_PINNED,
        TREE_OPENED,
        TREE_WIDTH,
        CALENDAR_PINNED,
        SLIDER_OPENED,
        TITLE_OPENED,
        NOTIFICATIONS_PINNED,
        NOTIFICATIONS_OPENED,

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

        /** Enable V-sync for OpenGL widgets */
        GL_VSYNC,

        TIMESTAMP_CORNER,

        /** Last used aspect ratio for resource item in grid layout. */
        RESOURCE_ASPECT_RATIOS,

        USER_IDLE_TIMEOUT_MSECS,

        /** Light client mode - no animations, no background, no opacity, no notifications, 1 camera only allowed. */
        LIGHT_MODE,

        /** If does not equal -1 then its value will be copied to LIGHT_MODE.
         *  It should be set from command-line and it disables light mode auto detection. */
        LIGHT_MODE_OVERRIDE,

        /** Do not show warning when a preset is being deleted but it is used by some tours. */
        PTZ_PRESET_IN_USE_WARNING_DISABLED,

        CLIENT_SKIN,

        /** Unique id for this PC for videowall construction. */
        PC_UUID,

        /** Flag that client is run in videowall mode. */
        VIDEO_WALL_MODE,

        /** Full set of background options. */
        BACKGROUND,

        /** A list of the urls that were discovered by QnDirectModuleFinder. */
        KNOWN_SERVER_URLS,

        VARIABLE_COUNT
    };

    QnClientSettings(QObject *parent = NULL);
    virtual ~QnClientSettings();

    void load();
    void save();

    /**
     * @brief isWritable    Check if settings storage is available for writing.
     * @returns             True if settings can be saved.
     */
    bool isWritable() const;

protected:
    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids) override;

    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;
    virtual void writeValueToSettings(QSettings *settings, int id, const QVariant &value) const override;

    virtual UpdateStatus updateValue(int id, const QVariant &value) override;

private:
    void loadFromWebsite();

    Q_SLOT void at_accessManager_finished(QNetworkReply *reply);

private:
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
        QN_DECLARE_RW_PROPERTY(bool,                        isLayoutsOpenedOnLogin, setLayoutsOpenedOnLogin,    OPEN_LAYOUTS_ON_LOGIN,      false)
        QN_DECLARE_RW_PROPERTY(bool,                        isSoftwareYuv,          setSoftwareYuv,             SOFTWARE_YUV,               false)
        QN_DECLARE_RW_PROPERTY(QnWorkbenchStateHash,        userWorkbenchStates,    setUserWorkbenchStates,     USER_WORKBENCH_STATES,      QnWorkbenchStateHash())
        QN_DECLARE_RW_PROPERTY(QnLicenseWarningStateHash,   licenseWarningStates,   setLicenseWarningStates,    LICENSE_WARNING_STATES,     QnLicenseWarningStateHash())
        QN_DECLARE_R_PROPERTY (QnConnectionData,            defaultConnection,                                  DEFAULT_CONNECTION,         QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionData,            lastUsedConnection,     setLastUsedConnection,      LAST_USED_CONNECTION,       QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionDataList,        customConnections,      setCustomConnections,       CUSTOM_CONNECTIONS,         QnConnectionDataList())
        QN_DECLARE_RW_PROPERTY(int,                         debugCounter,           setDebugCounter,            DEBUG_COUNTER,              0)
        QN_DECLARE_RW_PROPERTY(QString,                     extraTranslationsPath,  setExtraTranslationsPath,   EXTRA_TRANSLATIONS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                     extraPtzMappingsPath,   setExtraPtzMappingsPath,    EXTRA_PTZ_MAPPINGS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                     translationPath,        setTranslationPath,         TRANSLATION_PATH,           QLatin1String(":/translations/common_en_US.qm"))
        QN_DECLARE_RW_PROPERTY(QString,                     updateFeedUrl,          setUpdateFeedUrl,           UPDATE_FEED_URL,            QString())
        QN_DECLARE_RW_PROPERTY(bool,                        isAutoCheckForUpdates,  setAutoCheckForUpdates,     AUTO_CHECK_FOR_UPDATES,     true)
        QN_DECLARE_RW_PROPERTY(QnSoftwareVersion,           ignoredUpdateVersion,   setIgnoredUpdateVersion,    IGNORED_UPDATE_VERSION,     QnSoftwareVersion())
        QN_DECLARE_RW_PROPERTY(bool,                        ignoreUnsavedLayouts,   setIgnoreUnsavedLayouts,    IGNORE_UNSAVED_LAYOUTS,     false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClientUpdateDisabled, setClientUpdateDisabled,    NO_CLIENT_UPDATE,           false)
        QN_DECLARE_RW_PROPERTY(QUrl,                        showcaseUrl,            setShowcaseUrl,             SHOWCASE_URL,               QUrl())
        QN_DECLARE_RW_PROPERTY(bool,                        isShowcaseEnabled,      setShowcaseEnabled,         SHOWCASE_ENABLED,           false)
        QN_DECLARE_RW_PROPERTY(QUrl,                        settingsUrl,            setSettingsUrl,             SETTINGS_URL,               QUrl())
        QN_DECLARE_RW_PROPERTY(int,                         tourCycleTime,          setTourCycleTime,           TOUR_CYCLE_TIME,            4000)
        QN_DECLARE_RW_PROPERTY(bool,                        isIpShownInTree,        setIpShownInTree,           IP_SHOWN_IN_TREE,           true)
        QN_DECLARE_RW_PROPERTY(Qn::TimeMode,                timeMode,               setTimeMode,                TIME_MODE,                  Qn::ServerTimeMode)
        QN_DECLARE_RW_PROPERTY(bool,                        isDevMode,              setDevMode,                 DEV_MODE,                   false)
        QN_DECLARE_RW_PROPERTY(bool,                        isTreePinned,           setTreePinned,              TREE_PINNED,                true)
        QN_DECLARE_RW_PROPERTY(bool,                        isCalendarPinned,       setCalendarPinned,          CALENDAR_PINNED,            false)
        QN_DECLARE_RW_PROPERTY(bool,                        isTreeOpened,           setTreeOpened,              TREE_OPENED,                true)
        QN_DECLARE_RW_PROPERTY(int,                         treeWidth,              setTreeWidth,               TREE_WIDTH,                 250)
        QN_DECLARE_RW_PROPERTY(bool,                        isSliderOpened,         setSliderOpened,            SLIDER_OPENED,              true)
        QN_DECLARE_RW_PROPERTY(bool,                        isTitleOpened,          setTitleOpened,             TITLE_OPENED,               true)
        QN_DECLARE_RW_PROPERTY(bool,                        isNotificationsPinned,  setNotificationsPinned,     NOTIFICATIONS_PINNED,       true)
        QN_DECLARE_RW_PROPERTY(bool,                        isNotificationsOpened,  setNotificationsOpened,     NOTIFICATIONS_OPENED,       true)
        QN_DECLARE_RW_PROPERTY(bool,                        isClock24Hour,          setClock24Hour,             CLOCK_24HOUR,               true)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockWeekdayOn,       setClockWeekdayOn,          CLOCK_WEEKDAY,              false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockDateOn,          setClockDateOn,             CLOCK_DATE,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockSecondsOn,       setClockSecondsOn,          CLOCK_SECONDS,              true)
        QN_DECLARE_RW_PROPERTY(quint64,                     popupSystemHealth,      setPopupSystemHealth,       POPUP_SYSTEM_HEALTH,        0xFFFFFFFFFFFFFFFFull)
        QN_DECLARE_RW_PROPERTY(bool,                        autoStart,              setAutoStart,               AUTO_START,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        autoLogin,              setAutoLogin,               AUTO_LOGIN,                 false)
        QN_DECLARE_R_PROPERTY (int,                         statisticsNetworkFilter,                            STATISTICS_NETWORK_FILTER,  1)
        QN_DECLARE_RW_PROPERTY(bool,                        layoutKeepAspectRatio,  setLayoutKeepAspectRatio,   LAYOUT_KEEP_ASPECT_RATIO,   true)
        QN_DECLARE_RW_PROPERTY(QString,                     backgroundsFolder,      setBackgroundsFolder,       BACKGROUNDS_FOLDER,         QString())
        QN_DECLARE_RW_PROPERTY(bool,                        isGlDoubleBuffer,       setGLDoubleBuffer,          GL_DOUBLE_BUFFER,           true)
        QN_DECLARE_RW_PROPERTY(bool,                        isVSyncEnabled,         setVSyncEnabled,            GL_VSYNC,                   true)
        QN_DECLARE_RW_PROPERTY(QnAspectRatioHash,           resourceAspectRatios,   setResourceAspectRatios,    RESOURCE_ASPECT_RATIOS,     QnAspectRatioHash())
        QN_DECLARE_RW_PROPERTY(quint64,                     userIdleTimeoutMSecs,   setUserIdleTimeoutMSecs,    USER_IDLE_TIMEOUT_MSECS,    0)
        QN_DECLARE_RW_PROPERTY(bool,                        isPtzPresetInUseWarningDisabled,    setPtzPresetInUseWarningDisabled,   PTZ_PRESET_IN_USE_WARNING_DISABLED, false)
        QN_DECLARE_RW_PROPERTY(Qn::Corner,                  timestampCorner,        setTimestampCorner,         TIMESTAMP_CORNER,           Qn::BottomRightCorner)
        QN_DECLARE_RW_PROPERTY(Qn::LightModeFlags,          lightMode,              setLightMode,               LIGHT_MODE,                 0) 
        QN_DECLARE_RW_PROPERTY(int,                         lightModeOverride,      setLightModeOverride,       LIGHT_MODE_OVERRIDE,        -1)
        QN_DECLARE_RW_PROPERTY(Qn::ClientSkin,              clientSkin,             setClientSkin,              CLIENT_SKIN,                Qn::DarkSkin)
        QN_DECLARE_RW_PROPERTY(QnClientBackground,          background,             setBackground,              BACKGROUND,                 QnClientBackground())
        QN_DECLARE_RW_PROPERTY(QnUuid,                      pcUuid,                 setPcUuid,                  PC_UUID,                    QnUuid())
        QN_DECLARE_RW_PROPERTY(bool,                        isVideoWallMode,        setVideoWallMode,           VIDEO_WALL_MODE,            false)
        QN_DECLARE_RW_PROPERTY(QList<QUrl>,                 knownServerUrls,        setKnownServerUrls,         KNOWN_SERVER_URLS,          QList<QUrl>())
    QN_END_PROPERTY_STORAGE()

private:
    QNetworkAccessManager *m_accessManager;
    QSettings *m_settings;
    bool m_loading;
};


#define qnSettings (QnClientSettings::instance())

#endif // QN_CLIENT_SETTINGS_H
