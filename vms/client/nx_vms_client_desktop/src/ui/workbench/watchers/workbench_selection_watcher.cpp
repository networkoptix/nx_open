// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_selection_watcher.h"

#include <QtGui/QAction>

#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_target_provider.h>
#include <utils/common/delayed.h>

using namespace nx::vms::client::desktop;

namespace {

    /* Update selection no more often than once in 50 ms */
    const int selectionUpdateTimeoutMs = 50;
}

QnWorkbenchSelectionWatcher::QnWorkbenchSelectionWatcher(QObject *parent /*= nullptr*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_scope(menu::TreeScope | menu::SceneScope),
    m_selectionUpdatePending(false),
    m_lastScope(menu::InvalidScope)
{
    connect(action(menu::SelectionChangeAction), &QAction::triggered, this,
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


menu::ActionScopes QnWorkbenchSelectionWatcher::scope() const {
    return m_scope;
}

void QnWorkbenchSelectionWatcher::setScope(menu::ActionScopes value) {
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

    if (currentScope == menu::InvalidScope)
        return;

    auto resources = provider->currentParameters(currentScope).resources();
    emit selectionChanged(resources);
}
