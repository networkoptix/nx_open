#ifndef QN_WORKBENCH_CONTEXT_H
#define QN_WORKBENCH_CONTEXT_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

class QAction;

class QnResourcePool;
class QnResourcePoolUserWatcher;
class QnWorkbench;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchAccessController;
class QnActionManager;

/**
 * This is a class that ties together all objects comprising the global state 
 * and works as an application context.
 */
class QnWorkbenchContext: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnResourcePool *resourcePool() const {
        return m_resourcePool;
    }

    QnWorkbench *workbench() const {
        return m_workbench.data();
    }

    QnWorkbenchSynchronizer *synchronizer() const {
        return m_synchronizer.data();
    }

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const {
        return m_snapshotManager.data();
    }

    QnActionManager *menu() const {
        return m_menu.data();
    }

    QnWorkbenchAccessController *accessController() const {
        return m_accessController.data();
    }

    QAction *action(const Qn::ActionId id) const;

    QnUserResourcePtr user() const;

    static QnWorkbenchContext *instance(QnWorkbench *workbench);

signals:
    void userChanged(const QnUserResourcePtr &user);
    void aboutToBeDestroyed();

protected slots:
    void at_settings_lastUsedConnectionChanged();
    void at_resourcePool_aboutToBeDestroyed();

private:
    /* Note that we're using scoped pointers here as destruction order of these objects is important. */

    QnResourcePool *m_resourcePool;
    QScopedPointer<QnWorkbench> m_workbench;
    QScopedPointer<QnResourcePoolUserWatcher> m_userWatcher;
    QScopedPointer<QnWorkbenchSynchronizer> m_synchronizer;
    QScopedPointer<QnWorkbenchLayoutSnapshotManager> m_snapshotManager;
    QScopedPointer<QnWorkbenchAccessController> m_accessController;
    QScopedPointer<QnActionManager> m_menu;
};


#endif // QN_WORKBENCH_CONTEXT_H
