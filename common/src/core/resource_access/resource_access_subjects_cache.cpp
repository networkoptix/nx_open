#include "resource_access_subjects_cache.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>

#include <core/resource/user_resource.h>

QnResourceAccessSubjectsCache::QnResourceAccessSubjectsCache(QObject* parent):
    base_type(parent),
    m_mutex(QnMutex::NonRecursive)
{

    connect(qnResPool, &QnResourcePool::resourceAdded, this,
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

    connect(qnResPool, &QnResourcePool::resourceRemoved, this,
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

    connect(qnUserRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnResourceAccessSubjectsCache::handleRoleAdded);
    connect(qnUserRolesManager, &QnUserRolesManager::userRoleRemoved, this,
        &QnResourceAccessSubjectsCache::handleRoleRemoved);

    for (const auto& user : qnResPool->getResources<QnUserResource>())
        handleUserAdded(user);

    for (const auto& role : qnUserRolesManager->userRoles())
        handleRoleAdded(role);
}

QList<QnResourceAccessSubject> QnResourceAccessSubjectsCache::allSubjects() const
{
    QnMutexLocker lk(&m_mutex);
    return m_subjects;
}

QList<QnResourceAccessSubject> QnResourceAccessSubjectsCache::dependentSubjects(
    const QnResourceAccessSubject& subject) const
{
    QnMutexLocker lk(&m_mutex);
    return m_dependent.value(subject.id());
}

void QnResourceAccessSubjectsCache::handleUserAdded(const QnUserResourcePtr& user)
{
    QnResourceAccessSubject subject(user);
    {
        QnMutexLocker lk(&m_mutex);
        m_subjects << subject;
    }

    connect(user, &QnUserResource::userGroupChanged, this,
        &QnResourceAccessSubjectsCache::updateUserDependency);

    updateUserDependency(user);
}

void QnResourceAccessSubjectsCache::handleUserRemoved(const QnUserResourcePtr& user)
{
    user->disconnect(this);
    QnResourceAccessSubject subject(user);

    QnMutexLocker lk(&m_mutex);
    m_subjects.removeOne(subject);
    const auto roleId = m_roleByUser.take(user->getId());
    if (!roleId.isNull())
    {
        m_dependent[roleId].removeOne(subject);
        if (m_dependent[roleId].isEmpty())
            m_dependent.remove(roleId);
    }
}

void QnResourceAccessSubjectsCache::updateUserDependency(const QnUserResourcePtr& user)
{
    const auto id = user->getId();

    QnMutexLocker lk(&m_mutex);
    const auto oldRoleId = m_roleByUser.value(id);
    const auto newRoleId = user->userGroup();
    if (oldRoleId == newRoleId)
        return;

    if (!oldRoleId.isNull())
    {
        m_dependent[oldRoleId].removeOne(user);
        if (m_dependent[oldRoleId].isEmpty())
            m_dependent.remove(oldRoleId);
    }

    if (!newRoleId.isNull())
    {
        m_roleByUser[id] = newRoleId;
        m_dependent[newRoleId].append(user);
    }
}

void QnResourceAccessSubjectsCache::handleRoleAdded(const ec2::ApiUserGroupData& userRole)
{
    QnMutexLocker lk(&m_mutex);
    m_subjects << userRole;

    QList<QnResourceAccessSubject> children;
    for (const auto& subject: m_subjects)
    {
        if (subject.user() && subject.user()->userGroup() == userRole.id)
            children << subject;
    }
    if (children.isEmpty())
        m_dependent.remove(userRole.id);
    else
        m_dependent[userRole.id] = children;
}

void QnResourceAccessSubjectsCache::handleRoleRemoved(const ec2::ApiUserGroupData& userRole)
{
    QnMutexLocker lk(&m_mutex);
    m_subjects.removeOne(userRole);
    /* We are intentionally do not clear m_dependent field to make sure users with this role
     * will be correctly processed later. */
}
