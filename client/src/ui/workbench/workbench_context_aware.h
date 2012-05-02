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
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;

/**
 * This class simplifies access to workbench context. 
 * 
 * If some class needs access to workbench context, 
 * it can simply derive from <tt>QnWorkbenchContextAware</tt> and
 * pass its context-aware parent into constructor.
 */
class QnWorkbenchContextAware {
public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent for this object. Must itself be
     *                                  a child of some context-aware object,
     *                                  or of a context. Must not be NULL.
     */
    QnWorkbenchContextAware(QObject *parent);

    QnWorkbenchContextAware(QnWorkbenchContext *context);

    /**
     * \returns                         Context associated with this context-aware object.
     */
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

    QnWorkbenchAccessController *accessController() const;

    QnWorkbenchDisplay *display() const;

private:
    QnWorkbenchContext *m_context;
};


#endif // QN_WORKBENCH_CONTEXT_AWARE_H

