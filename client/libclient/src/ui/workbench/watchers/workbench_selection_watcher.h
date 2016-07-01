#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchSelectionWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchSelectionWatcher(QObject *parent = nullptr);
    virtual ~QnWorkbenchSelectionWatcher();

    Qn::ActionScopes scope() const;
    void setScope(Qn::ActionScopes value);

private:
    void updateFromSelection();

signals:
    void selectionChanged(const QnResourceList &resources);

private:
    Qn::ActionScopes m_scope;

    /** Whether the set of selected resources were changed. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    Qn::ActionScope m_lastScope;
};
