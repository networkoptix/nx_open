// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <client/client_message_processor.h>
#include <nx/utils/log/assert.h>

#include "system_context.h"

namespace nx::vms::client::core {

SystemContext* SystemContextAware::systemContext() const
{
    return dynamic_cast<SystemContext*>(nx::vms::common::SystemContextAware::systemContext());
}

AccessController* SystemContextAware::accessController() const
{
    return systemContext()->accessController();
}

QnClientMessageProcessor* SystemContextAware::clientMessageProcessor() const
{
    return systemContext()->clientMessageProcessor();
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

UserWatcher* SystemContextAware::userWatcher() const
{
    return systemContext()->userWatcher();
}

ServerTimeWatcher* SystemContextAware::serverTimeWatcher() const
{
    return systemContext()->serverTimeWatcher();
}

QnCameraBookmarksManager* SystemContextAware::cameraBookmarksManager() const
{
    return systemContext()->cameraBookmarksManager();
}

} // namespace nx::vms::client::desktop
