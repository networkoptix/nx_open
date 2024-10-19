// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "current_system_context_aware.h"

#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/application_context.h>

namespace nx::vms::client::mobile {

CurrentSystemContextAware::CurrentSystemContextAware():
    core::SystemContextAware(core::appContext()->systemContexts().front())
{
}

} // namespace nx::vms::client::mobile
