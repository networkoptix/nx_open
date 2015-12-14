#include "workbench_access_controller.h"

#include <cassert>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_criterion.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>

#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

Qn::Permissions operator-(Qn::Permissions minuend, Qn::Permissions subrahend) {
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}

Qn::Permissions operator-(Qn::Permissions minuend, Qn::Permission subrahend) {
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}

Qn::Permissions operator-(Qn::Permission minuend, Qn::Permission subrahend) {
    return static_cast<Qn::Permissions>(minuend & ~subrahend);
}

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
    , m_user()
    , m_userPermissions(0)
    , m_readOnlyMode(false)

{
    connect(qnResPool,          &QnResourcePool::resourceAdded,                     this,   &QnWorkbenchAccessController::at_resourcePool_resourceAdded);
    connect(qnResPool,          &QnResourcePool::resourceRemoved,                   this,   &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnWorkbenchAccessController::updatePermissions);

    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);
    connect(qnCommon,           &QnCommonModule::readOnlyChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);

    recalculateAllPermissions();
}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr &resource) const {
    if (resource == m_user)
        if (qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
            return Qn::GlobalViewerPermissions;

    if (!m_dataByResource.contains(resource)) {
        Q_ASSERT(resource);
        if (resource && !resource->resourcePool())         /* Calculated permissions should always exist for all resources in the pool. */
            qDebug() << "Requesting permissions for the non-pool resource" << resource->getName();
        return calculatePermissions(resource);
    }

    return m_dataByResource.value(resource).permissions;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const {
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

Qn::Permissions QnWorkbenchAccessController::globalPermissions() const {  
    return globalPermissions(m_user);
}

Qn::Permissions QnWorkbenchAccessController::globalPermissions(const QnUserResourcePtr &user) const {
    if (qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
        return Qn::GlobalViewerPermissions;

    Qn::Permissions result(0);

    if(!user)
        return result;

    result = static_cast<Qn::Permissions>(user->getPermissions());

    if(user->isAdmin())
        result |= Qn::GlobalOwnerPermissions;

    return Qn::undeprecate(result);
}

bool QnWorkbenchAccessController::hasGlobalPermissions(Qn::Permissions requiredPermissions) const {
    if (!m_user)
        return false;
    return (globalPermissions(m_user) & requiredPermissions) == requiredPermissions;
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr &resource) const {
    Q_ASSERT(m_dataByResource.contains(resource));

    if(!m_dataByResource.contains(resource))
        return NULL;

    PermissionsData &data = m_dataByResource[resource];
    if(!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr &resource) const {
    Q_ASSERT(resource);

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

    if(QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return calculatePermissions(archive);

    if(QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>())
        return calculatePermissions(videoWall);

    Q_ASSERT("invalid resource type");
    return 0;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnUserResourcePtr &user) const {
    Q_ASSERT(user);

    Qn::Permissions result = 0;
    if (user == m_user) {
        result |= m_userPermissions; /* Add global permissions for current user. */

        if (m_readOnlyMode)
            return result | Qn::ReadPermission;

        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
        result |= Qn::CreateLayoutPermission; /* Everyone can create a layout for themselves */
    }

    if (m_userPermissions & Qn::GlobalEditLayoutsPermission) /* Layout-admin can create layouts. */
        result |= Qn::CreateLayoutPermission;

    if ((user != m_user) && (m_userPermissions & Qn::GlobalEditUsersPermission)) {
        result |= Qn::ReadPermission;
        if (m_readOnlyMode)
            return result;

        /* Protected users can only be edited by super-user. */
        if ((m_userPermissions & Qn::GlobalEditProtectedUserPermission) || !(globalPermissions(user) & Qn::GlobalProtectedPermission))
            result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::RemovePermission;
    }

    if (user->isLdap()) {
        result &= ~Qn::WriteNamePermission;
        result &= ~Qn::WritePasswordPermission;
        result &= ~Qn::WriteEmailPermission;

    }

    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnLayoutResourcePtr &layout) const {
    Q_ASSERT(layout);

    auto checkReadOnly = [this, layout](Qn::Permissions permissions) {
        if (!m_readOnlyMode)
            return permissions;
        return permissions - (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLoggedIn = [this, layout](Qn::Permissions permissions) {
        if (context()->user())
            return permissions;
        return permissions - (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions) {
        if (!layout->locked())
            return permissions;
        return permissions - (Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };

    QVariant permissions = layout->data().value(Qn::LayoutPermissionsRole);
    if(permissions.isValid() && permissions.canConvert<int>()) {
        return checkReadOnly(static_cast<Qn::Permissions>(permissions.toInt())); // TODO: #Elric listen to changes
    } 
    
    if (resourcePool()->isAutoGeneratedLayout(layout)) {
        const auto items = layout->getItems();
        bool hasDesktopCamera = std::any_of(items.cbegin(), items.cend(), [this](const QnLayoutItemData &item) {
            QnResourcePtr childResource = qnResPool->getResourceById(item.resource.id);
            return childResource && childResource->hasFlags(Qn::desktop_camera); 
        });

        /* Layouts with desktop cameras are not to be modified. */
        if (hasDesktopCamera)
            return checkReadOnly(Qn::ReadPermission | Qn::RemovePermission);
    }
    
    if (QnWorkbenchLayoutSnapshotManager::isFile(layout))
        return checkLocked(Qn::ReadWriteSavePermission | Qn::AddRemoveItemsPermission | Qn::EditLayoutSettingsPermission);
    
    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions {
        /* User can do everything with local layouts except removing from server. */
        if (snapshotManager()->isLocal(layout))
            return Qn::FullLayoutPermissions - Qn::RemovePermission;

        /* Admin can do whatever he wants. */
        if (m_userPermissions.testFlag(Qn::GlobalEditLayoutsPermission))
            return Qn::FullLayoutPermissions;

        /* Viewer cannot view other user's layouts. */
        if(m_user && layout->getParentId() != m_user->getId())
            return 0;

        /* User can do whatever he wants with own layouts. */
        if (layout->userCanEdit())
            return Qn::FullLayoutPermissions; 
        
        /* Can structurally modify but cannot save. */
        return Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission; 
    };

    return checkLocked(checkLoggedIn(checkReadOnly(base()))); 
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVirtualCameraResourcePtr &camera) const {
    Q_ASSERT(camera);

    Qn::Permissions result = Qn::ReadPermission;
    if(m_userPermissions & Qn::GlobalExportPermission)
        result |= Qn::ExportPermission;

    //TODO: #GDM SafeMode should PTZ work in safe mode?
    if(m_userPermissions & Qn::GlobalPtzControlPermission)
        result |= Qn::WritePtzPermission;

    if (m_readOnlyMode)
        return result;

    if(m_userPermissions & Qn::GlobalEditCamerasPermission)
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;    

    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnAbstractArchiveResourcePtr &media) const {
    Q_ASSERT(media);

    return Qn::ReadPermission | Qn::ExportPermission;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnMediaServerResourcePtr &server) const {
    Q_ASSERT(server);

    if (m_readOnlyMode)
        return Qn::ReadPermission;

    if(m_userPermissions & Qn::GlobalEditServersPermissions) 
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    return 0;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnVideoWallResourcePtr &videoWall) const {
    Q_ASSERT(videoWall);

    if (m_readOnlyMode)
        return Qn::ReadPermission;

    if(m_userPermissions & Qn::GlobalEditVideoWallPermission) 
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    return 0;
}


void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr &resource) {
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions) {
    if(m_dataByResource.contains(resource) &&
        permissions == this->permissions(resource))
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

void QnWorkbenchAccessController::recalculateAllPermissions() {
    m_user = context()->user();
    m_userPermissions = globalPermissions(m_user);
    m_readOnlyMode = qnCommon->isReadOnly();

    for(const QnResourcePtr &resource: qnResPool->getResources())
        updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    connect(resource, &QnResource::parentIdChanged,    this, &QnWorkbenchAccessController::updatePermissions);
    connect(resource, &QnResource::statusChanged,      this, &QnWorkbenchAccessController::updatePermissions);

    if (const QnLayoutResourcePtr &layout = resource.dynamicCast<QnLayoutResource>()) {
        connect(layout, &QnLayoutResource::userCanEditChanged,  this, &QnWorkbenchAccessController::updatePermissions);
        connect(layout, &QnLayoutResource::lockedChanged,       this, &QnWorkbenchAccessController::updatePermissions);
    }

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    disconnect(resource, NULL, this, NULL);

    setPermissionsInternal(resource, 0); /* So that the signal is emitted. */
    m_dataByResource.remove(resource);
}
