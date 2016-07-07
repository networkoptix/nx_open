#include "workbench_access_controller.h"

#include <cassert>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>

#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"

QnWorkbenchPermissionsNotifier::QnWorkbenchPermissionsNotifier(QObject* parent) :
    QObject(parent)
{
}

QnWorkbenchAccessController::QnWorkbenchAccessController(QObject* parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_user(),
    m_globalPermissions(Qn::NoGlobalPermissions),
    m_readOnlyMode(false)
{
    connect(qnResPool,          &QnResourcePool::resourceAdded,                     this,   &QnWorkbenchAccessController::at_resourcePool_resourceAdded);
    connect(qnResPool,          &QnResourcePool::resourceRemoved,                   this,   &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnWorkbenchAccessController::updatePermissions);

    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);
    connect(qnCommon,           &QnCommonModule::readOnlyChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);
    connect(qnResourceAccessManager, &QnResourceAccessManager::accessibleResourcesChanged, this, &QnWorkbenchAccessController::at_accessibleResourcesChanged);

    recalculateAllPermissions();
}

QnWorkbenchAccessController::~QnWorkbenchAccessController()
{
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
    return qnResourceAccessManager->canCreateStorage(m_user, serverId);
}

bool QnWorkbenchAccessController::canCreateLayout(const QnUuid& layoutParentId) const
{
    if (!m_user)
        return false;
    return qnResourceAccessManager->canCreateLayout(m_user, layoutParentId);
}

bool QnWorkbenchAccessController::canCreateUser(Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!m_user)
        return false;
    return qnResourceAccessManager->canCreateUser(m_user, targetPermissions, isOwner);
}

bool QnWorkbenchAccessController::canCreateVideoWall() const
{
    if (!m_user)
        return false;
    return qnResourceAccessManager->canCreateVideoWall(m_user);
}

bool QnWorkbenchAccessController::canCreateWebPage() const
{
    if (!m_user)
        return false;
    return qnResourceAccessManager->canCreateWebPage(m_user);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return Qn::ReadPermission | Qn::ExportPermission;

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(layout);

    /* No other resources must be available while we are logged out. */
    if (!m_user)
        return Qn::NoPermissions;

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Check if we are creating new user */
        if (user->flags().testFlag(Qn::local))
            return hasGlobalPermission(Qn::GlobalAdminPermission) ? Qn::FullUserPermissions : Qn::NoPermissions;
    }

    return qnResourceAccessManager->permissions(m_user, resource);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnLayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);

    //TODO: #GDM Code duplication with QnResourceAccessManager::calculatePermissionsInternal
    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!m_readOnlyMode)
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLoggedIn = [this](Qn::Permissions permissions)
    {
        if (m_user)
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions)
    {
        if (!layout->locked())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };

    if (layout->isFile())
        return checkLocked(Qn::ReadWriteSavePermission | Qn::AddRemoveItemsPermission | Qn::EditLayoutSettingsPermission);

    /* User can do everything with local layouts except removing from server. */
    if (snapshotManager()->isLocal(layout))
        return checkLocked(checkLoggedIn(checkReadOnly(static_cast<Qn::Permissions>(Qn::FullLayoutPermissions &~Qn::RemovePermission))));

    /*
     * No other resources must be available while we are logged out.
     * We may come here while receiving initial resources, when user is still not set.
     * In this case permissions will be recalculated on userChanged.
     */
    if (!m_user)
        return Qn::NoPermissions;

    QVariant permissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (permissions.isValid() && permissions.canConvert<int>())
        return checkReadOnly(static_cast<Qn::Permissions>(permissions.toInt())); // TODO: #Elric listen to changes

    return qnResourceAccessManager->permissions(m_user, layout);
}

Qn::GlobalPermissions QnWorkbenchAccessController::calculateGlobalPermissions() const
{
    if (qnRuntime->isVideoWallMode())
        return Qn::GlobalVideoWallModePermissionSet;

    if (qnRuntime->isActiveXMode())
        return Qn::GlobalActiveXModePermissionSet;

    if (!m_user)
        return Qn::NoGlobalPermissions;

    return qnResourceAccessManager->globalPermissions(m_user);
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr& resource) {
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr& resource, Qn::Permissions permissions) {
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
    m_user = context()->user();
    m_globalPermissions = calculateGlobalPermissions();
    m_readOnlyMode = qnCommon->isReadOnly();

    for (const QnResourcePtr& resource: qnResPool->getResources())
        updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    connect(resource, &QnResource::parentIdChanged,         this, &QnWorkbenchAccessController::updatePermissions);

    if (const QnLayoutResourcePtr& layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnLayoutResource::lockedChanged,   this, &QnWorkbenchAccessController::updatePermissions);
    }

    if (const QnUserResourcePtr& user = resource.dynamicCast<QnUserResource>())
    {
        connect(user, &QnUserResource::permissionsChanged,  this, &QnWorkbenchAccessController::updatePermissions);
        connect(user, &QnUserResource::userGroupChanged,    this, &QnWorkbenchAccessController::updatePermissions);
    }

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    disconnect(resource, NULL, this, NULL);

    setPermissionsInternal(resource, Qn::NoPermissions); /* So that the signal is emitted. */
    m_dataByResource.remove(resource);
}

void QnWorkbenchAccessController::at_accessibleResourcesChanged(const QnUuid& userId)
{
    if (m_user && m_user->getId() == userId)
        recalculateAllPermissions();
}

QString QnWorkbenchAccessController::userRoleName(const QnUserResourcePtr& user) const
{
    if (!user || !user->resourcePool())
        return QString();

    if (user->isOwner())
        return tr("Owner");

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(user);
    if (permissions.testFlag(Qn::GlobalAdminPermission)) // admin permission is checked before user role
        return standardRoleName(permissions);

    QnUuid groupId = user->userGroup();
    if (!groupId.isNull())
    {
        ec2::ApiUserGroupData userGroup = qnResourceAccessManager->userGroup(groupId);
        if (!userGroup.name.isEmpty())
            return userGroup.name;
    }

    return standardRoleName(permissions);
}

QString QnWorkbenchAccessController::userRoleDescription(const QnUserResourcePtr& user) const
{
    if (!user || !user->resourcePool())
        return QString();

    if (user->isOwner())
        return tr("Has access to whole system and can do everything.");

    return userRoleDescription(qnResourceAccessManager->globalPermissions(user), user->userGroup());
}

QString QnWorkbenchAccessController::userRoleDescription(Qn::GlobalPermissions permissions, const QnUuid& groupId) const
{
    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return standardRoleDescription(permissions); // admin permission is checked before user role

    if (!groupId.isNull() && !qnResourceAccessManager->userGroup(groupId).name.isEmpty())
        return customRoleDescription();

    return standardRoleDescription(permissions);
}

QString QnWorkbenchAccessController::standardRoleName(Qn::GlobalPermissions permissions)
{
    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return tr("Administrator");

    switch (permissions)
    {
        case Qn::GlobalAdvancedViewerPermissionSet:
            return tr("Advanced Viewer");

        case Qn::GlobalViewerPermissionSet:
            return tr("Viewer");

        case Qn::GlobalLiveViewerPermissionSet:
            return tr("Live Viewer");

        default:
            return tr("Custom");
    }
}

QString QnWorkbenchAccessController::standardRoleDescription(Qn::GlobalPermissions permissions)
{
    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return tr("Has access to whole system and can manage it. Can create users.");

    switch (permissions)
    {
        case Qn::GlobalAdvancedViewerPermissionSet:
            return tr("Can manage all cameras and bookmarks.");

        case Qn::GlobalViewerPermissionSet:
            return tr("Can view all cameras and export video.");

        case Qn::GlobalLiveViewerPermissionSet:
            return tr("Can view live video from all cameras.");

        default:
            return customPermissionsDescription();
    }
}

QString QnWorkbenchAccessController::customRoleDescription()
{
    return tr("Custom user role.");
}

QString QnWorkbenchAccessController::customPermissionsDescription()
{
    return tr("Custom permissions.");
}

const QList<Qn::GlobalPermissions>& QnWorkbenchAccessController::standardRoles()
{
    static const QList<Qn::GlobalPermissions> roles({
        Qn::GlobalAdminPermissionsSet,
        Qn::GlobalAdvancedViewerPermissionSet,
        Qn::GlobalViewerPermissionSet,
        Qn::GlobalLiveViewerPermissionSet
    });

    return roles;
}
