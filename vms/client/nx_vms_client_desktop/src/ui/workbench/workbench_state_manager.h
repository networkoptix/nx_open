// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

class QWidget;

#include <nx/vms/client/desktop/current_system_context_aware.h>
#include <nx/vms/client/desktop/workbench/state/workbench_state_manager.h>

/** Delegate to maintain knowledge about current system context in a window context. */
class QnSessionAwareDelegate:
    public nx::vms::client::desktop::CurrentSystemContextAware,
    public nx::vms::client::desktop::SessionAwareDelegate

{
public:
    QnSessionAwareDelegate(QObject* parent);
    QnSessionAwareDelegate(nx::vms::client::desktop::WindowContext* windowContext);
};
