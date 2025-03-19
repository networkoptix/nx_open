// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_context_aware.h>

class QnAvailableCamerasWatcher;
class QnCameraThumbnailCache;
class QnResourceDiscoveryManager;

namespace nx::vms::client::mobile {

class SystemContext;
class WindowContext;

class SystemContextAware: public core::SystemContextAware
{
    using base_type = core::SystemContextAware;

public:
    using base_type::base_type;

    SystemContext* systemContext() const;

    WindowContext* windowContext() const;

    QnAvailableCamerasWatcher* availableCamerasWatcher() const;

    QnResourceDiscoveryManager* resourceDiscoveryManager() const;

    QnCameraThumbnailCache* cameraThumbnailCache() const;
};

} // namespace nx::vms::client::mobile
