#ifndef QN_SETTINGS_H
#define QN_SETTINGS_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QStringList>
#include <QtCore/QMutex>

#include <QtGui/QColor>

class QSettings;

class QnSettings: public QObject {
    Q_OBJECT;

public:
    QnSettings();

    static QnSettings *instance();

    struct Data
    {
        int maxVideoItems;
        bool downmixAudio;
        bool allowChangeIP;
        bool afterFirstRun;
        QString mediaRoot;
        QStringList auxMediaRoots;
        bool animateBackground;
        QColor backgroundColor;
    };

    void update(const Data &data);
    void fillData(Data &data) const;

    void load();
    void save();

    int maxVideoItems() const;
    bool downmixAudio() const;
    bool isAllowChangeIP() const;
    bool isAfterFirstRun() const;
    QString mediaRoot() const;
    QStringList auxMediaRoots() const;
    bool isBackgroundAnimated() const;
    QColor backgroundColor() const;

    void addAuxMediaRoot(const QString &root);

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
    Data m_data;
};

#define qnSettings QnSettings::instance()

#endif // QN_SETTINGS_H
