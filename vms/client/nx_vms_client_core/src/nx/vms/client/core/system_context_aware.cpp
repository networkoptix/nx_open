// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <nx/utils/log/assert.h>

#include "system_context.h"

namespace nx::vms::client::core {

SystemContext* SystemContextAware::systemContext() const
{
    return dynamic_cast<SystemContext*>(nx::vms::common::SystemContextAware::systemContext());
}

std::shared_ptr<RemoteSession> SystemContextAware::session() const
{
    return systemContext()->session();
}

QnUuid SystemContextAware::currentServerId() const
{
    return systemContext()->currentServerId();
}

QnMediaServerResourcePtr SystemContextAware::currentServer() const
{
    return systemContext()->currentServer();
}

RemoteConnectionPtr SystemContextAware::connection() const
{
    return systemContext()->connection();
}

rest::ServerConnectionPtr SystemContextAware::connectedServerApi() const
{
    return systemContext()->connectedServerApi();
}

ServerTimeWatcher* SystemContextAware::serverTimeWatcher() const
{
    return systemContext()->serverTimeWatcher();
}

} // namespace nx::vms::client::desktop
