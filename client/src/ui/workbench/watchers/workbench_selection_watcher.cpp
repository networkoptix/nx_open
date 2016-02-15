#include "workbench_selection_watcher.h"

#include <ui/actions/action_manager.h>
#include <ui/actions/action_target_provider.h>

#include <utils/common/delayed.h>

namespace {

    /* Update selection no more often than once in 50 ms */
    const int selectionUpdateTimeoutMs = 50;
}


QnWorkbenchSelectionWatcher::QnWorkbenchSelectionWatcher(QObject *parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent)
    , m_scope(Qn::TreeScope | Qn::SceneScope)
    , m_selectionUpdatePending(false)
    , m_lastScope(Qn::InvalidScope)
{
    connect(action(QnActions::SelectionChangeAction),  &QAction::triggered,    this,   [this] {
        if (m_selectionUpdatePending)
            return;

        m_selectionUpdatePending = true;
        executeDelayed([this] {
            updateFromSelection();
        }, selectionUpdateTimeoutMs);
    });

}

QnWorkbenchSelectionWatcher::~QnWorkbenchSelectionWatcher() {
}


Qn::ActionScopes QnWorkbenchSelectionWatcher::scope() const {
    return m_scope;
}

void QnWorkbenchSelectionWatcher::setScope(Qn::ActionScopes value) {
    m_scope = value;
}

void QnWorkbenchSelectionWatcher::updateFromSelection() {
    if(!m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = false;

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;

    Qn::ActionScope currentScope = provider->currentScope();
    if (!m_scope.testFlag(currentScope))
        currentScope = m_lastScope;
    else
        m_lastScope = currentScope;

    if (currentScope == Qn::InvalidScope)
        return;

    auto resources = provider->currentParameters(currentScope).resources();
    emit selectionChanged(resources);
}
