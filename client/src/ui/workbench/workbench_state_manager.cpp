#include "workbench_state_manager.h"

#include <common/common_module.h>

#include <client/client_settings.h>

#include <core/resource/user_resource.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

QnWorkbenchStateManager::QnWorkbenchStateManager(QObject *parent /*= NULL*/):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{

}

bool QnWorkbenchStateManager::tryClose(bool force) {

    bool canSaveState = 
           !qnCommon->remoteGUID().isNull()
        && !qnSettings->isVideoWallMode()
        &&  context()->user();

    QnWorkbenchState state;
    if(canSaveState)
        workbench()->submit(state);

    foreach (QnWorkbenchStateDelegate *d, m_delegates)
        if (!d->tryClose(force))
            return false;

    /* Server can be stopped while tryClose(false) was in progress because it produces confirmation dialogs. */
    /* currentLayoutIndex == -1 means we're in the end of disconnection process and all layout resource has already been deleted. Nothing to save now. */
    if (canSaveState && context()->user() && state.currentLayoutIndex != -1) {
        QnWorkbenchStateHash states = qnSettings->userWorkbenchStates();
        states[context()->user()->getName()] = state;
        qnSettings->setUserWorkbenchStates(states);
    }
    return true;
}

void QnWorkbenchStateManager::registerDelegate(QnWorkbenchStateDelegate *d) {
    m_delegates << d;
}

void QnWorkbenchStateManager::unregisterDelegate(QnWorkbenchStateDelegate *d) {
    m_delegates.removeOne(d);
}

void QnWorkbenchStateManager::forcedUpdate() {
    for (QnWorkbenchStateDelegate *d: m_delegates)
        d->forcedUpdate();
}


QnWorkbenchStateDelegate::QnWorkbenchStateDelegate(QObject *parent /*= NULL*/):
    QnWorkbenchContextAware(parent)
{
    context()->instance<QnWorkbenchStateManager>()->registerDelegate(this);
}

QnWorkbenchStateDelegate::~QnWorkbenchStateDelegate() {
    context()->instance<QnWorkbenchStateManager>()->unregisterDelegate(this);
}
