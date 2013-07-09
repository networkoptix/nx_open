#ifndef QN_SETTINGS_H
#define QN_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>

#include <ui/workbench/workbench_state.h>

#include <client/client_globals.h>


class QSettings;

// TODO: #gdm move these two classes into separate file.
struct QnConnectionData {
    QnConnectionData(): readOnly(false) {}
    QnConnectionData(const QString &name, const QUrl &url, bool readOnly = false): name(name), url(url), readOnly(readOnly) {}

    bool operator==(const QnConnectionData &other) const { 
        return name == other.name && url == other.url; 
    }

    bool operator!=(const QnConnectionData &other) const {
        return !(*this == other);
    }

    bool isValid() const {
        return url.isValid() && !url.isRelative() && !url.host().isEmpty();
    }

    QString name;
    QUrl url;
    bool readOnly;
};

// TODO: #gdm 
// * Use \returns doxytag.
// * Don't use "-" after param name, it will be displayed in generated docs.
//   A better way is to use indentation. See comment for QnConnectionDataList::contains().
class QnConnectionDataList: public QList<QnConnectionData> {
public:
    QnConnectionDataList(): QList<QnConnectionData>(){}

    /**
     * \param name                      Name of the connection.
     * \returns                         True if the list contains a connection with the provided name, false otherwise.
     */
    bool contains(const QString &name);

    /**
     * Returns the first occurrence of a connection in the list with the
     * name provided. Returns empty connection if no such connection was found.
     * \param name - name of the connection
     */
    QnConnectionData getByName(const QString &name);

    /**
     * Removes the first occurrence of a connection in the list with the
     * name provided and returns true on success; otherwise returns false.
     * \param name - name of the connection
     */
    bool removeOne(const QString &name);

    /**
     * Finds the first occurrence of a connection with the url provided
     * and places it in the head of the list.
     * \param url - url of the connection (password is not used)
     * \returns true if the reorder was successful.
     */
    bool reorderByUrl(const QUrl &url);

    /**
     * Generates the unique name with the provided name as the base,
     * appending number to the end of the string.
     * \param base - base name of the connection
     */
    QString generateUniqueName(const QString &base);

    /**
     * Returns default name for the last used connection.
     */
    static QString defaultLastUsedName();
};

Q_DECLARE_METATYPE(QnConnectionData)
Q_DECLARE_METATYPE(QnConnectionDataList)


class QnSettings: public QnPropertyStorage {
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

        BACKGROUND_EDITABLE,
        BACKGROUND_ANIMATED,
        BACKGROUND_COLOR,

        DEFAULT_CONNECTION,
        LAST_USED_CONNECTION,
        CUSTOM_CONNECTIONS,

        DEBUG_COUNTER,

        EXTRA_TRANSLATIONS_PATH,
        TRANSLATION_PATH,

        UPDATE_FEED_URL,
        UPDATES_ENABLED,

        TOUR_CYCLE_TIME,
        IP_SHOWN_IN_TREE,

        TIME_MODE,

        DEV_MODE,

        VARIABLE_COUNT
    };
    
    QnSettings();
    virtual ~QnSettings();

    static QnSettings *instance();

    void load();
    void save();

protected:
    virtual void updateValuesFromSettings(QSettings *settings, const QList<int> &ids) override;

    virtual QVariant readValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;
    virtual void writeValueToSettings(QSettings *settings, int id, const QVariant &value) const override;

    virtual UpdateStatus updateValue(int id, const QVariant &value) override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(int,                     maxVideoItems,          setMaxVideoItems,           MAX_VIDEO_ITEMS,            24)
        QN_DECLARE_RW_PROPERTY(bool,                    isAudioDownmixed,       setAudioDownmixed,          DOWNMIX_AUDIO,              false)
        QN_DECLARE_RW_PROPERTY(qreal,                   audioVolume,            setAudioVolume,             AUDIO_VOLUME,               1.0)
        QN_DECLARE_RW_PROPERTY(QString,                 mediaFolder,            setMediaFolder,             MEDIA_FOLDER,               QString())
        QN_DECLARE_RW_PROPERTY(QStringList,             extraMediaFolders,      setExtraMediaFolders,       EXTRA_MEDIA_FOLDERS,        QStringList())
        QN_DECLARE_RW_PROPERTY(bool,                    isBackgroundEditable,   setBackgroundEditable,      BACKGROUND_EDITABLE,        false)
        QN_DECLARE_RW_PROPERTY(bool,                    isBackgroundAnimated,   setBackgroundAnimated,      BACKGROUND_ANIMATED,        true)
        QN_DECLARE_RW_PROPERTY(QColor,                  backgroundColor,        setBackgroundColor,         BACKGROUND_COLOR,           QColor())
        QN_DECLARE_RW_PROPERTY(bool,                    isLayoutsOpenedOnLogin, setLayoutsOpenedOnLogin,    OPEN_LAYOUTS_ON_LOGIN,      false)
        QN_DECLARE_RW_PROPERTY(bool,                    isSoftwareYuv,          setSoftwareYuv,             SOFTWARE_YUV,               false)
        QN_DECLARE_RW_PROPERTY(QnWorkbenchStateHash,    userWorkbenchStates,    setUserWorkbenchStates,     USER_WORKBENCH_STATES,      QnWorkbenchStateHash())
        QN_DECLARE_R_PROPERTY (QnConnectionData,        defaultConnection,                                  DEFAULT_CONNECTION,         QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionData,        lastUsedConnection,     setLastUsedConnection,      LAST_USED_CONNECTION,       QnConnectionData())
        QN_DECLARE_RW_PROPERTY(QnConnectionDataList,    customConnections,      setCustomConnections,       CUSTOM_CONNECTIONS,         QnConnectionDataList())
        QN_DECLARE_RW_PROPERTY(int,                     debugCounter,           setDebugCounter,            DEBUG_COUNTER,              0)
        QN_DECLARE_RW_PROPERTY(QString,                 extraTranslationsPath,  setExtraTranslationsPath,   EXTRA_TRANSLATIONS_PATH,    QLatin1String(""))
        QN_DECLARE_RW_PROPERTY(QString,                 translationPath,        setLanguage,                TRANSLATION_PATH,           QLatin1String(":/translations/client_en.qm"))
        QN_DECLARE_RW_PROPERTY(QUrl,                    updateFeedUrl,          setUpdateFeedUrl,           UPDATE_FEED_URL,            QUrl())
        QN_DECLARE_RW_PROPERTY(bool,                    isUpdatesEnabled,       setUpdatesEnabled,          UPDATES_ENABLED,            true)
        QN_DECLARE_RW_PROPERTY(int,                     tourCycleTime,          setTourCycleTime,           TOUR_CYCLE_TIME,            4000)
        QN_DECLARE_RW_PROPERTY(bool,                    isIpShownInTree,        setIpShownInTree,           IP_SHOWN_IN_TREE,           true)
        QN_DECLARE_RW_PROPERTY(Qn::TimeMode,            timeMode,               setTimeMode,                TIME_MODE,                  Qn::ServerTimeMode)
        QN_DECLARE_RW_PROPERTY(bool,                    isDevMode,              setDevMode,                 DEV_MODE,                   false)
    QN_END_PROPERTY_STORAGE()

private:
    QSettings *m_settings;
    bool m_loading;
};


#define qnSettings QnSettings::instance()

#endif // QN_SETTINGS_H
