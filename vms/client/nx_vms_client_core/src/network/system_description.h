// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>

#include <network/base_system_description.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/module_information.h>

class NX_VMS_CLIENT_CORE_API QnSystemDescription: public QnBaseSystemDescription
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

    nx::vms::api::SaasState saasState() const override;

    bool isReachableServer(const QnUuid& serverId) const override;

    bool containsServer(const QnUuid& serverId) const override;

    nx::vms::api::ModuleInformationWithAddresses getServer(const QnUuid& serverId) const override;

    nx::utils::Url getServerHost(const QnUuid& serverId) const override;

    QSet<nx::utils::Url> getServerRemoteAddresses(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

    bool isReachable() const override;

    bool hasLocalServer() const override;

    QnSystemCompatibility systemCompatibility() const override;

    virtual bool isOauthSupported() const override;

    virtual nx::utils::SoftwareVersion version() const override;

    static QString extractSystemName(const QString& systemName);

public:
    enum { kDefaultPriority = 0 };
    void addServer(const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
        int priority, bool online = true);

    QnServerFields updateServer(
        const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
        bool online = true);

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

private:
    typedef QHash<QnUuid, nx::vms::api::ModuleInformationWithAddresses> ServerInfoHash;
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
    mutable QHash<QnUuid, QSet<nx::utils::Url>> m_remoteAddressesCache;
    PrioritiesMap m_prioritized;
    HostsHash m_hosts;
    IdsSet m_reachableServers;
};
