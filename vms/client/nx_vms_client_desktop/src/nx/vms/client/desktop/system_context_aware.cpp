// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <nx/utils/log/assert.h>

#include "system_context.h"

namespace nx::vms::client::desktop {

SystemContextAware::SystemContextAware(SystemContext* systemContext):
    base_type(systemContext)
{
}

SystemContext* SystemContextAware::systemContext() const
{
    return dynamic_cast<SystemContext*>(nx::vms::common::SystemContextAware::systemContext());
}

QnWorkbenchAccessController* SystemContextAware::accessController() const
{
    return systemContext()->accessController();
}

} // namespace nx::vms::client::desktop
