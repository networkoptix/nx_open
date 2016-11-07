
#pragma once

#include <QtCore/QElapsedTimer>

#include <network/module_information.h>
#include <network/base_system_description.h>
#include <nx/utils/uuid.h>

class QnSystemDescription : public QnBaseSystemDescription
{
    Q_OBJECT

    typedef QnBaseSystemDescription base_type;

public:
    typedef QSharedPointer<QnSystemDescription> PointerType;

    static PointerType createFactorySystem(const QString& systemId);

    static PointerType createLocalSystem(
        const QString &systemId,
        const QnUuid &localSystemId,
        const QString &systemName);

    static PointerType createCloudSystem(
        const QString &systemId,
        const QnUuid& localSystemId,
        const QString &systemName,
        const QString &ownerAccountEmail,
        const QString &ownerFullName);

    virtual ~QnSystemDescription();

public: // overrides
    QString id() const override;

    QnUuid localId() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    bool isNewSystem() const override;

    bool isOnline() const override;

    ServersList servers() const override;

    bool isOnlineServer(const QnUuid& serverId) const override;

    bool containsServer(const QnUuid& serverId) const override;

    QnModuleInformation getServer(const QnUuid& serverId) const override;

    QUrl getServerHost(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

    bool hasInternet() const override;

public:
    enum { kDefaultPriority = 0 };
    void addServer(const QnModuleInformation& serverInfo,
        int priority, bool online = true);

    QnServerFields updateServer(const QnModuleInformation& serverInfo);

    void removeServer(const QnUuid& serverId);

    void setServerHost(const QnUuid& serverId, const QUrl& host);

    void setName(const QString& value);

private:
    // Ctor for factory (new) system
    QnSystemDescription(const QString& systemId);

    // Ctor for local system
    QnSystemDescription(const QString& systemId,
        const QnUuid &localSystemId,
        const QString& systemName);

    // Ctor for cloud system
    QnSystemDescription(
        const QString& systemId,
        const QnUuid &localSystemId,
        const QString& systemName,
        const QString& cloudOwnerAccountEmail,
        const QString& ownerFullName);

    static QString extractSystemName(const QString& systemName);

    void handleOnlineServerAdded(const QnUuid& serverId);

    void handleServerRemoved(const QnUuid& serverId);

    void updateHasInternetState();

    void init();

private:
    typedef QHash<QnUuid, QnModuleInformation> ServerInfoHash;
    typedef QHash<QnUuid, QElapsedTimer> ServerLastUpdateTimeHash;
    typedef QHash<QnUuid, QUrl> HostsHash;
    typedef QMultiMap<int, QnUuid> PrioritiesMap;
    typedef QSet<QnUuid> IdsSet;

    const QString m_id;
    const QnUuid m_localId;
    const QString m_ownerAccountEmail;
    const QString m_ownerFullName;
    const bool m_isCloudSystem;
    const bool m_isNewSystem;
    QString m_systemName;
    ServerLastUpdateTimeHash m_serverTimestamps;
    ServerInfoHash m_servers;
    PrioritiesMap m_prioritized;
    HostsHash m_hosts;
    IdsSet m_onlineServers;
    bool m_hasInternet;
};