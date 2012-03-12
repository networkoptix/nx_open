#include "workbench_access_controller.h"
#include <cassert>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_criterion.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(),      SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));

    at_context_userChanged(context()->user());
}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
    disconnect(context(), NULL, this, NULL);
    disconnect(resourcePool(), NULL, this, NULL);

    at_context_userChanged(QnUserResourcePtr());
}

bool QnWorkbenchAccessController::isAdmin() const {
    return m_user && m_user->isAdmin();
}

#if 0
bool QnWorkbenchAccessController::canRead(const QnUserResourcePtr &user) {
    if(isAdmin())
        return true;

    if(user == m_user)
        return true;

    return false;
}

bool QnWorkbenchAccessController::canWrite(const QnUserResourcePtr &user) {

}

bool QnWorkbenchAccessController::canSave(const QnUserResourcePtr &user) {
    if(isAdmin())
        return true;

    return m_user == user;
}

bool QnWorkbenchAccessController::canRead(const QnLayoutResourcePtr &layout) {
    if(isAdmin())
        return true;

    QnResourcePtr user = resourcePool()->getResourceById(layout->getParentId());
    return user == m_user;
}

bool QnWorkbenchAccessController::canWrite(const QnLayoutResourcePtr &layout) {
    if(isAdmin())
        return true;

    /* Non-admins can structurally modify local layouts only. */
    return context()->snapshotManager()->isLocal(layout);
}

bool QnWorkbenchAccessController::canSave(const QnLayoutResourcePtr &layout) {
    if(isAdmin())
        return true;

    return false; /* Non-admins cannot save layouts. */
}
#endif

void QnWorkbenchAccessController::updateVisibility(const QnUserResourcePtr &user) {
    user->setStatus(/*canRead(user)*/ user ? QnResource::Online : QnResource::Disabled);
}

void QnWorkbenchAccessController::updateVisibility(const QnUserResourceList &users) {
    foreach(const QnUserResourcePtr &user, users)
        updateVisibility(user);
}

void QnWorkbenchAccessController::at_context_userChanged(const QnUserResourcePtr &) {
    m_user = context()->user();

    updateVisibility(QnResourceCriterion::filter<QnUserResource>(resourcePool()->getResources()));
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    updateVisibility(user);
}

