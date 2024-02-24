// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/utils/save_state_manager.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class ShowreelStateManager:
    public core::SaveStateManager,
    public SystemContextAware
{
    Q_OBJECT
    using base_type = core::SaveStateManager;

public:
    ShowreelStateManager(SystemContext* systemContext, QObject* parent = nullptr);
};

} // namespace nx::vms::client::desktop
