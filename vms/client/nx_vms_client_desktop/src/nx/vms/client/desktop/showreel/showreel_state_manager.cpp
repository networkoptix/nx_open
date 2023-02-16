// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_state_manager.h"

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/showreel/showreel_manager.h>

namespace nx::vms::client::desktop {

ShowreelStateManager::ShowreelStateManager(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext)
{
    connect(systemContext->showreelManager(),
        &common::ShowreelManager::showreelRemoved,
        this,
        &core::SaveStateManager::clean);
}

} // namespace nx::vms::client::desktop
