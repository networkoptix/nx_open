#include "resource_access_manager.h"

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_videowall_data.h>
#include <nx_ec/data/api_webpage_data.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/raii_guard.h>

//#define DEBUG_PERMISSIONS
#ifdef DEBUG_PERMISSIONS
#define TRACE(...) qDebug() << "QnResourceAccessManager: " << __VA_ARGS__;
#else
#define TRACE(...)
#endif

QnResourceAccessManager::QnResourceAccessManager(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_globalPermissionsCache(),
    m_permissionsCache()
{
    /* This change affects all accessible resources. */
    connect(qnCommon,& QnCommonModule::readOnlyChanged, this,
        &QnResourceAccessManager::recalculateAllPermissions);

    connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            QnAbstractResourceAccessProvider::Source /*value*/)
        {
            updatePermissions(subject, resource);
        });

    /* update global permissions */
    /*connect to subject removed*/
    /*  user group change */

    recalculateAllPermissions();
}

Qn::GlobalPermissions QnResourceAccessManager::dependentPermissions(Qn::GlobalPermission value)
{
    switch (value)
    {
        case Qn::GlobalViewArchivePermission:
            return Qn::GlobalViewBookmarksPermission | Qn::GlobalExportPermission
                | Qn::GlobalManageBookmarksPermission;
        case Qn::GlobalViewBookmarksPermission:
            return Qn::GlobalManageBookmarksPermission;
        default:
            break;
    }
    return Qn::NoGlobalPermissions;
}

Qn::GlobalPermissions QnResourceAccessManager::filterDependentPermissions(
    Qn::GlobalPermissions source) const
{
    //TODO: #GDM code duplication with ::dependentPermissions() method.
    Qn::GlobalPermissions result = source;
    if (!result.testFlag(Qn::GlobalViewArchivePermission))
    {
        result &= ~Qn::GlobalViewBookmarksPermission;
        result &= ~Qn::GlobalExportPermission;
    }

    if (!result.testFlag(Qn::GlobalViewBookmarksPermission))
    {
        result &= ~Qn::GlobalManageBookmarksPermission;
    }
    return result;
}

void QnResourceAccessManager::setGlobalPermissionsInternal(const QnResourceAccessSubject& subject,
    Qn::GlobalPermissions permissions)
{
    {
        QnMutexLocker lk(&m_mutex);
        auto& value = m_globalPermissionsCache[subject.id()];
        if (value == permissions)
            return;
        value = permissions;
    }
    emit globalPermissionsChanged(subject, permissions);
}

void QnResourceAccessManager::setPermissionsInternal(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource, Qn::Permissions permissions)
{
    {
        QnMutexLocker lk(&m_mutex);
        PermissionKey key(subject.id(), resource->getId());
        auto& value = m_permissionsCache[key];
        if (value == permissions)
            return;
        value = permissions;
    }
    emit permissionsChanged(subject, resource, permissions);
}

Qn::GlobalPermissions QnResourceAccessManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_globalPermissionsCache.find(subject.effectiveId());
        if (iter != m_globalPermissionsCache.cend())
            return *iter;
    }
    return calculateGlobalPermissions(subject);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const Qn::UserAccessData& accessRights,
    Qn::GlobalPermission requiredPermission) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;

    auto user = qnResPool->getResourceById<QnUserResource>(accessRights.userId);
    if (!user)
        return false;
    return hasGlobalPermission(user, requiredPermission);
}

bool QnResourceAccessManager::hasGlobalPermission(
    const QnResourceAccessSubject& subject,
    Qn::GlobalPermission requiredPermission) const
{
    if (requiredPermission == Qn::NoGlobalPermissions)
        return true;

    return globalPermissions(subject).testFlag(requiredPermission);
}

Qn::Permissions QnResourceAccessManager::permissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!subject.isValid() || !resource)
        return Qn::NoPermissions;

    /* Resource is not added to pool, checking if we can create such resource. */
    if (!resource->resourcePool())
        return canCreateResource(subject, resource)
            ? Qn::ReadWriteSavePermission
            : Qn::NoPermissions;

    PermissionKey key(subject.id(), resource->getId());
    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_permissionsCache.find(key);
        if (iter != m_permissionsCache.cend())
            return *iter;
    }

    NX_ASSERT("Should never normally get here.");
    return calculatePermissions(subject, resource);
}

bool QnResourceAccessManager::hasPermission(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(subject, resource) & requiredPermissions) == requiredPermissions;
}

bool QnResourceAccessManager::hasPermission(
    const Qn::UserAccessData& accessRights,
    const QnResourcePtr& resource,
    Qn::Permissions permissions) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;
    if (accessRights.access == Qn::UserAccessData::Access::ReadAllResources
        && permissions == Qn::ReadPermission)
    {
        return true;
    }

    auto userResource = qnResPool->getResourceById<QnUserResource>(accessRights.userId);
    if (!userResource)
        return false;

    return hasPermission(userResource, resource, permissions);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target) const
{
    NX_ASSERT(subject.isValid());
    NX_ASSERT(target);

    if (!subject.isValid())
        return false;

    if (qnCommon->isReadOnly())
        return false;

    /* Check new layouts creating. */
    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return canCreateLayout(subject, layout->getParentId());

    /* Check new users creating. */
    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return canCreateUser(subject, targetUser->getRawPermissions(), targetUser->isOwner());

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return canCreateStorage(subject, storage->getParentId());

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return canCreateVideoWall(subject);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return canCreateWebPage(subject);

    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiStorageData& data) const
{
    return canCreateStorage(subject, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiLayoutData& data) const
{
    return canCreateLayout(subject, data.parentId);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiUserData& data) const
{
    if (!data.groupId.isNull() && !qnUserRolesManager->hasRole(data.groupId))
        return false;

    return canCreateUser(subject, data.permissions, data.isAdmin);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiVideowallData& data) const
{
    Q_UNUSED(data);
    return canCreateVideoWall(subject);
}

bool QnResourceAccessManager::canCreateResource(const QnResourceAccessSubject& subject,
    const ec2::ApiWebPageData& data) const
{
    Q_UNUSED(data);
    return canCreateWebPage(subject);
}

void QnResourceAccessManager::recalculateAllPermissions()
{
    for (const auto& subject : QnAbstractResourceAccessProvider::allSubjects())
    {
        updateGlobalPermissions(subject);
        for (const QnResourcePtr& resource: qnResPool->getResources())
            updatePermissions(subject, resource);
    }
}

void QnResourceAccessManager::updateGlobalPermissions(const QnResourceAccessSubject& subject)
{
    setGlobalPermissionsInternal(subject, calculateGlobalPermissions(subject));
}

void QnResourceAccessManager::updatePermissions(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target)
{
    setPermissionsInternal(subject, target, calculatePermissions(subject, target));
}

Qn::GlobalPermissions QnResourceAccessManager::calculateGlobalPermissions(
    const QnResourceAccessSubject& subject) const
{
    Qn::GlobalPermissions result = Qn::NoGlobalPermissions;

    if (!subject.isValid())
        return result;

    if (subject.user())
    {
        auto user = subject.user();
        if (!user->isEnabled())
            return result;

        /* Handle just-created user situation. */
        if (user->flags().testFlag(Qn::local))
            return filterDependentPermissions(user->getRawPermissions());

        /* User is already removed. Problems with 'on_resource_removed' connection order. */
        if (!user->resourcePool())
            return Qn::NoGlobalPermissions;

        QnUuid userId = user->getId();

        switch (user->role())
        {
            case Qn::UserRole::CustomUserGroup:
                result = globalPermissions(qnUserRolesManager->userRole(user->userGroup()));
                break;
            case Qn::UserRole::Owner:
            case Qn::UserRole::Administrator:
                result = Qn::GlobalAdminPermissionSet;
                break;
            default:
                result = filterDependentPermissions(user->getRawPermissions());
                break;
        }
    }
    else
    {
        /* If the group does not exist, permissions will be empty. */
        result = subject.role().permissions;

        /* If user belongs to group, he cannot be an admin - by design. */
        result &= ~Qn::GlobalAdminPermission;
        result = filterDependentPermissions(result);
    }

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissions(
    const QnResourceAccessSubject& subject, const QnResourcePtr& target) const
{
    NX_ASSERT(target);
    if (subject.user() && !subject.user()->isEnabled())
        return Qn::NoPermissions;

    if (QnUserResourcePtr targetUser = target.dynamicCast<QnUserResource>())
        return calculatePermissionsInternal(subject, targetUser);

    if (QnLayoutResourcePtr layout = target.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(subject, layout);

    if (QnVirtualCameraResourcePtr camera = target.dynamicCast<QnVirtualCameraResource>())
        return calculatePermissionsInternal(subject, camera);

    if (QnMediaServerResourcePtr server = target.dynamicCast<QnMediaServerResource>())
        return calculatePermissionsInternal(subject, server);

    if (QnStorageResourcePtr storage = target.dynamicCast<QnStorageResource>())
        return calculatePermissionsInternal(subject, storage);

    if (QnVideoWallResourcePtr videoWall = target.dynamicCast<QnVideoWallResource>())
        return calculatePermissionsInternal(subject, videoWall);

    if (QnWebPageResourcePtr webPage = target.dynamicCast<QnWebPageResource>())
        return calculatePermissionsInternal(subject, webPage);

    NX_ASSERT("invalid resource type");
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject,
    const QnVirtualCameraResourcePtr& camera) const
{
    if (subject.user())
    {
        TRACE("Calculate permissions of " << subject.user()->getName() << " to camera " << camera->getName());
    }
    else
    {
        TRACE("Calculate permissions of role " << subject.role().name << " to camera " << camera->getName());
    }
    NX_ASSERT(camera);

    Qn::Permissions result = Qn::NoPermissions;

    /* Admins must be able to remove any cameras to delete servers.  */
    if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        result |= Qn::RemovePermission;

    if (!qnResourceAccessProvider->hasAccess(subject, camera))
        return result;

    result |= Qn::ReadPermission;
    if (hasGlobalPermission(subject, Qn::GlobalExportPermission))
        result |= Qn::ExportPermission;

    if (hasGlobalPermission(subject, Qn::GlobalUserInputPermission))
        result |= Qn::WritePtzPermission;

    if (qnCommon->isReadOnly())
        return result;

    if (hasGlobalPermission(subject, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnMediaServerResourcePtr& server) const
{
    NX_ASSERT(server);
    /* All users must have at least ReadPermission to send api requests
     * (recorded periods, bookmarks, etc) and view servers on shared layouts. */
    Qn::Permissions result = Qn::ReadPermission;

    if (hasGlobalPermission(subject, Qn::GlobalAdminPermission) && !qnCommon->isReadOnly())
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnStorageResourcePtr& storage) const
{
    NX_ASSERT(storage);
    auto server = storage->getParentServer();
    if (!server)
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;  /*< Server is already deleted. Storage can be removed. */

    auto serverPermissions = permissions(subject, server);
    if (serverPermissions.testFlag(Qn::RemovePermission))
        return Qn::ReadWriteSavePermission | Qn::RemovePermission;

    if (serverPermissions.testFlag(Qn::SavePermission))
        return Qn::ReadWriteSavePermission;

    if (serverPermissions.testFlag(Qn::ReadPermission))
        return Qn::ReadPermission;

    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnVideoWallResourcePtr& videoWall) const
{
    NX_ASSERT(videoWall);

    if (hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
    {
        if (qnCommon->isReadOnly())
            return Qn::ReadPermission;
        return Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;
    }
    return Qn::NoPermissions;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnWebPageResourcePtr& webPage) const
{
    NX_ASSERT(webPage);

    if (!qnResourceAccessProvider->hasAccess(subject, webPage))
        return Qn::NoPermissions;

    Qn::Permissions result = Qn::ReadPermission;
    if (qnCommon->isReadOnly())
        return result;

    /* Web Page behaves totally like camera. */
    if (hasGlobalPermission(subject, Qn::GlobalEditCamerasPermission))
        result |= Qn::ReadWriteSavePermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return result;
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnLayoutResourcePtr& layout) const
{
    if (subject.user())
    {
        TRACE("Calculate permissions of " << subject.user()->getName() << " to layout " << layout->getName());
    }
    else
    {
        TRACE("Calculate permissions of role " << subject.role().name << " to layout " << layout->getName());
    }

    NX_ASSERT(layout);
    if (!subject.isValid() || !layout)
        return Qn::NoPermissions;

    //TODO: #GDM Code duplication with QnWorkbenchAccessController::calculatePermissionsInternal
    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!qnCommon->isReadOnly())
            return permissions;
        return permissions& ~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions)
    {
        if (!layout->locked())
            return permissions;
        return permissions& ~(Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };

    /* Layouts with desktop cameras are not to be modified but can be removed. */
    if (qnResPool->isAutoGeneratedLayout(layout))
    {
        const auto items = layout->getItems();
        bool hasDesktopCamera = std::any_of(items.cbegin(), items.cend(), [this](const QnLayoutItemData& item)
        {
            QnResourcePtr childResource = qnResPool->getResourceById(item.resource.id);
            return childResource && childResource->hasFlags(Qn::desktop_camera);
        });

        if (hasDesktopCamera)
            return checkReadOnly(Qn::ReadPermission | Qn::RemovePermission);
    }


    /* Calculate base layout permissions */
    auto base = [&]() -> Qn::Permissions
    {
        /* Owner can do anything. */
        if (subject.user() && subject.user()->isOwner())
            return Qn::FullLayoutPermissions;

        /* Access to global layouts. Simple check is enough, exported layouts are checked on the client side. */
        if (layout->isShared())
        {
            if (!qnResourceAccessProvider->hasAccess(subject, layout))
                return Qn::NoPermissions;

            /* Global layouts editor. */
            if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
                return Qn::FullLayoutPermissions;

            /* Ability to cleanup unused autogenerated layouts. */
            if (hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission)
                && qnResPool->isAutoGeneratedLayout(layout))
                return Qn::FullLayoutPermissions;

            return Qn::ModifyLayoutPermission;
        }

        /* Checking other user's layout. Only users can have access to them. */
        if (auto user = subject.user())
        {
            QnUuid ownerId = layout->getParentId();
            if (ownerId != user->getId())
            {
                QnUserResourcePtr owner = qnResPool->getResourceById<QnUserResource>(ownerId);

                if (!owner)
                {
                    /* Everybody can modify lite client layout. */
                    const auto server = qnResPool->getResourceById<QnMediaServerResource>(ownerId);
                    if (server)
                        return Qn::FullLayoutPermissions;

                    /* Layout of user, which we don't know of. */
                    return hasGlobalPermission(subject, Qn::GlobalAdminPermission)
                        ? Qn::FullLayoutPermissions
                        : Qn::NoPermissions;
                }

                /* We can modify layout for user if we can modify this user. */
                Qn::Permissions userPermissions = permissions(subject, owner);
                if (userPermissions.testFlag(Qn::SavePermission))
                    return Qn::FullLayoutPermissions;

                /* We can see layouts for another users if we are able to see these users. */
                if (userPermissions.testFlag(Qn::ReadPermission))
                    return Qn::ModifyLayoutPermission;

                return Qn::NoPermissions;
            }
        }

        /* User can do whatever he wants with own layouts. */
        return Qn::FullLayoutPermissions;
    };

    return checkLocked(checkReadOnly(base()));
}

Qn::Permissions QnResourceAccessManager::calculatePermissionsInternal(
    const QnResourceAccessSubject& subject, const QnUserResourcePtr& targetUser) const
{
    NX_ASSERT(targetUser);

    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!qnCommon->isReadOnly())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission
            | Qn::WritePasswordPermission | Qn::WriteEmailPermission | Qn::WriteFullNamePermission);
    };

    auto checkUserType = [targetUser](Qn::Permissions permissions)
    {
        switch (targetUser->userType())
        {
        case QnUserType::Ldap:
            return permissions &~ (Qn::WriteNamePermission | Qn::WritePasswordPermission | Qn::WriteEmailPermission);
        case QnUserType::Cloud:
            return permissions &~ (Qn::WritePasswordPermission | Qn::WriteEmailPermission | Qn::WriteFullNamePermission);
        default:
            break;
        }
        return permissions;
    };

    Qn::Permissions result = Qn::NoPermissions;
    if (targetUser == subject.user())
    {
        /* Everyone can edit own data. */
        result |= Qn::ReadWriteSavePermission | Qn::WritePasswordPermission
            | Qn::WriteEmailPermission | Qn::WriteFullNamePermission;
    }
    else
    {
        if (hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        {
            result |= Qn::ReadPermission;

            /* Nobody can do something with owner. */
            if (targetUser->isOwner())
                return result;

            /* Only users may have admin permissions. */
            NX_ASSERT(subject.user());
            auto isOwner = subject.user() && subject.user()->isOwner();

            /* Admins can only be edited by owner, other users - by all admins. */
            if (isOwner || !hasGlobalPermission(targetUser, Qn::GlobalAdminPermission))
                result |= Qn::FullUserPermissions;
        }
    }

    return checkReadOnly(checkUserType(result));
}

bool QnResourceAccessManager::canCreateStorage(const QnResourceAccessSubject& subject,
    const QnUuid& storageParentId) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    auto server = qnResPool->getResourceById<QnMediaServerResource>(storageParentId);
    return hasPermission(subject, server, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateLayout(const QnResourceAccessSubject& subject,
    const QnUuid& layoutParentId) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    /* Everybody can create own layouts. */
    if (subject.user() && layoutParentId == subject.user()->getId())
        return true;

    /* Only admins and videowall-admins can create global layouts. */
    if (layoutParentId.isNull())
    {
        return hasGlobalPermission(subject, Qn::GlobalAdminPermission)
            || hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission);
    }

    const auto ownerResource = qnResPool->getResourceById(layoutParentId);

    /* Everybody can create layout for lite client. */
    const auto parentServer = ownerResource.dynamicCast<QnMediaServerResource>();
    if (parentServer)
    {
        if (!parentServer->getServerFlags().testFlag(Qn::SF_HasLiteClient))
            return false;

        /* There can only be one layout for each lite client. */
        const auto existingLayouts = qnResPool->getResources<QnLayoutResource>().filtered(
            [id = parentServer->getId()](const QnLayoutResourcePtr& layout)
            {
                return layout->getId() == id;
            });

        return existingLayouts.isEmpty();
    }

    const auto owner = ownerResource.dynamicCast<QnUserResource>();
    if (!owner)
        return false;

    /* We can create layout for user if we can modify this user. */
    return hasPermission(subject, owner, Qn::SavePermission);
}

bool QnResourceAccessManager::canCreateUser(const QnResourceAccessSubject& subject,
    Qn::GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    /* Nobody can create owners. */
    if (isOwner)
        return false;

    /* Only owner can create admins. */
    if (targetPermissions.testFlag(Qn::GlobalAdminPermission))
        return subject.user() && subject.user()->isOwner();

    /* Admins can create other users. */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateVideoWall(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    /* Only admins can create new videowalls (and attach new screens). */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canCreateWebPage(const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    /* Only admins can add new web pages. */
    return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiStorageData& update) const
{
    NX_ASSERT(target.dynamicCast<QnStorageResource>());

    /* Storages cannot be moved from one server to another. */
    if (target->getParentId() != update.parentId)
        return false;

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiLayoutData& update) const
{
    NX_ASSERT(target.dynamicCast<QnLayoutResource>());

    /* If we are changing layout parent, it is equal to creating new layout. */
    if (target->getParentId() != update.parentId)
        return canCreateLayout(subject, update.parentId);

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiUserData& update) const
{
    if (!update.groupId.isNull() && !qnUserRolesManager->hasRole(update.groupId))
        return false;

    auto userResource = target.dynamicCast<QnUserResource>();
    NX_ASSERT(userResource);

    if (!subject.isValid() || !target)
        return false;

    /* Nobody can make user an owner (isAdmin ec2 field) unless target user is already an owner. */
    if (!userResource->isOwner() && update.isAdmin)
        return false;

    /* We should have full access to user to modify him. */
    Qn::Permissions requiredPermissions = Qn::ReadWriteSavePermission;

    if (target->getName() != update.name)
        requiredPermissions |= Qn::WriteNamePermission;

    if (userResource->getHash() != update.hash)
        requiredPermissions |= Qn::WritePasswordPermission;

    //TODO: #rvasilenko describe somewhere password changing logic, it looks like hell to me
    if (userResource->getDigest() != update.digest)
        requiredPermissions |= Qn::WritePasswordPermission;

    if (userResource->getRawPermissions() != update.permissions)
        requiredPermissions |= Qn::WriteAccessRightsPermission;

    if (userResource->getEmail() != update.email)
        requiredPermissions |= Qn::WriteEmailPermission;

    if (userResource->fullName() != update.fullName)
        requiredPermissions |= Qn::WriteFullNamePermission;

    return hasPermission(subject, target, requiredPermissions);
}

bool QnResourceAccessManager::canModifyResource(const QnResourceAccessSubject& subject,
    const QnResourcePtr& target, const ec2::ApiVideowallData& update) const
{
    if (!subject.isValid() || qnCommon->isReadOnly())
        return false;

    auto videoWallResource = target.dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videoWallResource);

    auto hasItemsChange = [items = videoWallResource->items()->getItems(), update]
    {
        /* Quick check */
        if ((size_t)items.size() != update.items.size())
            return true;

        for (auto updated : update.items)
        {
            if (!items.contains(updated.guid))
                return true;
        }

        return false;
    };

    /* Only admin can add and remove videowall items. */
    if (hasItemsChange())
        return hasGlobalPermission(subject, Qn::GlobalAdminPermission);

    /* Otherwise - default behavior. */
    return hasPermission(subject, target, Qn::SavePermission);
}
