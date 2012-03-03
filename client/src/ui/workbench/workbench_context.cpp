#include "workbench_context.h"
#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_pool_user_watcher.h>
#include "settings.h"
#include "workbench.h"
#include "workbench_synchronizer.h"
#include "workbench_layout_snapshot_manager.h"
#include "workbench_layout_visibility_controller.h"

Q_GLOBAL_STATIC_WITH_ARGS(QnWorkbenchContext, qn_workbenchContext, (qnResPool));

QnWorkbenchContext::QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent):
    QObject(parent)
{
    if(resourcePool == NULL) {
        qnNullWarning(resourcePool);
        resourcePool = new QnResourcePool();
    }

    m_resourcePool = resourcePool;
    m_userWatcher = new QnResourcePoolUserWatcher(resourcePool, this);
    m_workbench = new QnWorkbench(this);
    
    connect(m_resourcePool,                 SIGNAL(aboutToBeDestroyed()),                   this,                                   SLOT(at_resourcePool_aboutToBeDestroyed()));
    connect(m_userWatcher,                  SIGNAL(userChanged(const QnUserResourcePtr &)), this,                                   SIGNAL(userChanged(const QnUserResourcePtr &)));
    connect(qnSettings,                     SIGNAL(lastUsedConnectionChanged()),            this,                                   SLOT(at_settings_lastUsedConnectionChanged()));

    /* Update state. */
    at_settings_lastUsedConnectionChanged();

    /* Create dependent objects. */
    m_synchronizer = new QnWorkbenchSynchronizer(this);
    m_synchronizer->setContext(this);

    m_snapshotManager = new QnWorkbenchLayoutSnapshotManager(this);
    m_snapshotManager->setContext(this);

    m_visibilityController = new QnWorkbenchLayoutVisibilityController(this);
    m_visibilityController->setContext(this);
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

QnWorkbenchContext *QnWorkbenchContext::instance(QnWorkbench *workbench) {
    return dynamic_cast<QnWorkbenchContext *>(workbench->parent());
}

QnWorkbenchContext *QnWorkbenchContext::instance() {
    return qn_workbenchContext();
}

QnUserResourcePtr QnWorkbenchContext::user() {
    return m_userWatcher->user();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchContext::at_settings_lastUsedConnectionChanged() {
    m_userWatcher->setUserName(qnSettings->lastUsedConnection().url.userName());
}

void QnWorkbenchContext::at_resourcePool_aboutToBeDestroyed() {
    m_resourcePool = NULL;
    delete this;
}

