#ifndef QN_SETTINGS_H
#define QN_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>

class QSettings;

struct QnConnectionData {
    QnConnectionData(): readOnly(false) {}

    inline bool operator==(const QnConnectionData &other) const
    { return name == other.name && url == other.url; }

    QString name;
    QUrl url;
    bool readOnly;
};

typedef QList<QnConnectionData> QnConnectionDataList;

Q_DECLARE_METATYPE(QnConnectionData);
Q_DECLARE_METATYPE(QnConnectionDataList);


class QnSettings: public QnPropertyStorage {
    Q_OBJECT;

    typedef QnPropertyStorage base_type;

public:
    enum Variable {
        MAX_VIDEO_ITEMS,
        DOWNMIX_AUDIO,
        FIRST_RUN,
        MEDIA_FOLDER,
        EXTRA_MEDIA_FOLDERS,
        BACKGROUND_ANIMATED,
        BACKGROUND_COLOR,
        OPEN_LAYOUTS_ON_LOGIN,
        SOFTWARE_YUV,
        LAST_USED_CONNECTION,
        CONNECTIONS,

        DEBUG_COUNTER,

        VARIABLE_COUNT
    };
    
    QnSettings();
    virtual ~QnSettings();

    static QnSettings *instance();

    void load();
    void save();

    //void addAuxMediaRoot(const QString &root);

    virtual bool setValue(int id, const QVariant &value) override;


signals:
    /**
     * This signal is emitted whenever last used connection changes.
     *
     * Note that due to implementation limitations, this signal may get emitted
     * even if the actual connection parameters didn't change.
     */
    void lastUsedConnectionChanged();

protected:
    virtual QVariant updateValueFromSettings(QSettings *settings, int id, const QVariant &defaultValue) override;
    virtual void submitValueToSettings(QSettings *settings, int id, const QVariant &value) const override;

    virtual void lock() const override;
    virtual void unlock() const override;

private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT);
        QN_DECLARE_RW_PROPERTY(int,                     maxVideoItems,          setMaxVideoItems,           MAX_VIDEO_ITEMS,            24);
        QN_DECLARE_RW_PROPERTY(bool,                    isAudioDownmixed,       setAudioDownmixed,          DOWNMIX_AUDIO,              false);
        QN_DECLARE_RW_PROPERTY(bool,                    isFirstRun,             setFirstRun,                FIRST_RUN,                  true);
        QN_DECLARE_RW_PROPERTY(QString,                 mediaFolder,            setMediaFolder,             MEDIA_FOLDER,               QString());
        QN_DECLARE_RW_PROPERTY(QStringList,             extraMediaFolders,      setExtraMediaFolders,       EXTRA_MEDIA_FOLDERS,        QStringList());
        QN_DECLARE_RW_PROPERTY(bool,                    isBackgroundAnimated,   setBackgroundAnimated,      BACKGROUND_ANIMATED,        true);
        QN_DECLARE_RW_PROPERTY(QColor,                  backgroundColor,        setBackgroundColor,         BACKGROUND_COLOR,           QColor());
        QN_DECLARE_RW_PROPERTY(bool,                    isLayoutsOpenedOnLogin, setLayoutsOpenedOnLogin,    OPEN_LAYOUTS_ON_LOGIN,      false);
        QN_DECLARE_RW_PROPERTY(bool,                    isSoftwareYuv,          setSoftwareYuv,             SOFTWARE_YUV,               false);
        QN_DECLARE_RW_PROPERTY(QnConnectionData,        lastUsedConnection,     setLastUsedConnection,      LAST_USED_CONNECTION,       QnConnectionData());
        QN_DECLARE_RW_PROPERTY(QnConnectionDataList,    connections,            setConnections,             CONNECTIONS,                QnConnectionDataList());
        QN_DECLARE_RW_PROPERTY(int,                     debugCounter,           setDebugCounter,            DEBUG_COUNTER,              0);
    QN_END_PROPERTY_STORAGE();

private:
    mutable QMutex m_mutex;
    QSettings *m_settings;
};


#define qnSettings QnSettings::instance()

#endif // QN_SETTINGS_H
