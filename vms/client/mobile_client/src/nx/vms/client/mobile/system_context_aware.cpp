// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_context_aware.h"

#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/system_context.h>

namespace nx::vms::client::mobile {

SystemContext* SystemContextAware::systemContext() const
{
    return base_type::systemContext()->as<SystemContext>();
}

SessionManager* SystemContextAware::sessionManager() const
{
    return systemContext()->sessionManager();
}

} // namespace nx::vms::client::mobile
