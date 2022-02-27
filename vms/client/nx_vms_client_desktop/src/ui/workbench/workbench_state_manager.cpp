// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_state_manager.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource/user_resource.h>

#include <statistics/statistics_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <network/system_helpers.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>

namespace {

static const int kSavedStatesLimit = 20;

} // namespace

QnWorkbenchStateManager::QnWorkbenchStateManager(QObject *parent /* = nullptr*/) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

}

bool QnWorkbenchStateManager::tryClose(bool force)
{
    if (!force && canSaveState())
    {
        // State is saved by QnWorkbench::StateDelegate.
        if (auto statisticsManager = commonModule()->instance<QnStatisticsManager>())
            statisticsManager->saveCurrentStatistics();
    }

    /* Order should be backward, so more recently opened dialogs will ask first. */
    for (int i = m_delegates.size() - 1; i >=0; --i)
    {
        if (!m_delegates[i]->tryClose(force))
            return false;
    }

    return true;
}

bool QnWorkbenchStateManager::canSaveState() const
{
    /*
     *  We may get here in the disconnect process when all layouts are already closed.
     *  Marker of an invalid state - 'dummy' layout in the QnWorkbench class.
     *  We can detect it by `workbench()->currentLayoutIndex() == -1` check.
     */
    return !commonModule()->remoteGUID().isNull()
        && qnRuntime->isDesktopMode()
        && context()->user()
        && !helpers::currentSystemIsNew(commonModule())
        && workbench()->currentLayoutIndex() != -1;
}

void QnWorkbenchStateManager::registerDelegate(QnSessionAwareDelegate* d)
{
    m_delegates << d;
}

void QnWorkbenchStateManager::unregisterDelegate(QnSessionAwareDelegate* d)
{
    m_delegates.removeOne(d);
}

void QnWorkbenchStateManager::forcedUpdate()
{
    for (QnSessionAwareDelegate *d : m_delegates)
        d->forcedUpdate();
}

QnSessionAwareDelegate::QnSessionAwareDelegate(
    QObject* parent,
    InitializationMode initMode)
    :
    QnWorkbenchContextAware(parent, initMode)
{
    context()->instance<QnWorkbenchStateManager>()->registerDelegate(this);
}

QnSessionAwareDelegate::~QnSessionAwareDelegate()
{
    context()->instance<QnWorkbenchStateManager>()->unregisterDelegate(this);
}
