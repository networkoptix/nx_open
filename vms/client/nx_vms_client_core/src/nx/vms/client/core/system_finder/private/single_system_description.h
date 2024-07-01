// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/module_information.h>

#include "../system_description.h"

namespace nx::vms::client::core {

/** Base class for storing information about a single system. */
class NX_VMS_CLIENT_CORE_API SingleSystemDescription: public SystemDescription
{
    Q_OBJECT

public:
    QString id() const override;

    nx::Uuid localId() const override;

    QString cloudId() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    ServersList servers() const override;

    nx::vms::api::SaasState saasState() const override;

    bool isReachableServer(const nx::Uuid& serverId) const override;

    bool containsServer(const nx::Uuid& serverId) const override;

    nx::vms::api::ModuleInformationWithAddresses getServer(const nx::Uuid& serverId) const override;

    nx::utils::Url getServerHost(const nx::Uuid& serverId) const override;

    QSet<nx::utils::Url> getServerRemoteAddresses(const nx::Uuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const nx::Uuid& serverId) const override;

    bool isReachable() const override;

    bool hasLocalServer() const override;

    QnSystemCompatibility systemCompatibility() const override;

    virtual bool isOauthSupported() const override;

    virtual nx::utils::SoftwareVersion version() const override;

    virtual QString idForToStringFromPtr() const override;

    static QString extractSystemName(const QString& systemName);

public:
    void addServer(
        const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
        bool online = true);

    QnServerFields updateServer(
        const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
        bool online = true);

    void removeServer(const nx::Uuid& serverId);

    void setServerHost(const nx::Uuid& serverId, const nx::utils::Url& host);

    void setName(const QString& value);

protected:
    SingleSystemDescription(
        const QString& systemId,
        const nx::Uuid& localSystemId,
        const QString& cloudSystemId,
        const QString& systemName);

    void handleReachableServerAdded(const nx::Uuid& serverId);

    void handleServerRemoved(const nx::Uuid& serverId);

private:
    const QString m_id;
    const nx::Uuid m_localId;
    const QString m_cloudId;
    const QString m_ownerAccountEmail;
    const QString m_ownerFullName;
    QString m_systemName;
    QHash<nx::Uuid, QElapsedTimer> m_serverTimestamps;
    QHash<nx::Uuid, nx::vms::api::ModuleInformationWithAddresses> m_servers;
    mutable QHash<nx::Uuid, QSet<nx::utils::Url>> m_remoteAddressesCache;
    QMultiMap<int, nx::Uuid> m_prioritized;
    QHash<nx::Uuid, nx::utils::Url> m_hosts;
    QSet<nx::Uuid> m_reachableServers;
};

} // namespace nx::vms::client::core
