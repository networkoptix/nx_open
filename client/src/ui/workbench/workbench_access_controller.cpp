#include "workbench_access_controller.h"

#include <cassert>

#include <utils/common/checked_cast.h>

#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource_managment/resource_criterion.h>

#include <plugins/resources/archive/abstract_archive_resource.h>

#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(context(),          SIGNAL(userChanged(const QnUserResourcePtr &)),     this,   SLOT(at_context_userChanged(const QnUserResourcePtr &)));
    connect(resourcePool(),     SIGNAL(resourceAdded(const QnResourcePtr &)),       this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(),     SIGNAL(resourceRemoved(const QnResourcePtr &)),     this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(snapshotManager(),  SIGNAL(flagsChanged(const QnLayoutResourcePtr &)),  this,   SLOT(at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &)));

    at_context_userChanged(context()->user());
}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
    disconnect(context(), NULL, this, NULL);
    disconnect(resourcePool(), NULL, this, NULL);

    at_context_userChanged(QnUserResourcePtr());
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr &resource) const {
    return m_dataByResource.value(resource).permissions;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const {
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

Qn::Permissions QnWorkbenchAccessController::globalPermissions() const {
    return permissions(m_user);
}

Qn::Permissions QnWorkbenchAccessController::globalPermissions(const QnUserResourcePtr &user) {
    Qn::Permissions result(0);

    if(!user)
        return result;

    result = static_cast<Qn::Permissions>(user->getPermissions());
    
    if(user->isAdmin())
        result |= Qn::GlobalOwnerPermissions;

    return Qn::undeprecate(result);
}

bool QnWorkbenchAccessController::hasGlobalPermissions(Qn::Permissions requiredPermissions) const {
    return hasPermissions(m_user, requiredPermissions);
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr &resource) const {
    if(!m_dataByResource.contains(resource))
        return NULL;

    const PermissionsData &data = m_dataByResource[resource];
    if(!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr &resource) {
    if(!resource)
        return 0;

    if(resource->isDisabled())
        return 0;

    if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        return calculatePermissions(user);

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissions(layout);

    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissions(camera);

    if(QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return calculatePermissions(archive);

    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return calculatePermissions(server);

    return 0;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnUserResourcePtr &user) {
    assert(user);

    Qn::Permissions result = 0;
    if (user == m_user) {
        result |= m_userPermissions; /* Add global permissions for current user. */
        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
    }

    if (m_userPermissions & Qn::GlobalEditLayoutsPermission) /* Layout-admin can create layouts. */
        result |= Qn::CreateLayoutPermission;

    if ((user != m_user) && (m_userPermissions & Qn::GlobalEditUsersPermission)) {
        result |= Qn::ReadPermission;
        
        /* Protected users can only be edited by super-user. */
        if ((m_userPermissions & Qn::GlobalEditProtectedUserPermission) || !(globalPermissions(user) & Qn::GlobalProtectedPermission))
            result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::RemovePermission;
    }
    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnLayoutResourcePtr &layout) {
    assert(layout);

    QVariant permissions = layout->data().value(Qn::LayoutPermissionsRole);
    if(permissions.isValid() && permissions.canConvert<int>()) {
        return static_cast<Qn::Permissions>(permissions.toInt()); // TODO: listen to changes
    } if(QnWorkbenchLayoutSnapshotManager::isFile(layout)) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::AddRemoveItemsPermission;
    } else if(m_userPermissions & Qn::GlobalEditLayoutsPermission) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission;
    } else {
        QnResourcePtr user = resourcePool()->getResourceById(layout->getParentId());
        if(user != m_user) 
            return 0; /* Viewer can't view other's layouts. */

        if(snapshotManager()->isLocal(layout) || layout->userCanEdit()) {
            return Qn::ReadPermission | Qn::WritePermission | Qn::WriteNamePermission | Qn::RemovePermission | Qn::AddRemoveItemsPermission; /* Can structurally modify local layouts only. */
        } else {
            return Qn::ReadPermission | Qn::WritePermission;
        }
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVirtualCameraResourcePtr &camera) {
    assert(camera);

    Qn::Permissions result = Qn::ReadPermission;
    if(m_userPermissions & Qn::GlobalEditCamerasPermission)
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    if(m_userPermissions & Qn::GlobalPtzControlPermission)
        result |= Qn::WritePtzPermission;
    if(m_userPermissions & Qn::GlobalExportPermission)
        result |= Qn::ExportPermission;
    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnAbstractArchiveResourcePtr &media) {
    assert(media);

    return Qn::ReadPermission | Qn::ExportPermission;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnMediaServerResourcePtr &server) {
    assert(server);

    if(m_userPermissions & Qn::GlobalEditServersPermissions) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    } else {
        return 0;
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

    PermissionsData &data = m_dataByResource[resource];
    data.permissions = permissions;
    
    emit permissionsChanged(resource);
    if(data.notifier)
        emit data.notifier->permissionsChanged(resource);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchAccessController::at_context_userChanged(const QnUserResourcePtr &) {
    m_user = context()->user();
    m_userPermissions = globalPermissions(m_user);

    updatePermissions(resourcePool()->getResources());
    updatePermissions(QnResourcePtr());
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    connect(resource.data(), SIGNAL(parentIdChanged(const QnResourcePtr &)),    this, SLOT(updatePermissions(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),      this, SLOT(updatePermissions(const QnResourcePtr &)));
    connect(resource.data(), SIGNAL(disabledChanged(const QnResourcePtr &)),    this, SLOT(updatePermissions(const QnResourcePtr &)));
    
    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    disconnect(resource.data(), NULL, this, NULL);

    setPermissionsInternal(resource, 0); /* So that the signal is emitted. */
    m_dataByResource.remove(resource);
}

void QnWorkbenchAccessController::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &layout) {
    updatePermissions(layout);
}

