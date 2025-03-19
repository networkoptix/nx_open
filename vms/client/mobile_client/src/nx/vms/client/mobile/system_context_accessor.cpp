// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_accessor.h"

#include <nx/vms/client/mobile/system_context.h>

namespace nx::vms::client::mobile {

SystemContext* SystemContextAccessor::mobileSystemContext() const
{
    return qobject_cast<SystemContext*>(systemContext());
}

QnCameraThumbnailCache* SystemContextAccessor::cameraThumbnailCache() const
{
    if (const auto context = mobileSystemContext())
        return context->cameraThumbnailCache();

    return nullptr;
}

} // nx::vms::client::mobile
