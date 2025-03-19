// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/system_context.h>

namespace nx::vms::client::mobile {

SystemContext* SystemContextAware::systemContext() const
{
    return base_type::systemContext()->as<SystemContext>();
}

WindowContext* SystemContextAware::windowContext() const
{
    return systemContext()->windowContext();
}

QnAvailableCamerasWatcher* SystemContextAware::availableCamerasWatcher() const
{
    return systemContext()->availableCamerasWatcher();
}

QnResourceDiscoveryManager* SystemContextAware::resourceDiscoveryManager() const
{
    return systemContext()->resourceDiscoveryManager();
}

QnCameraThumbnailCache* SystemContextAware::cameraThumbnailCache() const
{
    return systemContext()->cameraThumbnailCache();
}

} // namespace nx::vms::client::mobile
