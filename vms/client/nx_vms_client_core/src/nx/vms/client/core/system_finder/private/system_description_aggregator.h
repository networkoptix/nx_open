// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <nx/vms/api/data/module_information.h>

#include "../system_description.h"

namespace nx::vms::client::core {

class SystemDescriptionAggregator: public SystemDescription
{
    using base_type = SystemDescription;

public:
    SystemDescriptionAggregator(int priority, const SystemDescriptionPtr& systemDescription);
    virtual ~SystemDescriptionAggregator() = default;

    bool isAggregator() const;

    bool containsSystem(const QString& systemId) const;
    bool containsSystem(int priority) const;

    void mergeSystem(int priority, const SystemDescriptionPtr& system);

    // TODO: #sivanov SystemId check is highly suspicious here, aggregator must not know about
    // model-level ids.
    void removeSystem(const QString& systemId, int priority, bool isMerge = false);

    QString id() const override;

    nx::Uuid localId() const override;

    QString name() const override;

    QString ownerAccountEmail() const override;

    QString ownerFullName() const override;

    bool isCloudSystem() const override;

    bool is2FaEnabled() const override;

    bool isNewSystem() const override;

    bool isOnline() const override;

    bool isReachable() const override;

    ServersList servers() const override;

    nx::vms::api::SaasState saasState() const override;

    bool isReachableServer(const nx::Uuid& serverId) const override;

    bool containsServer(const nx::Uuid& serverId) const override;

    nx::vms::api::ModuleInformationWithAddresses getServer(const nx::Uuid& serverId) const override;

    nx::utils::Url getServerHost(const nx::Uuid& serverId) const override;

    QSet<nx::utils::Url> getServerRemoteAddresses(const nx::Uuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const nx::Uuid& serverId) const override;

    bool hasLocalServer() const override;

    QnSystemCompatibility systemCompatibility() const override;

    virtual bool isOauthSupported() const override;

    virtual nx::utils::SoftwareVersion version() const override;

    virtual QString idForToStringFromPtr() const override;

private:
    void emitSystemChanged();

    void onSystemNameChanged(const SystemDescriptionPtr& system);
    void onOnlineStateChanged();

    ServersList gatherServers() const;

    void updateServers();

    void handleServerChanged(const nx::Uuid& serverId, QnServerFields fields);

    bool isEmptyAggregator() const;

private:
    QMap<int, SystemDescriptionPtr> m_systems;
    ServersList m_servers;
};

} // namespace nx::vms::client::core
