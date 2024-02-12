// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/url.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

/**
 * OtherServersManager informs another components that "Other server" resource tree nodes should be
 * updated. The resulting node id may be equal to incompatible server resource node which is located
 * in current system subtree.
 */

class NX_VMS_CLIENT_DESKTOP_API OtherServersManager:
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT

public:
    explicit OtherServersManager(
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);
    virtual ~OtherServersManager() override;

    void setMessageProcessor(QnClientMessageProcessor* messageProcessor);

    void start();
    void stop();

    UuidList getServers() const;
    bool containsServer(const nx::Uuid& serverId) const;
    nx::vms::api::ModuleInformationWithAddresses getModuleInformationWithAddresses(
        const nx::Uuid& serverId) const;
    nx::utils::Url getUrl(const nx::Uuid& serverId) const;

signals:
    void serverAdded(const nx::Uuid& serverId);
    void serverRemoved(const nx::Uuid& serverId);
    void serverUpdated(const nx::Uuid& serverId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
