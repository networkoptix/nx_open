#ifndef QN_WORKBENCH_CONTEXT_H
#define QN_WORKBENCH_CONTEXT_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <ui/actions/actions.h>

class QAction;

class QnResourcePoolUserWatcher;
class QnResourcePool;
class QnWorkbench;
class QnWorkbenchSynchronizer;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchLayoutVisibilityController;
class QnWorkbenchAccessController;
class QnActionManager;

class QnWorkbenchContext: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    QnResourcePool *resourcePool() const {
        return m_resourcePool;
    }

    QnWorkbench *workbench() const {
        return m_workbench;
    }

    QnWorkbenchSynchronizer *synchronizer() const {
        return m_synchronizer;
    }

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const {
        return m_snapshotManager;
    }

    QnActionManager *menu() const {
        return m_menu;
    }

    QAction *action(const Qn::ActionId id);

    QnUserResourcePtr user();

    static QnWorkbenchContext *instance(QnWorkbench *workbench);

signals:
    void userChanged(const QnUserResourcePtr &user);
    void aboutToBeDestroyed();

protected slots:
    void at_settings_lastUsedConnectionChanged();
    void at_resourcePool_aboutToBeDestroyed();

private:
    QnResourcePoolUserWatcher *m_userWatcher;
    QnResourcePool *m_resourcePool;
    QnWorkbenchSynchronizer *m_synchronizer;
    QnWorkbenchLayoutSnapshotManager *m_snapshotManager;
    QnWorkbenchLayoutVisibilityController *m_visibilityController;
    QnWorkbenchAccessController *m_accessController;
    QnActionManager *m_menu;
    QnWorkbench *m_workbench;
};


#endif // QN_WORKBENCH_CONTEXT_H
