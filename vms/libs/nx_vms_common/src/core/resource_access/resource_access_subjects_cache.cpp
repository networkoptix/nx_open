// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_subjects_cache.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/common/system_context.h>

static GlobalPermissions permissions(const QnUserResourcePtr& user)
{
    auto p = user->getRawPermissions();
    if (user->isOwner())
        p |= GlobalPermission::owner;
    return p;
}

QnResourceAccessSubjectsCache::QnResourceAccessSubjectsCache(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_mutex(nx::Mutex::NonRecursive)
{
    NX_CRITICAL(resourcePool()
        && m_context->userRolesManager()
        && m_context->globalPermissionsManager());

    // TODO: #vkutin #sivanov Don't connect to users directly.
    // Add required signals to globalPermissionsManager add use it.

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

    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
        &QnResourceAccessSubjectsCache::handleRoleAddedOrUpdated);
    connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
        &QnResourceAccessSubjectsCache::handleRoleRemoved);

    connect(m_context->globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged, this,
        [this](const QnResourceAccessSubject& subject)
        {
            if (const auto user = subject.user())
                return updateSubjectRoles(subject, user->userRoleIds(), permissions(user));

            const auto role = m_context->userRolesManager()->userRole(subject.id());
            updateSubjectRoles(subject, role.parentRoleIds, role.permissions);
        });

    for (const auto& user: m_context->resourcePool()->getResources<QnUserResource>())
        handleUserAdded(user);

    for (const auto& role: m_context->userRolesManager()->userRoles())
        handleRoleAddedOrUpdated(role);
}

std::unordered_set<QnResourceAccessSubject> QnResourceAccessSubjectsCache::allSubjects() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_subjects;
}

std::unordered_set<QnResourceAccessSubject> QnResourceAccessSubjectsCache::subjectsInRole(
    const QnUuid& roleId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto it = m_subjectsInRole.find(roleId); it != m_subjectsInRole.end())
        return std::unordered_set<QnResourceAccessSubject>(it->second.begin(), it->second.end());
    return {};
}

std::unordered_set<QnResourceAccessSubject> QnResourceAccessSubjectsCache::allUsersInRole(
    const QnUuid& roleId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::unordered_set<QnResourceAccessSubject> result;
    forEachSubjectInRole(roleId, [&](const auto& s) { if (s.isUser()) result.insert(s); }, lock);
    NX_VERBOSE(this, "All users in role %1 - %2", roleId, nx::containerString(result));
    return result;
}

std::unordered_set<QnResourceAccessSubject> QnResourceAccessSubjectsCache::allSubjectsInRole(
    const QnUuid& roleId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::unordered_set<QnResourceAccessSubject> result;
    forEachSubjectInRole(roleId, [&](const auto& s) { result.insert(s); }, lock);
    NX_VERBOSE(this, "All subjects in role %1 - %2", roleId, nx::containerString(result));
    return result;
}

static void insertWithParents(
    const QnResourceAccessSubject& subject,
    const std::unordered_map<QnUuid, std::unordered_set<QnResourceAccessSubject>>& rolesOfSubject,
    std::vector<QnUuid> *result)
{
    if (const auto it = rolesOfSubject.find(subject.id()); it != rolesOfSubject.end())
    {
        for (const auto parentSubject: it->second)
        {
            result->push_back(parentSubject.id());
            insertWithParents(parentSubject, rolesOfSubject, result);
        }
    }
}

std::vector<QnUuid> QnResourceAccessSubjectsCache::subjectWithParents(
    const QnResourceAccessSubject& subject) const
{
    std::vector<QnUuid> result{subject.id()};
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        insertWithParents(subject, m_rolesOfSubject, &result);
    }
    NX_VERBOSE(this, "All subjects for %1 - %2", subject, nx::containerString(result));
    return result;
}

void QnResourceAccessSubjectsCache::handleUserAdded(const QnUserResourcePtr& user)
{
    connect(user.get(), &QnUserResource::userRolesChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            updateSubjectRoles(user, user->userRoleIds(), permissions(user));
        });
    QnResourceAccessSubject subject(user);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_subjects.insert(subject);
    updateSubjectRoles(subject, user->userRoleIds(), permissions(user), lock);
}

void QnResourceAccessSubjectsCache::handleUserRemoved(const QnUserResourcePtr& user)
{
    user->disconnect(this);
    QnResourceAccessSubject subject(user);

    NX_MUTEX_LOCKER lock(&m_mutex);
    updateSubjectRoles(subject, {}, {}, lock);
    m_subjects.erase(subject);
}

void QnResourceAccessSubjectsCache::handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole)
{
    const QnResourceAccessSubject subject(userRole);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_subjects.insert(subject);
    updateSubjectRoles(subject, userRole.parentRoleIds, userRole.permissions, lock);
}

void QnResourceAccessSubjectsCache::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    const QnResourceAccessSubject subject(userRole);

    NX_MUTEX_LOCKER lock(&m_mutex);
    updateSubjectRoles(subject, {}, {}, lock);
    m_subjects.erase(subject);
}

void QnResourceAccessSubjectsCache::updateSubjectRoles(
    const QnResourceAccessSubject& subject,
    const std::vector<QnUuid>& newRoleIds,
    GlobalPermissions rawPermissions)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    updateSubjectRoles(subject, newRoleIds, rawPermissions, lock);
}

void QnResourceAccessSubjectsCache::updateSubjectRoles(
    const QnResourceAccessSubject& subject,
    const std::vector<QnUuid>& newRoleIds,
    GlobalPermissions rawPermissions,
    const nx::MutexLocker&)
{
    std::unordered_set<QnResourceAccessSubject> newRoles;
    if (const auto& id = QnUserRolesManager::predefinedRoleId(rawPermissions))
        newRoles.insert(nx::vms::api::UserRoleData{*id, ""});
    for (const auto& id: newRoleIds)
        newRoles.insert(nx::vms::api::UserRoleData{id, ""});

    static const std::unordered_set<QnResourceAccessSubject> kEmpty;
    const std::unordered_set<QnResourceAccessSubject>* knownRoles = &kEmpty;
    if (const auto it = m_rolesOfSubject.find(subject.id()); it != m_rolesOfSubject.end())
        knownRoles = &it->second;

    if (newRoles == *knownRoles)
    {
        NX_DEBUG(this, "%1 roles are still %2", subject, nx::containerString(*knownRoles));
        return;
    }

    NX_DEBUG(this, "%1 roles are changed from %2 to %3", subject,
        nx::containerString(*knownRoles), nx::containerString(newRoles));

    for (const auto& role: *knownRoles) //< Cleanup subjects from old roles.
    {
        const auto subjectsIt = m_subjectsInRole.find(role.id());
        if (NX_ASSERT(subjectsIt != m_subjectsInRole.end(), "Role: %1", role.id()))
        {
            NX_ASSERT(subjectsIt->second.erase(subject), "%1 -> %2", role.id(), subject.id());
            if (subjectsIt->second.empty())
                m_subjectsInRole.erase(subjectsIt);
        }
    }

    for (const auto& role: newRoles) //< Add subject to new roles.
        m_subjectsInRole[role.id()].insert(subject);

    if (newRoles.empty())
        m_rolesOfSubject.erase(subject.id());
    else
        m_rolesOfSubject[subject.id()] = std::move(newRoles);
}

template<typename Action>
void QnResourceAccessSubjectsCache::forEachSubjectInRole(
    const QnUuid& roleId, const Action& action, const nx::MutexLocker& lock) const
{
    const auto& it = m_subjectsInRole.find(roleId);
    if (it == m_subjectsInRole.end())
        return;

    for (const auto& subject: it->second)
    {
        action(subject);
        if (subject.isRole())
            forEachSubjectInRole(subject.id(), action, lock);
    }
}
