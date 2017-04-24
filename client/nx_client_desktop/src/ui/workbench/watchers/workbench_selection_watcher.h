#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/ui/actions/action_types.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchSelectionWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchSelectionWatcher(QObject *parent = nullptr);
    virtual ~QnWorkbenchSelectionWatcher();

    nx::client::desktop::ui::action::ActionScopes scope() const;
    void setScope(nx::client::desktop::ui::action::ActionScopes value);

private:
    void updateFromSelection();

signals:
    void selectionChanged(const QnResourceList &resources);

private:
    nx::client::desktop::ui::action::ActionScopes m_scope;

    /** Whether the set of selected resources were changed. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    nx::client::desktop::ui::action::ActionScope m_lastScope;
};
