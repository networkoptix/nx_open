#include "workbench_selection_watcher.h"

#include <QtWidgets/QAction>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_target_provider.h>

#include <utils/common/delayed.h>

namespace {

    /* Update selection no more often than once in 50 ms */
    const int selectionUpdateTimeoutMs = 50;
}

using namespace nx::client::desktop::ui::action;

QnWorkbenchSelectionWatcher::QnWorkbenchSelectionWatcher(QObject *parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scope(TreeScope | SceneScope),
    m_selectionUpdatePending(false),
    m_lastScope(InvalidScope)
{
    connect(action(QnActions::SelectionChangeAction), &QAction::triggered, this,
        [this]()
        {
            if (m_selectionUpdatePending)
                return;

            m_selectionUpdatePending = true;
            const auto callback = [this]() { updateFromSelection(); };
            executeDelayedParented(callback, selectionUpdateTimeoutMs, this);
        });

}

QnWorkbenchSelectionWatcher::~QnWorkbenchSelectionWatcher() {
}


nx::client::desktop::ui::action::ActionScopes QnWorkbenchSelectionWatcher::scope() const {
    return m_scope;
}

void QnWorkbenchSelectionWatcher::setScope(nx::client::desktop::ui::action::ActionScopes value) {
    m_scope = value;
}

void QnWorkbenchSelectionWatcher::updateFromSelection() {
    if(!m_selectionUpdatePending)
        return;

    m_selectionUpdatePending = false;

    auto provider = menu()->targetProvider();
    if(!provider)
        return;

    auto currentScope = provider->currentScope();
    if (!m_scope.testFlag(currentScope))
        currentScope = m_lastScope;
    else
        m_lastScope = currentScope;

    if (currentScope == InvalidScope)
        return;

    auto resources = provider->currentParameters(currentScope).resources();
    emit selectionChanged(resources);
}
