// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API ServerRemoteAccessWatcher: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    ServerRemoteAccessWatcher(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~ServerRemoteAccessWatcher() override;

private:
    void onResourcesAdded(const QnResourceList& resources);
    void onResourcesRemoved(const QnResourceList& resources);
    void updateRemoteAccess(const ServerResourcePtr& server);
};

} // namespace nx::vms::client::desktop
