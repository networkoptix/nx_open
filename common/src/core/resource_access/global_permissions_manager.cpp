#include "global_permissions_manager.h"

#include <nx/core/access/access_types.h>
#include <core/resource_access/resource_access_subjects_cache.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>
#include <common/common_module.h>

using namespace nx::core::access;

QnGlobalPermissionsManager::QnGlobalPermissionsManager(Mode mode, QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_mode(mode),
    m_mutex(QnMutex::NonRecursive),
    m_cache()
{
    if (mode == Mode::cached)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            &QnGlobalPermissionsManager::handleResourceAdded);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            &QnGlobalPermissionsManager::handleResourceRemoved);

        connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            &QnGlobalPermissionsManager::handleRoleAddedOrUpdated);
        connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
            &QnGlobalPermissionsManager::handleRoleRemoved);
    }
}

QnGlobalPermissionsManager::~QnGlobalPermissionsManager()
{

}

GlobalPermissions QnGlobalPermissionsManager::dependentPermissions(GlobalPermission value)
{
    switch (value)
    {
        case GlobalPermission::viewArchive:
            return GlobalPermission::viewBookmarks | GlobalPermission::exportArchive
                | GlobalPermission::manageBookmarks;
        case GlobalPermission::viewBookmarks:
            return GlobalPermission::manageBookmarks;
        default:
            break;
    }
    return {};
}

GlobalPermissions QnGlobalPermissionsManager::globalPermissions(
    const QnResourceAccessSubject& subject) const
{
    if (m_mode == Mode::cached)
    {
        QnMutexLocker lk(&m_mutex);
        auto iter = m_cache.find(subject.effectiveId());
        if (iter != m_cache.cend())
            return *iter;
    }
    return calculateGlobalPermissions(subject);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const QnResourceAccessSubject& subject,
    GlobalPermission requiredPermission) const
{
    if (requiredPermission == GlobalPermission::none)
        return true;

    return globalPermissions(subject).testFlag(requiredPermission);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const Qn::UserAccessData& accessRights,
    GlobalPermission requiredPermission) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;

    const auto& resPool = commonModule()->resourcePool();
    auto user = resPool->getResourceById<QnUserResource>(accessRights.userId);
    if (!user)
        return false;
    return hasGlobalPermission(user, requiredPermission);
}

GlobalPermissions QnGlobalPermissionsManager::filterDependentPermissions(GlobalPermissions source) const
{
    // TODO: #GDM code duplication with ::dependentPermissions() method.
    GlobalPermissions result = source;
    if (!result.testFlag(GlobalPermission::viewArchive))
        result &= ~(GlobalPermission::viewBookmarks | GlobalPermission::exportArchive);

    if (!result.testFlag(GlobalPermission::viewBookmarks))
        result &= ~GlobalPermissions(GlobalPermission::manageBookmarks);

    return result;
}

void QnGlobalPermissionsManager::updateGlobalPermissions(const QnResourceAccessSubject& subject)
{
    NX_ASSERT(m_mode == Mode::cached);

    setGlobalPermissionsInternal(subject, calculateGlobalPermissions(subject));
}

GlobalPermissions QnGlobalPermissionsManager::calculateGlobalPermissions(
    const QnResourceAccessSubject& subject) const
{
    GlobalPermissions result = {};

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
            return {};

        QnUuid userId = user->getId();

        switch (user->userRole())
        {
            case Qn::UserRole::customUserRole:
                result = globalPermissions(userRolesManager()->userRole(user->userRoleId()));
                break;
            case Qn::UserRole::owner:
            case Qn::UserRole::administrator:
                result = GlobalPermission::adminPermissions;
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
        result &= ~GlobalPermissions(GlobalPermission::admin);
        result = filterDependentPermissions(result);
    }

    return result;
}

void QnGlobalPermissionsManager::setGlobalPermissionsInternal(
    const QnResourceAccessSubject& subject, GlobalPermissions permissions)
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
        connect(user, &QnUserResource::userRoleChanged, this,
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

void QnGlobalPermissionsManager::handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole)
{
    updateGlobalPermissions(userRole);
    for (auto subject: resourceAccessSubjectsCache()->usersInRole(userRole.id))
        updateGlobalPermissions(subject);
}

void QnGlobalPermissionsManager::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject: resourceAccessSubjectsCache()->usersInRole(userRole.id))
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
    emit globalPermissionsChanged(subject, {});
}
