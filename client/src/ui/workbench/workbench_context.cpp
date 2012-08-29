#include "workbench_context.h"

#include <utils/common/warnings.h>
#include <utils/settings.h>

#include <api/video_server_statistics_manager.h>

#include <core/resourcemanagment/resource_pool.h>

#include <ui/actions/action_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_synchronizer.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

QnWorkbenchContext::QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent):
    QObject(parent)
{
    if(resourcePool == NULL) {
        qnNullWarning(resourcePool);
        resourcePool = new QnResourcePool();
    }

    m_resourcePool = resourcePool;
    m_workbench.reset(new QnWorkbench(this));
    
    m_userWatcher = instance<QnWorkbenchUserWatcher>();
    connect(m_resourcePool, SIGNAL(aboutToBeDestroyed()),                   this,   SLOT(at_resourcePool_aboutToBeDestroyed()));
    connect(m_userWatcher,    SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SIGNAL(userChanged(const QnUserResourcePtr &)));

    /* Create dependent objects. */
    m_synchronizer.reset(new QnWorkbenchSynchronizer(this));
    m_snapshotManager.reset(new QnWorkbenchLayoutSnapshotManager(this));
    m_accessController.reset(new QnWorkbenchAccessController(this));
    m_menu.reset(new QnActionManager(this));
    m_display.reset(new QnWorkbenchDisplay(this));
    m_navigator.reset(new QnWorkbenchNavigator(this));
}

QnWorkbenchContext::~QnWorkbenchContext() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    qDeleteAll(m_instanceByTypeName);
    m_instanceByTypeName.clear();
    m_userWatcher = NULL;

    /* Destruction order of these objects is important. */
    m_navigator.reset();
    m_display.reset();
    m_menu.reset();
    m_accessController.reset();
    m_snapshotManager.reset();
    m_synchronizer.reset();
    m_workbench.reset();

    m_resourcePool = NULL;
}

QAction *QnWorkbenchContext::action(const Qn::ActionId id) const {
    return m_menu->action(id);
}

QnUserResourcePtr QnWorkbenchContext::user() const {
    if(m_userWatcher) {
        return m_userWatcher->user();
    } else {
        return QnUserResourcePtr();
    }
}

void QnWorkbenchContext::setUserName(const QString &userName) {
    if(m_userWatcher)
        m_userWatcher->setUserName(userName);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchContext::at_resourcePool_aboutToBeDestroyed() {
    delete this;
}

