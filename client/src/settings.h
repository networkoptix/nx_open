#ifndef QN_SETTINGS_H
#define QN_SETTINGS_H

#include <QObject>
#include <QUrl>
#include <QSettings>
#include <QStringList>
#include <QReadWriteLock>

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

    bool haveValidLicense();

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
    mutable QReadWriteLock m_lock;
    QSettings m_settings;
    Data m_data;
};

#define qnSettings QnSettings::instance()

#endif // QN_SETTINGS_H
