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
class QnWorkbenchNavigator;
class QnWorkbenchCustomizer;

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
    QnWorkbenchContextAware(QObject *parent, bool lazyInitialization = false);

    QnWorkbenchContextAware(QnWorkbenchContext *context);

    QnWorkbenchContextAware(QObject *parent, QnWorkbenchContext *context);

    /**
     * Virtual destructor.
     * 
     * We do <tt>dynamic_cast</tt>s to <tt>QnWorkbenchContextAware</tt>, so this
     * class better have a vtable.
     */
    virtual ~QnWorkbenchContextAware() {}

    /**
     * \returns                         Context associated with this context-aware object.
     */
    QnWorkbenchContext *context() const {
        return m_context;
    }

    /** Delayed initialization call. */
    void initializeContext(QObject *parent);

    /** Delayed initialization call. */
    void initializeContext(QnWorkbenchContext *context);

protected:
    virtual void afterContextInitialized();

    QAction *action(const Qn::ActionId id) const;

    QnActionManager *menu() const;

    QnWorkbench *workbench() const;

    QnResourcePool *resourcePool() const;

    QnWorkbenchSynchronizer *synchronizer() const;

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const;

    QnWorkbenchAccessController *accessController() const;

    QnWorkbenchDisplay *display() const;

    QnWorkbenchNavigator *navigator() const;

    QWidget *mainWindow() const;

private:
    void init(QObject *parent, bool lazyInitialization = false);
    void init(QnWorkbenchContext *context);

private:
    QnWorkbenchContext *m_context;
    bool m_initialized;
};


#endif // QN_WORKBENCH_CONTEXT_AWARE_H

