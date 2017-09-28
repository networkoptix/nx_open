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

// TODO: This tag is better to be bound to QnWorkbenchStateManager.
static const nx::utils::log::Tag kWorkbenchStateTag(lit("__workbenchState"));

} // namespace

QnWorkbenchStateManager::QnWorkbenchStateManager(QObject *parent /* = NULL*/) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

}

bool QnWorkbenchStateManager::tryClose(bool force)
{
    if (!force && canSaveState())
    {
        saveState();
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

QnWorkbenchState QnWorkbenchStateManager::state() const
{
    QnWorkbenchState state;
    state.localSystemId = helpers::currentSystemLocalId(commonModule());
    if (const auto user = context()->user())
        state.userId = context()->user()->getId();
    workbench()->submit(state);
    for (const auto& d: m_delegates)
        d->submitState(&state);

    return state;
}

void QnWorkbenchStateManager::saveState()
{
    if (!canSaveState())
        return;

    const auto localSystemId = helpers::currentSystemLocalId(commonModule());
    const auto userId = context()->user()->getId();
    if (localSystemId.isNull() || userId.isNull())
    {
        NX_ASSERT(false, "Invalid connections state");
        return;
    }
    NX_DEBUG(kWorkbenchStateTag, "Saving workbench state...");
    NX_DEBUG(kWorkbenchStateTag, lm("System ID: %1").arg(localSystemId));
    NX_DEBUG(kWorkbenchStateTag, lm("User ID: %1").arg(userId));

    const auto state = this->state();
    NX_DEBUG(kWorkbenchStateTag, lm("Full state:\n%1").arg(QJson::serialized(state)));

    auto states = qnSettings->workbenchStates();
    states.erase(std::remove_if(states.begin(), states.end(),
        [localSystemId, userId](const QnWorkbenchState& state)
        {
            return state.localSystemId == localSystemId && state.userId == userId;
        }), states.end());

    states.prepend(state);
    if (states.size() > kSavedStatesLimit)
        states.erase(states.begin() + kSavedStatesLimit, states.end());

    qnSettings->setWorkbenchStates(states);
    qnSettings->save();
}

void QnWorkbenchStateManager::restoreState()
{
    const auto user = context()->user();
    NX_ASSERT(user, "Invalid connections state");
    if (!user)
        return;

    const auto localSystemId = helpers::currentSystemLocalId(commonModule());
    const auto userId = user->getId();

    NX_DEBUG(kWorkbenchStateTag, "Loading workbench state...");
    NX_DEBUG(kWorkbenchStateTag, lm("System ID: %1").arg(localSystemId));
    NX_DEBUG(kWorkbenchStateTag, lm("User ID: %1").arg(userId));

    auto states = qnSettings->workbenchStates();
    if (!states.empty())
    {
        NX_DEBUG(kWorkbenchStateTag, lm("Last saved state:\n%1")
            .arg(QJson::serialized(states.first())));
    }

    auto iter = std::find_if(states.cbegin(), states.cend(),
        [&userId, &localSystemId](const QnWorkbenchState& state)
        {
            return state.localSystemId == localSystemId && state.userId == userId;
        });

    if (iter != states.cend())
    {
        NX_DEBUG(kWorkbenchStateTag, lm("Found saved state:\n%1")
            .arg(QJson::serialized(*iter)));
        workbench()->update(*iter);
        for (const auto& d : m_delegates)
            d->loadState(*iter);
    }
    else
    {
        NX_DEBUG(kWorkbenchStateTag, "State was not found");
    }
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

QnSessionAwareDelegate::QnSessionAwareDelegate(QObject *parent /* = NULL*/) :
    QnWorkbenchContextAware(parent)
{
    context()->instance<QnWorkbenchStateManager>()->registerDelegate(this);
}

QnSessionAwareDelegate::~QnSessionAwareDelegate()
{
    context()->instance<QnWorkbenchStateManager>()->unregisterDelegate(this);
}

void QnSessionAwareDelegate::loadState(const QnWorkbenchState& state)
{

}

void QnSessionAwareDelegate::submitState(QnWorkbenchState* state)
{

}
