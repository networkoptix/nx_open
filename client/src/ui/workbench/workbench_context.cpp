#include "workbench_context.h"
#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_pool_user_watcher.h>
#include <ui/actions/action_manager.h>
#include "settings.h"
#include "workbench.h"
#include "workbench_synchronizer.h"
#include "workbench_layout_snapshot_manager.h"
#include "workbench_access_controller.h"
#include "workbench_display.h"

QnWorkbenchContext::QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent):
    QObject(parent)
{
    if(resourcePool == NULL) {
        qnNullWarning(resourcePool);
        resourcePool = new QnResourcePool();
    }

    m_resourcePool = resourcePool;
    m_workbench.reset(new QnWorkbench(this));
    m_userWatcher.reset(new QnResourcePoolUserWatcher(resourcePool, this));
    
    connect(m_resourcePool,                 SIGNAL(aboutToBeDestroyed()),                   this,                                   SLOT(at_resourcePool_aboutToBeDestroyed()));
    connect(m_userWatcher.data(),           SIGNAL(userChanged(const QnUserResourcePtr &)), this,                                   SIGNAL(userChanged(const QnUserResourcePtr &)));

    /* Create dependent objects. */
    m_synchronizer.reset(new QnWorkbenchSynchronizer(this));
    m_snapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(this));
    m_accessController.reset(new QnWorkbenchAccessController(this));
    m_menu.reset(new QnActionManager(this));

    /* Create display. */
    m_display.reset(new QnWorkbenchDisplay(this));
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    /* Destruction order of these objects is important. */
    m_menu.reset();
    m_accessController.reset();
    m_snapshotManager.reset();
    m_synchronizer.reset();
    m_userWatcher.reset();
    m_workbench.reset();

    m_resourcePool = NULL;
}

QAction *QnWorkbenchContext::action(const Qn::ActionId id) const {
    return m_menu->action(id);
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    return m_userWatcher->user();
}

void QnWorkbenchContext::setUserName(const QString &userName) {
    m_userWatcher->setUserName(userName);
}

QnWorkbenchContext *QnWorkbenchContext::instance(QnWorkbench *workbench) {
    return dynamic_cast<QnWorkbenchContext *>(workbench->parent());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchContext::at_resourcePool_aboutToBeDestroyed() {
    delete this;
}

