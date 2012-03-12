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
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    at_context_userChanged(context()->user());
}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
    disconnect(context(), NULL, this, NULL);
    disconnect(resourcePool(), NULL, this, NULL);

    at_context_userChanged(QnUserResourcePtr());
}

bool QnWorkbenchAccessController::isOwner() const {
    return m_user && m_user->isAdmin() && m_user->getName() == QLatin1String("admin");
}

bool QnWorkbenchAccessController::isAdmin() const {
    return m_user && m_user->isAdmin();
}

Qn::Permissions QnWorkbenchAccessController::calculateGlobalPermissions() {
    if(isOwner() || isAdmin())
        return Qn::CreateUserPermission;

    return 0;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr &resource) {
    if(!resource)
        return calculateGlobalPermissions();

    QnResource::Status status = resource->getStatus();
    if(status == QnResource::Disabled)
        return 0;

    if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        return calculatePermissions(user);

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissions(layout);

    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissions(camera);

    if(QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>())
        return calculatePermissions(server);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnUserResourcePtr &user) {
    if(user == m_user)
        return Qn::ReadWriteSavePermission | Qn::ReadPasswordPermission | Qn::WritePasswordPermission;

    if(isOwner()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::ReadPasswordPermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission;
    } else if(isAdmin()) {
        if(user->isAdmin()) {
            return Qn::ReadPermission;
        } else {
            return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::ReadPasswordPermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission;
        }
    } else {
        return 0;
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnLayoutResourcePtr &layout) {
    if(isAdmin()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;
    } else {
        QnResourcePtr user = resourcePool()->getResourceById(layout->getParentId());
        if(user != m_user) 
            return 0; /* Viewer can't view other's layouts. */

        if(snapshotManager()->isLocal(layout)) {
            return Qn::ReadPermission | Qn::WritePermission | Qn::RemovePermission; /* Can structurally modify local layouts only. */
        } else {
            return Qn::ReadPermission;
        }
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVirtualCameraResourcePtr &camera) {
    if(isAdmin()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;
    } else {
        return Qn::ReadPermission;
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVideoServerResourcePtr &server) {
    if(isAdmin()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;
    } else {
        return Qn::ReadPermission;
    }
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr &resource) {
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::updatePermissions(const QnResourceList &resources) {
    foreach(const QnResourcePtr &resource, resources)
        updatePermissions(resource);

}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions) {
    if(permissions == this->permissions(resource))
        return;

    m_permissionsByResource[resource] = permissions;
    emit permissionsChanged(resource);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchAccessController::at_context_userChanged(const QnUserResourcePtr &) {
    m_user = context()->user();

    updatePermissions(resourcePool()->getResources());
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    setPermissionsInternal(resource, 0); /* So that the signal is emitted. */
    m_permissionsByResource.remove(resource);
}

