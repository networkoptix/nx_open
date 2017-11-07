#include "workbench_access_controller.h"

#include <cassert>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/global_permissions_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>

QnWorkbenchPermissionsNotifier::QnWorkbenchPermissionsNotifier(QObject* parent) :
    QObject(parent)
{
}

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject* parent) :
    base_type(parent),
    m_user(),
    m_globalPermissions(Qn::NoGlobalPermissions)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchAccessController::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);

    connect(resourceAccessManager(), &QnResourceAccessManager::permissionsChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            Qn::Permissions /*permissions*/)
        {
            if (!m_user || subject.user() != m_user)
                return;

            updatePermissions(resource);
        });

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this](const QnResourceAccessSubject& subject, Qn::GlobalPermissions /*value*/)
        {
            if (!subject.user())
                return;

            if (subject.user() == m_user)
                recalculateAllPermissions();
            else
                updatePermissions(subject.user());
        });

    recalculateAllPermissions();
}

QnWorkbenchAccessController::~QnWorkbenchAccessController()
{
}

QnUserResourcePtr QnWorkbenchAccessController::user() const
{
    return m_user;
}

void QnWorkbenchAccessController::setUser(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;
    recalculateAllPermissions();
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr& resource) const
{
    if (!m_dataByResource.contains(resource))
        return calculatePermissions(resource);

    return m_dataByResource.value(resource).permissions;
}

Qn::Permissions QnWorkbenchAccessController::combinedPermissions(const QnResourceList& resources) const
{
    Qn::Permissions result = Qn::AllPermissions;
    for (const QnResourcePtr& resource : resources)
        result &= permissions(resource);
    return result;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const
{
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

Qn::GlobalPermissions QnWorkbenchAccessController::globalPermissions() const
{
    return m_globalPermissions;
}

bool QnWorkbenchAccessController::hasGlobalPermission(Qn::GlobalPermission requiredPermission) const
{
    return m_globalPermissions.testFlag(requiredPermission);
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr& resource) const
{
    NX_ASSERT(m_dataByResource.contains(resource));

    if (!m_dataByResource.contains(resource))
        return NULL;

    PermissionsData& data = m_dataByResource[resource];
    if (!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

bool QnWorkbenchAccessController::canCreateStorage(const QnUuid& serverId) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateStorage(m_user, serverId);
}

bool QnWorkbenchAccessController::canCreateLayout(const QnUuid& layoutParentId) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateLayout(m_user, layoutParentId);
}

bool QnWorkbenchAccessController::canCreateUser(Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateUser(m_user, targetPermissions, isOwner);
}

bool QnWorkbenchAccessController::canCreateVideoWall() const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateVideoWall(m_user);
}

bool QnWorkbenchAccessController::canCreateWebPage() const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateWebPage(m_user);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(
    const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
    {
        return Qn::ReadPermission
            | Qn::ViewContentPermission
            | Qn::ViewLivePermission
            | Qn::ViewFootagePermission
            | Qn::ExportPermission
            | Qn::WritePermission
            | Qn::WriteNamePermission;
    }

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(layout);

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallMediaPermissions;

    /* No other resources must be available while we are logged out. */
    if (!m_user)
        return Qn::NoPermissions;

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Check if we are creating new user */
        if (!user->resourcePool())
        {
            return hasGlobalPermission(Qn::GlobalAdminPermission)
                ? Qn::FullUserPermissions
                : Qn::NoPermissions;
        }
    }

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->licenseType() == Qn::LC_VMAX && !camera->isLicenseUsed())
        {
            const Qn::Permissions forbidden = Qn::ViewLivePermission | Qn::ViewFootagePermission;
            const auto basePermissions = resourceAccessManager()->permissions(m_user, resource);
            return basePermissions & ~forbidden;
        }
    }

    return resourceAccessManager()->permissions(m_user, resource);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(
    const QnLayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);

    // TODO: #GDM Code duplication with QnResourceAccessManager::calculatePermissionsInternal
    auto checkReadOnly =
        [this](Qn::Permissions permissions)
        {
            if (!commonModule()->isReadOnly())
                return permissions;

            return permissions &~ (Qn::RemovePermission
                | Qn::SavePermission
                | Qn::WriteNamePermission
                | Qn::EditLayoutSettingsPermission);
        };

    auto checkLoggedIn =
        [this](Qn::Permissions permissions)
        {
            if (m_user)
                return permissions;

            return permissions &~ (Qn::RemovePermission
                | Qn::SavePermission
                | Qn::WriteNamePermission
                | Qn::EditLayoutSettingsPermission);
        };

    auto checkLocked =
        [this, layout](Qn::Permissions permissions)
        {
            // Removable layouts must be checked via main pipeline
            NX_ASSERT(!permissions.testFlag(Qn::RemovePermission));
            if (!layout->locked())
                return permissions;

            if (layout->isFile()) //< Local files can be renamed even if locked.
                return permissions &~ Qn::AddRemoveItemsPermission;

            return permissions &~ (Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
        };

    // Some layouts are created with predefined permissions which are never changed.
    QVariant permissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (permissions.isValid() && permissions.canConvert<int>())
        return checkReadOnly(static_cast<Qn::Permissions>(permissions.toInt()) | Qn::ReadPermission);

    if (layout->isFile())
    {
        return checkLocked(Qn::ReadWriteSavePermission
            | Qn::AddRemoveItemsPermission
            | Qn::EditLayoutSettingsPermission
            | Qn::WriteNamePermission);
    }

    /* User can do everything with local layouts except removing from server. */
    if (layout->hasFlags(Qn::local))
    {
        return checkLocked(checkLoggedIn(checkReadOnly(
            static_cast<Qn::Permissions>(Qn::FullLayoutPermissions &~ Qn::RemovePermission))));
    }

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallLayoutPermissions;

    /*
     * No other resources must be available while we are logged out.
     * We may come here while receiving initial resources, when user is still not set.
     * In this case permissions will be recalculated on userChanged.
     */
    if (!m_user)
        return Qn::NoPermissions;

    return resourceAccessManager()->permissions(m_user, layout);
}

Qn::GlobalPermissions QnWorkbenchAccessController::calculateGlobalPermissions() const
{
    if (qnRuntime->isVideoWallMode())
        return Qn::GlobalVideoWallModePermissionSet;

    if (qnRuntime->isActiveXMode())
        return Qn::GlobalActiveXModePermissionSet;

    if (!m_user)
        return Qn::NoGlobalPermissions;

    return resourceAccessManager()->globalPermissions(m_user);
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr& resource)
{
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr& resource,
    Qn::Permissions permissions)
{
    if (m_dataByResource.contains(resource) &&
        permissions == this->permissions(resource))
        return;

    PermissionsData& data = m_dataByResource[resource];
    data.permissions = permissions;

    emit permissionsChanged(resource);
    if (data.notifier)
        emit data.notifier->permissionsChanged(resource);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchAccessController::recalculateAllPermissions()
{
    auto newGlobalPermissions = calculateGlobalPermissions();
    bool changed = newGlobalPermissions != m_globalPermissions;
    m_globalPermissions = newGlobalPermissions;

    if (changed)
        emit globalPermissionsChanged();

    for (const QnResourcePtr& resource: resourcePool()->getResources())
        updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    connect(resource, &QnResource::flagsChanged, this,
        &QnWorkbenchAccessController::updatePermissions);

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    disconnect(resource, NULL, this, NULL);
    m_dataByResource.remove(resource);
}

