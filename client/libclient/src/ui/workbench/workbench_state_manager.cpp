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

namespace {
static const int kSavedStatesLimit = 20;
}

QnWorkbenchStateManager::QnWorkbenchStateManager(QObject *parent /* = NULL*/) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

}

bool QnWorkbenchStateManager::tryClose(bool force)
{
    /*
    *  We may get here in the disconnect process when all layouts are already closed.
    *  Marker of an invalid state - 'dummy' layout in the QnWorkbench class.
    *  We can detect it by `workbench()->currentLayoutIndex() == -1` check.
    */
    bool canSaveState =
        !qnCommon->remoteGUID().isNull()
        && qnRuntime->isDesktopMode()
        && context()->user()
        && !helpers::currentSystemIsNew()
        && workbench()->currentLayoutIndex() != -1;

    if (canSaveState)
    {
        saveState();
        qnStatisticsManager->saveCurrentStatistics();
    }

    for (auto d: m_delegates)
    {
        if (!d->tryClose(force))
            return false;
    }

    return true;
}

void QnWorkbenchStateManager::saveState()
{
    auto localId = helpers::currentSystemLocalId();
    auto userId = context()->user()->getId();
    if (localId.isNull() || userId.isNull())
    {
        NX_ASSERT(false, "Invalid connections state");
        return;
    }

    QnWorkbenchState state;
    state.localSystemId = localId;
    state.userId = userId;
    workbench()->submit(state);

    auto states = qnSettings->workbenchStates();
    auto iter = states.begin();
    while (iter != states.end())
    {
        if (iter->localSystemId == localId && iter->userId == userId)
            iter = states.erase(iter);
        else
            ++iter;
    }
    states.prepend(state);
    while (states.size() > kSavedStatesLimit)
        states.removeLast();

    qnSettings->setWorkbenchStates(states);
    qnSettings->save();
}

void QnWorkbenchStateManager::restoreState()
{
    auto localId = helpers::currentSystemLocalId();

    auto userId = context()->user()->getId();
    if (userId.isNull())
    {
        NX_ASSERT(false, "Invalid connections state");
        return;
    }

    auto states = qnSettings->workbenchStates();
    for (auto state : states)
    {
        if (state.localSystemId == localId
            && state.userId == userId)
        {
            workbench()->update(state);
            break;
        }
    }
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

QnSessionAwareDelegate::QnSessionAwareDelegate(QObject *parent /* = NULL*/) :
    QnWorkbenchContextAware(parent)
{
    context()->instance<QnWorkbenchStateManager>()->registerDelegate(this);
}

QnSessionAwareDelegate::~QnSessionAwareDelegate()
{
    context()->instance<QnWorkbenchStateManager>()->unregisterDelegate(this);
}
