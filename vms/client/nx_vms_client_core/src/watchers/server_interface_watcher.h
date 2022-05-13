// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/discovery/manager.h>

// TODO: #dklychkov move the watcher to the network folder, as the router and module finder
class NX_VMS_CLIENT_CORE_API QnServerInterfaceWatcher:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
public:
    explicit QnServerInterfaceWatcher(QObject* parent = nullptr);

private slots:
    void at_connectionChanged(nx::vms::discovery::ModuleEndpoint module);
    void at_connectionChangedById(QnUuid id);
    void at_resourcePool_statusChanged(const QnResourcePtr& resource);
    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);

private:
    void updatePrimaryInterface(const QnMediaServerResourcePtr& server);
};
