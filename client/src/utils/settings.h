#ifndef QN_SETTINGS_H
#define QN_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtGui/QColor>

#include <utils/common/property_storage.h>

class QSettings;

class QnSettings: public QnPropertyStorage {
    Q_OBJECT;

    typedef QnPropertyStorage base_type;

public:
    QnSettings();

    static QnSettings *instance();

    void load();
    void save();

    int maxVideoItems() const;
    void setMaxVideoItems(int maxVideoItems);

    bool downmixAudio() const;
    bool isAllowChangeIP() const;
    bool isAfterFirstRun() const;
    QString mediaRoot() const;
    QStringList auxMediaRoots() const;
    bool isBackgroundAnimated() const;
    QColor backgroundColor() const;

    bool layoutsOpenedOnLogin() const;
    void setLayoutsOpenedOnLogin(bool openLayoutsOnLogin);

    void addAuxMediaRoot(const QString &root);

    bool isForceSoftYUV() const;
    void setForceSoftYUV(bool value);

    struct ConnectionData {
        ConnectionData()
            : readOnly(false)
        {}

        inline bool operator==(const ConnectionData &other) const
        { return name == other.name && url == other.url; }

        QString name;
        QUrl url;
        bool readOnly;
    };

    ConnectionData lastUsedConnection();
    void setLastUsedConnection(const ConnectionData &connection);
    QList<ConnectionData> connections();
    void setConnections(const QList<ConnectionData> &connections);

signals:
    /**
     * This signal is emitted whenever last used connection changes.
     *
     * Note that due to implementation limitations, this signal may get emitted
     * even if the actual connection parameters didn't change.
     */
    void lastUsedConnectionChanged();

private:
    void setMediaRoot(const QString &root);
    void setAllowChangeIP(bool allow);
    void setAuxMediaRoots(const QStringList &);
    void removeAuxMediaRoot(const QString &root);
    void reset();

private:
    mutable QMutex m_lock;
    QSettings *m_settings;
    ConnectionData m_lastUsed;

    int m_maxVideoItems;
    bool m_downmixAudio;
    bool m_allowChangeIP;
    bool m_afterFirstRun;
    QString m_mediaRoot;
    QStringList m_auxMediaRoots;
    bool m_animateBackground;
    bool m_openLayoutsOnLogin;
    QColor m_backgroundColor;
    bool m_forceSoftYUV;
};

#define qnSettings QnSettings::instance()

#endif // QN_SETTINGS_H
