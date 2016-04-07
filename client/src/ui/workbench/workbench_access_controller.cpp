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
#include <core/resource/webpage_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_criterion.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>

#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"



QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
    , m_user()
    , m_userPermissions(Qn::NoGlobalPermissions)
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

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr &resource) const
{
    if (!m_dataByResource.contains(resource))
    {
        NX_ASSERT(resource);
        if (resource && !resource->resourcePool())         /* Calculated permissions should always exist for all resources in the pool. */
            qDebug() << "Requesting permissions for the non-pool resource" << resource->getName();
        return calculatePermissions(resource);
    }

    return m_dataByResource.value(resource).permissions;
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourceList &resources) const
{
    Qn::Permissions result = Qn::AllPermissions;
    for (const QnResourcePtr &resource : resources)
        result &= permissions(resource);
    return result;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const {
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

Qn::GlobalPermissions QnWorkbenchAccessController::globalPermissions() const
{
    return globalPermissions(m_user);
}

Qn::GlobalPermissions QnWorkbenchAccessController::globalPermissions(const QnUserResourcePtr &user) const
{
    if (qnRuntime->isVideoWallMode() || qnRuntime->isActiveXMode())
        return Qn::GlobalViewerPermissions;

    return qnResourceAccessManager->globalPermissions(user);
}

bool QnWorkbenchAccessController::hasGlobalPermission(Qn::GlobalPermission requiredPermission) const
{
    return globalPermissions().testFlag(requiredPermission);
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr &resource) const
{
    NX_ASSERT(m_dataByResource.contains(resource));

    if(!m_dataByResource.contains(resource))
        return NULL;

    PermissionsData &data = m_dataByResource[resource];
    if(!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr &resource) const {
    NX_ASSERT(resource);

    if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        return calculatePermissionsInternal(user);

    if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(layout);

    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissionsInternal(camera);

    if(QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return calculatePermissionsInternal(archive);

    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        return calculatePermissionsInternal(server);

    if(QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return calculatePermissionsInternal(archive);

    if(QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>())
        return calculatePermissionsInternal(videoWall);

    if(QnWebPageResourcePtr webPage = resource.dynamicCast<QnWebPageResource>())
        return calculatePermissionsInternal(webPage);

    NX_ASSERT("invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnUserResourcePtr &user) const {
    NX_ASSERT(user);

    Qn::Permissions result = Qn::NoPermissions;
    if (user == m_user)
    {
        if (m_readOnlyMode)
            return result | Qn::ReadPermission;

        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission; /* Everyone can edit own data. */
        result |= Qn::CreateLayoutPermission; /* Everyone can create a layout for themselves */
    }

    if (m_userPermissions.testFlag(Qn::GlobalEditLayoutsPermission)) /* Layout-admin can create layouts. */
        result |= Qn::CreateLayoutPermission;

    if ((user != m_user) && m_userPermissions.testFlag(Qn::GlobalEditUsersPermission))
    {
        result |= Qn::ReadPermission;
        if (m_readOnlyMode)
            return result;

        /* Protected users can only be edited by super-user. */
        if (m_userPermissions.testFlag(Qn::GlobalOwnerPermission) || !globalPermissions(user).testFlag(Qn::GlobalAdminPermission))
            result |= Qn::ReadWriteSavePermission | Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteAccessRightsPermission | Qn::RemovePermission;
    }

    if (user->isLdap())
    {
        result &= ~Qn::WriteNamePermission;
        result &= ~Qn::WritePasswordPermission;
        result &= ~Qn::WriteEmailPermission;

    }

    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnLayoutResourcePtr &layout) const {
    NX_ASSERT(layout);

    auto checkReadOnly = [this, layout](Qn::Permissions permissions) {
        if (!m_readOnlyMode)
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLoggedIn = [this, layout](Qn::Permissions permissions) {
        if (context()->user())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions) {
        if (!layout->locked())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
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
            return Qn::NoPermissions;

        /* User can do whatever he wants with own layouts. */
        if (layout->userCanEdit())
            return Qn::FullLayoutPermissions;

        /* Can structurally modify but cannot save. */
        return Qn::ReadPermission | Qn::WritePermission | Qn::AddRemoveItemsPermission;
    };

    return checkLocked(checkLoggedIn(checkReadOnly(base())));
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnVirtualCameraResourcePtr &camera) const {
    NX_ASSERT(camera);

    Qn::Permissions result = Qn::ReadPermission;
    if (m_userPermissions.testFlag(Qn::GlobalExportPermission))
        result |= Qn::ExportPermission;

    if (m_userPermissions.testFlag(Qn::GlobalPtzControlPermission))
        result |= Qn::WritePtzPermission;

    if (m_readOnlyMode)
        return result;

    if (m_userPermissions.testFlag(Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnAbstractArchiveResourcePtr &media) const {
    NX_ASSERT(media);

    return Qn::ReadPermission | Qn::ExportPermission;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnMediaServerResourcePtr &server) const {
    NX_ASSERT(server);

    if (m_readOnlyMode)
        return Qn::ReadPermission;

    if (m_userPermissions.testFlag(Qn::GlobalEditServersPermissions))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    return Qn::NoPermissions;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnVideoWallResourcePtr &videoWall) const {
    NX_ASSERT(videoWall);

    if (m_readOnlyMode)
        return Qn::ReadPermission;

    if (m_userPermissions.testFlag(Qn::GlobalEditVideoWallPermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnWebPageResourcePtr &webPage) const {
    NX_ASSERT(webPage);

    if (m_readOnlyMode)
        return Qn::ReadPermission;

    /* Web Page behaves totally like camera. */
    if (m_userPermissions.testFlag(Qn::GlobalEditCamerasPermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return Qn::NoPermissions;
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

void QnWorkbenchAccessController::recalculateAllPermissions()
{
    m_user = context()->user();
    m_userPermissions = globalPermissions();
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

    setPermissionsInternal(resource, Qn::NoPermissions); /* So that the signal is emitted. */
    m_dataByResource.remove(resource);
}
