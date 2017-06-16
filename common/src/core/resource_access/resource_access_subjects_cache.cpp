#include "resource_access_subjects_cache.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnResourceAccessSubjectsCache::QnResourceAccessSubjectsCache(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    m_mutex(QnMutex::NonRecursive)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            /* Quick check */
            if (!resource->hasFlags(Qn::user))
                return;

            auto user = resource.dynamicCast<QnUserResource>();
            NX_ASSERT(user);
            if (user)
                handleUserAdded(user);
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            /* Quick check */
            if (!resource->hasFlags(Qn::user))
                return;

            auto user = resource.dynamicCast<QnUserResource>();
            NX_ASSERT(user);
            if (user)
                handleUserRemoved(user);
        });

    connect(userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnResourceAccessSubjectsCache::handleRoleAdded);
    connect(userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        &QnResourceAccessSubjectsCache::handleRoleRemoved);

    for (const auto& user: resourcePool()->getResources<QnUserResource>())
        handleUserAdded(user);

    for (const auto& role : userRolesManager()->userRoles())
        handleRoleAdded(role);
}

QList<QnResourceAccessSubject> QnResourceAccessSubjectsCache::allSubjects() const
{
    QnMutexLocker lk(&m_mutex);
    return m_subjects;
}

QList<QnResourceAccessSubject> QnResourceAccessSubjectsCache::usersInRole(
    const QnUuid& roleId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_usersByRoleId.value(roleId);
}

void QnResourceAccessSubjectsCache::handleUserAdded(const QnUserResourcePtr& user)
{
    QnResourceAccessSubject subject(user);
    {
        QnMutexLocker lk(&m_mutex);
        m_subjects << subject;
    }

    connect(user, &QnUserResource::userRoleChanged, this,
        &QnResourceAccessSubjectsCache::updateUserRole);

    updateUserRole(user);
}

void QnResourceAccessSubjectsCache::handleUserRemoved(const QnUserResourcePtr& user)
{
    user->disconnect(this);
    QnResourceAccessSubject subject(user);

    QnMutexLocker lk(&m_mutex);
    m_subjects.removeOne(subject);
    const auto roleId = m_roleIdByUserId.take(user->getId());
    removeUserFromRole(user, roleId);
}

void QnResourceAccessSubjectsCache::updateUserRole(const QnUserResourcePtr& user)
{
    const auto id = user->getId();

    QnMutexLocker lk(&m_mutex);
    const auto oldRoleId = m_roleIdByUserId.value(id);

    const auto userRole = user->userRole();
    const auto newRoleId = userRole == Qn::UserRole::CustomUserRole
        ? user->userRoleId()
        : QnUserRolesManager::predefinedRoleId(userRole);

    if (oldRoleId == newRoleId)
        return;

    removeUserFromRole(user, oldRoleId);

    m_roleIdByUserId[id] = newRoleId;
    m_usersByRoleId[newRoleId].append(user);
}

void QnResourceAccessSubjectsCache::handleRoleAdded(const ec2::ApiUserRoleData& userRole)
{
    QnMutexLocker lk(&m_mutex);
    m_subjects << userRole;

    QList<QnResourceAccessSubject> children;
    for (const auto& subject: m_subjects)
    {
        if (subject.user() && subject.user()->userRoleId() == userRole.id)
            children << subject;
    }

    if (children.isEmpty())
        m_usersByRoleId.remove(userRole.id);
    else
        m_usersByRoleId[userRole.id] = children;
}

void QnResourceAccessSubjectsCache::handleRoleRemoved(const ec2::ApiUserRoleData& userRole)
{
    QnMutexLocker lk(&m_mutex);
    m_subjects.removeOne(userRole);
    /* We are intentionally do not clear m_usersByRoleId field to make sure users with this role
     * will be correctly processed later. */
}

void QnResourceAccessSubjectsCache::removeUserFromRole(const QnUserResourcePtr& user,
    const QnUuid& roleId)
{
    auto users = m_usersByRoleId.find(roleId);
    if (users == m_usersByRoleId.end())
        return;

    users->removeOne(user);
    if (users->isEmpty())
        m_usersByRoleId.erase(users);
}
