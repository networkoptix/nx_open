
#pragma once

#include <QtCore/QElapsedTimer>

#include <network/module_information.h>
#include <network/base_system_description.h>
#include <nx/utils/uuid.h>

class QnSystemDescription : public QnBaseSystemDescription
{
    typedef QnBaseSystemDescription base_type;

public:
    typedef QSharedPointer<QnSystemDescription> PointerType;
    static PointerType createLocalSystem(const QString &systemId
        , const QString &systemName);

    static PointerType createCloudSystem(const QString &systemId
        , const QString &systemName
        , const QString &ownerAccountEmail
        , const QString &ownerFullName);

    virtual ~QnSystemDescription();

public: // overrides
    QString id() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    ServersList servers() const override;

    bool containsServer(const QnUuid &serverId) const override;

    QnModuleInformation getServer(const QnUuid &serverId) const override;

    QString getServerHost(const QnUuid &serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid &serverId) const override;

public:
    enum { kDefaultPriority = 0 };
    void addServer(const QnModuleInformation &serverInfo, int priority);

    QnServerFields updateServer(const QnModuleInformation &serverInfo);

    void removeServer(const QnUuid &serverId);

    void setServerHost(const QnUuid &serverId, const QString &host);

private:
    QnSystemDescription(const QString &systemId
        , const QString &systemName);

    QnSystemDescription(const QString &systemId
        , const QString &systemName
        , const QString &cloudOwnerAccountEmail
        , const QString &ownerFullName);

private:
    typedef QHash<QnUuid, QnModuleInformation> ServerInfoHash;
    typedef QHash<QnUuid, QElapsedTimer> ServerLastUpdateTimeHash;
    typedef QHash<QnUuid, QString> HostsHash;
    typedef QMultiMap<int, QnUuid> PrioritiesMap;

    const QString m_id;
    const QString m_systemName;
    const QString m_ownerAccountEmail;
    const QString m_ownerFullName;
    const bool m_isCloudSystem;
    ServerLastUpdateTimeHash m_serverTimestamps;
    ServerInfoHash m_servers;
    PrioritiesMap m_prioritized;
    HostsHash m_hosts;
};