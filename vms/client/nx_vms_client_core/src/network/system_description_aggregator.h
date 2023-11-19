// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSharedPointer>

#include <network/base_system_description.h>
#include <nx/vms/api/data/module_information.h>

class QnSystemDescriptionAggregator: public QnBaseSystemDescription
{
    using base_type = QnBaseSystemDescription;

public:
    QnSystemDescriptionAggregator(int priority, const QnSystemDescriptionPtr& systemDescription);
    virtual ~QnSystemDescriptionAggregator() = default;

    bool isAggregator() const;

    bool containsSystem(const QString& systemId) const;
    bool containsSystem(int priority) const;

    void mergeSystem(int priority, const QnSystemDescriptionPtr& system);

    void removeSystem(const QString& systemId, int priority);

public: // overrides
    QString id() const override;

    QnUuid localId() const override;

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

    bool isReachableServer(const QnUuid& serverId) const override;

    bool containsServer(const QnUuid& serverId) const override;

    nx::vms::api::ModuleInformationWithAddresses getServer(const QnUuid& serverId) const override;

    nx::utils::Url getServerHost(const QnUuid& serverId) const override;

    QSet<nx::utils::Url> getServerRemoteAddresses(const QnUuid& serverId) const override;

    qint64 getServerLastUpdatedMs(const QnUuid& serverId) const override;

    bool hasLocalServer() const override;

    QnSystemCompatibility systemCompatibility() const override;

    virtual bool isOauthSupported() const override;

    virtual nx::utils::SoftwareVersion version() const override;

private:
    void emitSystemChanged();

    void onSystemNameChanged(const QnSystemDescriptionPtr& system);
    void onOnlineStateChanged();

    ServersList gatherServers() const;

    void updateServers();

    void handleServerChanged(const QnUuid& serverId, QnServerFields fields);

    bool isEmptyAggregator() const;

private:
    typedef QMap<int, QnSystemDescriptionPtr> SystemsMap;
    SystemsMap m_systems;
    ServersList m_servers;
};
