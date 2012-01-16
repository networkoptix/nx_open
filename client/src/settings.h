#ifndef global_settings_h1933
#define global_settings_h1933

#include <QUrl>
#include <QColor>

class Settings
{
public:
    static Settings& instance();

    struct Data
    {
        int maxVideoItems;
        bool downmixAudio;
        bool allowChangeIP;
        bool afterFirstRun;
        QString mediaRoot;
        QStringList auxMediaRoots;
    };

    void update(const Data& data);
    void fillData(Data& data) const;

    void load(const QString& fileName);
    void save();

    int maxVideoItems() const;
    bool downmixAudio() const;
    bool isAllowChangeIP() const;
    bool isAfterFirstRun() const;
    QString mediaRoot() const;
    QStringList auxMediaRoots() const;
    void addAuxMediaRoot(const QString& root);

    // ### separate out ?
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
    static ConnectionData lastUsedConnection();
    static void setLastUsedConnection(const ConnectionData &connection);
    static QList<ConnectionData> connections();
    static void setConnections(const QList<ConnectionData> &connections);
    // ###

    static bool haveValidLicense();

private:
    Settings();
    Settings(const Settings&) {}

    void setMediaRoot(const QString& root);
    void setAllowChangeIP(bool allow);
    void setAuxMediaRoots(const QStringList&);
    void removeAuxMediaRoot(const QString& root);
    void reset();

private:
    mutable QReadWriteLock m_RWLock;
    QString m_fileName;

    Data m_data;
};

#endif //global_settings_h1933
