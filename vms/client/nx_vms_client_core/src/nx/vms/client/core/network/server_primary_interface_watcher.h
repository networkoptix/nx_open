// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerPrimaryInterfaceWatcher:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

public:
    ServerPrimaryInterfaceWatcher(
        SystemContext* systemContext,
        QObject* parent = nullptr);

private:
    void onConnectionChanged(nx::vms::discovery::ModuleEndpoint module);
    void onConnectionChangedById(nx::Uuid id);
    void onResourcePoolStatusChanged(const QnResourcePtr& resource);
    void onResourcesAdded(const QnResourceList& resources);

private:
    void updatePrimaryInterface(const QnMediaServerResourcePtr& server);
};

} // namespace nx::vms::client::core
