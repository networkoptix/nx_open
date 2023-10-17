// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_state_manager.h"

#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/window_context.h>
#include <statistics/statistics_manager.h>

namespace nx::vms::client::desktop {

SessionAwareDelegate::SessionAwareDelegate(WorkbenchStateManager* manager):
    m_manager(manager)
{
    m_manager->registerDelegate(this);
}

SessionAwareDelegate::~SessionAwareDelegate()
{
    m_manager->unregisterDelegate(this);
}

WorkbenchStateManager::WorkbenchStateManager(WindowContext* windowContext, QObject* parent):
    QObject(parent),
    WindowContextAware(windowContext)
{
}

bool WorkbenchStateManager::tryClose(bool force)
{
    if (!force)
    {
        // State is saved by Workbench::StateDelegate.
        if (auto statisticsManager = statisticsModule()->manager())
        {
            statisticsManager->saveCurrentStatistics();
            statisticsManager->resetStatistics();
        }
    }

    /* Order should be backward, so more recently opened dialogs will ask first. */
    for (int i = m_delegates.size() - 1; i >=0; --i)
    {
        if (!m_delegates[i]->tryClose(force))
            return false;
    }

    return true;
}

void WorkbenchStateManager::registerDelegate(SessionAwareDelegate* d)
{
    m_delegates << d;
}

void WorkbenchStateManager::unregisterDelegate(SessionAwareDelegate* d)
{
    m_delegates.removeOne(d);
}

} // namespace nx::vms::client::desktop
