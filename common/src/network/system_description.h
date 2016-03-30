
#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <network/module_information.h>
#include <nx/utils/uuid.h>
#include <nx/network/socket_common.h>

class QnSystemDescription;
typedef QSharedPointer<QnSystemDescription> QnSystemDescriptionPtr;

enum class QnServerField
{
    NoField                 = 0x00
    , NameField             = 0x01
    , SystemNameField       = 0x02
    , HostField             = 0x04
    , FlagsField            = 0x08
};
Q_DECLARE_FLAGS(QnServerFields, QnServerField)
Q_DECLARE_METATYPE(QnServerFields)

class QnSystemDescription : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    static QnSystemDescriptionPtr createLocalSystem(const QString &systemId
        , const QString &systemName);

    static QnSystemDescriptionPtr createCloudSystem(const QString &systemId
        , const QString &systemName);

    virtual ~QnSystemDescription();

    QString id() const;

    QString name() const;

    bool isCloudSystem() const;

    typedef QList<QnModuleInformation> ServersList;
    ServersList servers() const;

    enum { kDefaultPriority = 0};
    void addServer(const QnModuleInformation &serverInfo
        , int priority = kDefaultPriority);

    bool containsServer(const QnUuid &serverId) const;

    QnModuleInformation getServer(const QnUuid &serverId) const;

    QnServerFields updateServer(const QnModuleInformation &serverInfo);

    void removeServer(const QnUuid &serverId);

    void setServerHost(const QnUuid &serverId
        , const QString &host);

    QString getServerHost(const QnUuid &serverId) const;

    qint64 getServerLastUpdatedMs(const QnUuid &serverId) const;

signals:
    void serverAdded(const QnUuid &serverId);

    void serverRemoved(const QnUuid &serverId);

    void serverChanged(const QnUuid &serverId
        , QnServerFields flags);

private:
    QnSystemDescription(const QString &systemId
        , const QString &systemName
        , const bool isCloudSystem);

private:
    typedef QHash<QnUuid, QnModuleInformation> ServerInfoHash;
    typedef QHash<QnUuid, QElapsedTimer> ServerLastUpdateTimeHash;
    typedef QHash<QnUuid, QString> HostsHash;
    typedef QMultiMap<int, QnUuid> PrioritiesMap;

    const QString m_id;
    const QString m_systemName;
    const bool m_isCloudSystem;
    ServerLastUpdateTimeHash m_serverTimestamps;
    ServerInfoHash m_servers;
    PrioritiesMap m_prioritized;
    HostsHash m_hosts;


};