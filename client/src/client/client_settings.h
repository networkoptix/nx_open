#ifndef QN_CLIENT_SETTINGS_H
#define QN_CLIENT_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>
#include <utils/common/software_version.h>
#include <utils/common/singleton.h>

#include <client/client_globals.h>
#include <client/client_connection_data.h>
#include <client/client_model_types.h>

class QSettings;

class QnClientSettings: public QnPropertyStorage, public Singleton<QnClientSettings> {
    Q_OBJECT
    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        MAX_VIDEO_ITEMS,
        DOWNMIX_AUDIO,
        AUDIO_VOLUME,
        MEDIA_FOLDER,
        EXTRA_MEDIA_FOLDERS,
        OPEN_LAYOUTS_ON_LOGIN,
        SOFTWARE_YUV,
        USER_WORKBENCH_STATES,
        SERVER_STORAGE_STATES,
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
        IGNORED_UPDATE_VERSION,

        TOUR_CYCLE_TIME,
        IP_SHOWN_IN_TREE,

        USE_HARDWARE_DECODING,
        TIME_MODE,

        DEV_MODE,

        TREE_PINNED,
        TREE_OPENED,
        SLIDER_OPENED,
        TITLE_OPENED,
        NOTIFICATIONS_PINNED,
        NOTIFICATIONS_OPENED,

        STORED_PASSWORD,

        CLOCK_24HOUR,
        CLOCK_WEEKDAY,
        CLOCK_DATE,
        CLOCK_SECONDS,

        POPUP_SYSTEM_HEALTH,
        
        AUTO_START,

        /** Filter value for network connections in the statistics widget */
        STATISTICS_NETWORK_FILTER,

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
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(int,                         maxVideoItems,          setMaxVideoItems,           MAX_VIDEO_ITEMS,            24)
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
        QN_DECLARE_RW_PROPERTY(QnServerStorageStateHash,    serverStorageStates,    setServerStorageStates,     SERVER_STORAGE_STATES,      QnServerStorageStateHash())
        QN_DECLARE_RW_PROPERTY(QnLicenseWarningStateHash,   licenseWarningStates,   setLicenseWarningStates,    LICENSE_WARNING_STATES,     QnLicenseWarningStateHash())
        QN_DECLARE_R_PROPERTY (QnConnectionData,            defaultConnection,                                  DEFAULT_CONNECTION,         QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionData,            lastUsedConnection,     setLastUsedConnection,      LAST_USED_CONNECTION,       QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionDataList,        customConnections,      setCustomConnections,       CUSTOM_CONNECTIONS,         QnConnectionDataList())
        QN_DECLARE_RW_PROPERTY(int,                         debugCounter,           setDebugCounter,            DEBUG_COUNTER,              0)
        QN_DECLARE_RW_PROPERTY(QString,                     extraTranslationsPath,  setExtraTranslationsPath,   EXTRA_TRANSLATIONS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                     extraPtzMappingsPath,   setExtraPtzMappingsPath,    EXTRA_PTZ_MAPPINGS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                     translationPath,        setLanguage,                TRANSLATION_PATH,           QLatin1String(":/translations/client_en.qm"))
        QN_DECLARE_RW_PROPERTY(QUrl,                        updateFeedUrl,          setUpdateFeedUrl,           UPDATE_FEED_URL,            QUrl())
        QN_DECLARE_RW_PROPERTY(QnSoftwareVersion,           ignoredUpdateVersion,   setIgnoredUpdateVersion,    IGNORED_UPDATE_VERSION,     QnSoftwareVersion())
        QN_DECLARE_RW_PROPERTY(int,                         tourCycleTime,          setTourCycleTime,           TOUR_CYCLE_TIME,            4000)
        QN_DECLARE_RW_PROPERTY(bool,                        isIpShownInTree,        setIpShownInTree,           IP_SHOWN_IN_TREE,           true)
        QN_DECLARE_RW_PROPERTY(bool,                        isHardwareDecodingUsed, setUseHardwareDecoding,     USE_HARDWARE_DECODING,      false)
        QN_DECLARE_RW_PROPERTY(Qn::TimeMode,                timeMode,               setTimeMode,                TIME_MODE,                  Qn::ServerTimeMode)
        QN_DECLARE_RW_PROPERTY(bool,                        isDevMode,              setDevMode,                 DEV_MODE,                   false)
        QN_DECLARE_RW_PROPERTY(bool,                        isTreePinned,           setTreePinned,              TREE_PINNED,                true)
        QN_DECLARE_RW_PROPERTY(bool,                        isTreeOpened,           setTreeOpened,              TREE_OPENED,                true)
        QN_DECLARE_RW_PROPERTY(bool,                        isSliderOpened,         setSliderOpened,            SLIDER_OPENED,              true)
        QN_DECLARE_RW_PROPERTY(bool,                        isTitleOpened,          setTitleOpened,             TITLE_OPENED,               true)
        QN_DECLARE_RW_PROPERTY(bool,                        isNotificationsPinned,  setNotificationsPinned,     NOTIFICATIONS_PINNED,       false)
        QN_DECLARE_RW_PROPERTY(bool,                        isNotificationsOpened,  setNotificationsOpened,     NOTIFICATIONS_OPENED,       false)
        QN_DECLARE_RW_PROPERTY(QString,                     storedPassword,         setStoredPassword,          STORED_PASSWORD,            QString())
        QN_DECLARE_RW_PROPERTY(bool,                        isClock24Hour,          setClock24Hour,             CLOCK_24HOUR,               true)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockWeekdayOn,       setClockWeekdayOn,          CLOCK_WEEKDAY,              false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockDateOn,          setClockDateOn,             CLOCK_DATE,                 false)
        QN_DECLARE_RW_PROPERTY(bool,                        isClockSecondsOn,       setClockSecondsOn,          CLOCK_SECONDS,              true)
        QN_DECLARE_RW_PROPERTY(quint64,                     popupSystemHealth,      setPopupSystemHealth,       POPUP_SYSTEM_HEALTH,        0xFFFFFFFFFFFFFFFFull)
        QN_DECLARE_RW_PROPERTY(bool,                        autoStart,              setAutoStart,               AUTO_START,                 false)
        QN_DECLARE_R_PROPERTY (int,                         statisticsNetworkFilter,                            STATISTICS_NETWORK_FILTER,  1)
    QN_END_PROPERTY_STORAGE()

private:
    QSettings *m_settings;
    bool m_loading;
};


#define qnSettings (QnClientSettings::instance())

#endif // QN_CLIENT_SETTINGS_H
