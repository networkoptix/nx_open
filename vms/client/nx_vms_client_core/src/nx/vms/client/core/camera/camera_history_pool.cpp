// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_history_pool.h"

#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

CameraHistoryPool::CameraHistoryPool(SystemContext* systemContext):
    QnCameraHistoryPool(systemContext)
{
}

rest::ServerConnectionPtr CameraHistoryPool::connectedServerApi() const
{
    return systemContext()->as<SystemContext>()->connectedServerApi();
}

} // namespace nx::vms::client::core
