#include "global_permissions_manager.h"

#include <common/common_module.h>

#include <core/resource_access/providers/abstract_resource_access_provider.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnGlobalPermissionsManager::QnGlobalPermissionsManager(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive),
    m_cache()
{
    /* This change affects all accessible resources. */
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this,
        &QnGlobalPermissionsManager::recalculateAllPermissions);

    connect(qnResPool, &QnResourcePool::resourceAdded, this,
        &QnGlobalPermissionsManager::handleResourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
        &QnGlobalPermissionsManager::handleResourceRemoved);

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnGlobalPermissionsManager::handleRoleAddedOrUpdated);
    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnGlobalPermissionsManager::handleRoleRemoved);
}

QnGlobalPermissionsManager::~QnGlobalPermissionsManager()
{

}

Qn::GlobalPermissions QnGlobalPermissionsManager::dependentPermissions(Qn::GlobalPermission value)
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

Qn::GlobalPermissions QnGlobalPermissionsManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_cache.find(subject.effectiveId());
        if (iter != m_cache.cend())
            return *iter;
    }
    return calculateGlobalPermissions(subject);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const QnResourceAccessSubject& subject,
    Qn::GlobalPermission requiredPermission) const
{
    if (requiredPermission == Qn::NoGlobalPermissions)
        return true;

    return globalPermissions(subject).testFlag(requiredPermission);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const Qn::UserAccessData& accessRights,
    Qn::GlobalPermission requiredPermission) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;

    auto user = qnResPool->getResourceById<QnUserResource>(accessRights.userId);
    if (!user)
        return false;
    return hasGlobalPermission(user, requiredPermission);
}

void QnGlobalPermissionsManager::recalculateAllPermissions()
{
    for (const auto& subject : QnAbstractResourceAccessProvider::allSubjects())
        updateGlobalPermissions(subject);
}

Qn::GlobalPermissions QnGlobalPermissionsManager::filterDependentPermissions(Qn::GlobalPermissions source) const
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

void QnGlobalPermissionsManager::updateGlobalPermissions(const QnResourceAccessSubject& subject)
{
    setGlobalPermissionsInternal(subject, calculateGlobalPermissions(subject));
}

Qn::GlobalPermissions QnGlobalPermissionsManager::calculateGlobalPermissions(
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

void QnGlobalPermissionsManager::setGlobalPermissionsInternal(
    const QnResourceAccessSubject& subject, Qn::GlobalPermissions permissions)
{
    {
        QnMutexLocker lk(&m_mutex);
        auto& value = m_cache[subject.id()];
        if (value == permissions)
            return;
        value = permissions;
    }
    emit globalPermissionsChanged(subject, permissions);
}

void QnGlobalPermissionsManager::handleResourceAdded(const QnResourcePtr& resource)
{
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        updateGlobalPermissions(user);

        connect(user, &QnUserResource::permissionsChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
        connect(user, &QnUserResource::userGroupChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
        connect(user, &QnUserResource::enabledChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
    }
}

void QnGlobalPermissionsManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    disconnect(resource, nullptr, this, nullptr);
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
}

void QnGlobalPermissionsManager::handleRoleAddedOrUpdated(const ec2::ApiUserGroupData& userRole)
{
    updateGlobalPermissions(userRole);
    for (auto subject : QnAbstractResourceAccessProvider::dependentSubjects(userRole))
        updateGlobalPermissions(subject);
}

void QnGlobalPermissionsManager::handleRoleRemoved(const ec2::ApiUserGroupData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject : QnAbstractResourceAccessProvider::dependentSubjects(userRole))
        updateGlobalPermissions(subject);
}

void QnGlobalPermissionsManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    auto id = subject.id();
    {
        QnMutexLocker lk(&m_mutex);
        NX_ASSERT(m_cache.contains(id));
        m_cache.remove(id);
    }
    emit globalPermissionsChanged(subject, Qn::NoGlobalPermissions);
}
