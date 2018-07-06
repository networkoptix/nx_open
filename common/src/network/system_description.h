#pragma once

#include <QtCore/QElapsedTimer>

#include <network/module_information.h>
#include <network/base_system_description.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class QnSystemDescription: public QnBaseSystemDescription
{
    Q_OBJECT

    typedef QnBaseSystemDescription base_type;

public:
    typedef QSharedPointer<QnSystemDescription> PointerType;

    virtual ~QnSystemDescription();

public: // overrides
    QString id() const override;

    QnUuid localId() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    ServersList servers() const override;

    bool isReachableServer(const QnUuid& serverId) const override;

    bool containsServer(const QnUuid& serverId) const override;

    QnModuleInformation getServer(const QnUuid& serverId) const override;

    nx::utils::Url getServerHost(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

    bool isReachable() const override;

    bool isConnectable() const override;

    bool safeMode() const override;

    static QString extractSystemName(const QString& systemName);

public:
    enum { kDefaultPriority = 0 };
    void addServer(const QnModuleInformation& serverInfo,
        int priority, bool online = true);

    QnServerFields updateServer(const QnModuleInformation& serverInfo);

    void removeServer(const QnUuid& serverId);

    void setServerHost(const QnUuid& serverId, const nx::utils::Url& host);

    void setName(const QString& value);

protected:
    QnSystemDescription(
        const QString& systemId,
        const QnUuid& localSystemId,
        const QString& systemName);

    void handleReachableServerAdded(const QnUuid& serverId);

    void handleServerRemoved(const QnUuid& serverId);

    void updateSafeModeState();

private:
    typedef QHash<QnUuid, QnModuleInformation> ServerInfoHash;
    typedef QHash<QnUuid, QElapsedTimer> ServerLastUpdateTimeHash;
    typedef QHash<QnUuid, nx::utils::Url> HostsHash;
    typedef QMultiMap<int, QnUuid> PrioritiesMap;
    typedef QSet<QnUuid> IdsSet;

    const QString m_id;
    const QnUuid m_localId;
    const QString m_ownerAccountEmail;
    const QString m_ownerFullName;
    QString m_systemName;
    ServerLastUpdateTimeHash m_serverTimestamps;
    ServerInfoHash m_servers;
    PrioritiesMap m_prioritized;
    HostsHash m_hosts;
    IdsSet m_reachableServers;
    bool m_safeMode;
};
