#include "workbench_access_controller.h"
#include <cassert>
#include <utils/common/checked_cast.h>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <plugins/resources/archive/abstract_archive_resource.h>
#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

namespace {
    bool isFileLayout(const QnLayoutResourcePtr &layout) {
        return (layout->flags() & QnResource::url) && !layout->getUrl().isEmpty();
    }

} // anonymous namespace


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

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr &resource) const {
    if(!m_dataByResource.contains(resource))
        return NULL;

    const PermissionsData &data = m_dataByResource[resource];
    if(!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

bool QnWorkbenchAccessController::isOwner() const {
    return m_user && m_user->isAdmin() && m_user->getName() == QLatin1String("admin");
}

bool QnWorkbenchAccessController::isAdmin() const {
    return m_user && m_user->isAdmin() && m_user->getName() != QLatin1String("admin");
}

bool QnWorkbenchAccessController::isViewer() const {
    return !m_user || !m_user->isAdmin();
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

    if(QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>())
        return calculatePermissions(server);

    return 0;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnUserResourcePtr &user) {
    if(isOwner()) {
        if(user == m_user) {
            return Qn::ReadWriteSavePermission | Qn::WritePasswordPermission | Qn::CreateLayoutPermission | Qn::CreateUserPermission;
        } else {
            return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteLoginPermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::CreateLayoutPermission;
        }
    } else if(isAdmin()) {
        if(user == m_user) {
            return Qn::ReadWriteSavePermission | Qn::WritePasswordPermission | Qn::CreateLayoutPermission | Qn::CreateUserPermission;
        } else if(user->isAdmin()) {
            return Qn::ReadPermission | Qn::CreateLayoutPermission;
        } else {
            return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteLoginPermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::CreateLayoutPermission;
        }
    } else {
        if(user == m_user) {
            return Qn::ReadWriteSavePermission | Qn::WritePasswordPermission;
        } else {
            return 0;
        }
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnLayoutResourcePtr &layout) {
    if(isFileLayout(layout)) {
        return Qn::ReadPermission;
    } else if(isAdmin() || isOwner()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::AddRemoveItemsPermission;
    } else {
        QnResourcePtr user = resourcePool()->getResourceById(layout->getParentId());
        if(user != m_user) 
            return 0; /* Viewer can't view other's layouts. */

        if(snapshotManager()->isLocal(layout)) {
            return Qn::ReadPermission | Qn::WritePermission | Qn::RemovePermission | Qn::AddRemoveItemsPermission; /* Can structurally modify local layouts only. */
        } else {
            return Qn::ReadPermission | Qn::WritePermission;
        }
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVirtualCameraResourcePtr &) {
    if(isAdmin() || isOwner()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;
    } else {
        return Qn::ReadPermission;
    }
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnAbstractArchiveResourcePtr &) {
    return Qn::ReadPermission;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVideoServerResourcePtr &) {
    if(isAdmin() || isOwner()) {
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;
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

void QnWorkbenchAccessController::updateSenderPermissions() {
    QObject *sender = this->sender();
    if(!sender)
        return; /* Already disconnected from this sender. */

    updatePermissions(toSharedPointer(checked_cast<QnResource *>(sender)));
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

    updatePermissions(resourcePool()->getResources());
    updatePermissions(QnResourcePtr());
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    connect(resource.data(), SIGNAL(parentIdChanged()), this, SLOT(updateSenderPermissions()));
    connect(resource.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(updateSenderPermissions()));
	connect(resource.data(), SIGNAL(disabledChanged(bool, bool)), this, SLOT(updateSenderPermissions()));
    
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

