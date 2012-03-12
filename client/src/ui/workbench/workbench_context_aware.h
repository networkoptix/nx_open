#ifndef QN_WORKBENCH_CONTEXT_AWARE_H
#define QN_WORKBENCH_CONTEXT_AWARE_H

#include <ui/actions/actions.h>

class QAction;
class QObject;

class QnWorkbenchContext;
class QnActionManager;
class QnWorkbench;
class QnResourcePool;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;

class QnWorkbenchContextAware {
public:
    QnWorkbenchContextAware(QObject *parent);
    
    QnWorkbenchContextAware(QnWorkbenchContext *context);

    QnWorkbenchContext *context() const {
        return m_context;
    }

protected:
    QAction *action(const Qn::ActionId id) const;

    QnActionManager *menu() const;

    QnWorkbench *workbench() const;

    QnResourcePool *resourcePool() const;

    QnWorkbenchSynchronizer *synchronizer() const;

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const;

private:
    QnWorkbenchContext *m_context;
};


#endif // QN_WORKBENCH_CONTEXT_AWARE_H

