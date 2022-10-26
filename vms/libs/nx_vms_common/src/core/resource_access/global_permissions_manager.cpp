// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permissions_manager.h"

#include <common/common_module.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/system_context.h>
#include <nx/utils/log/log.h>

using namespace nx::core::access;

QnGlobalPermissionsManager::QnGlobalPermissionsManager(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(parent),
    nx::vms::common::SystemContextAware(context),
    m_mode(mode),
    m_mutex(nx::Mutex::NonRecursive)
{
    if (mode == Mode::cached)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            &QnGlobalPermissionsManager::handleResourceAdded);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            &QnGlobalPermissionsManager::handleResourceRemoved);

        connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleAddedOrUpdated, this,
            &QnGlobalPermissionsManager::handleRoleAddedOrUpdated);
        connect(m_context->userRolesManager(), &QnUserRolesManager::userRoleRemoved, this,
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
        GlobalPermissions result;
        bool haveUnknownValues = false;
        NX_MUTEX_LOCKER lk(&m_mutex);
        for (const auto& effectiveId:
            m_context->resourceAccessSubjectsCache()->subjectWithParents(subject))
        {
            auto iter = m_cache.find(effectiveId);
            if (iter == m_cache.cend())
            {
                haveUnknownValues = true;
                break;
            }
            result |= *iter;
        }
        if (!haveUnknownValues)
            return result;
        // The cache is not calculated yet, fallback into direct calculation.
    }
    return calculateGlobalPermissions(subject);
}

bool QnGlobalPermissionsManager::containsPermission(
    GlobalPermissions globalPermissions,
    GlobalPermission requiredPermission)
{
    return requiredPermission == GlobalPermission::none
        ? true
        : globalPermissions.testFlag(requiredPermission);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const QnResourceAccessSubject& subject,
    GlobalPermission requiredPermission) const
{
    return containsPermission(globalPermissions(subject), requiredPermission);
}

bool QnGlobalPermissionsManager::canSeeAnotherUsers(const Qn::UserAccessData& accessRights) const
{
    return hasGlobalPermission(accessRights, nx::vms::api::GlobalPermission::admin);
}

bool QnGlobalPermissionsManager::hasGlobalPermission(const Qn::UserAccessData& accessRights,
    GlobalPermission requiredPermission) const
{
    if (accessRights == Qn::kSystemAccess)
        return true;

    auto user = resourcePool()->getResourceById<QnUserResource>(accessRights.userId);
    if (!user)
    {
        NX_VERBOSE(this, "User %1 is not found", accessRights.userId);
        return false;
    }
    return hasGlobalPermission(user, requiredPermission);
}

GlobalPermissions QnGlobalPermissionsManager::filterDependentPermissions(GlobalPermissions source) const
{
    // TODO: #sivanov Code duplication with ::dependentPermissions() method.
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
    NX_VERBOSE(this, "Update permissions for %1", subject);
    setGlobalPermissionsInternal(subject, calculateGlobalPermissions(subject));
}

GlobalPermissions QnGlobalPermissionsManager::calculateGlobalPermissions(
    const QnResourceAccessSubject& subject) const
{
    if (!subject.isValid())
    {
        NX_VERBOSE(this, "Invalid subject: %1", subject);
        return {};
    }

    const auto rolesPermissions =
        [this](const auto& roleIds)
        {
            GlobalPermissions result;
            for (const auto& role: m_context->userRolesManager()->userRolesWithParents(roleIds))
                result |= role.permissions;
            return filterDependentPermissions(result);
        };

    if (auto user = subject.user())
    {
        if (!user->isEnabled())
        {
            NX_VERBOSE(this, "%1 is disabled", user);
            return {};
        }

        if (user->flags().testFlag(Qn::local))
        {
            const auto result = filterDependentPermissions(user->getRawPermissions());
            NX_VERBOSE(this, "%1 just created with %2", user, result);
            return result;
        }

        if (!user->resourcePool())
        {
            // Problems with 'on_resource_removed' connection order.
            NX_VERBOSE(this, "%1 is not in pool", user);
            return {};
        }

        if (const auto r = user->userRole();
            r == Qn::UserRole::owner || r == Qn::UserRole::administrator)
        {
            NX_VERBOSE(this, "%1 is %2", user, r);
            return GlobalPermission::adminPermissions;
        }

        const auto raw = filterDependentPermissions(user->getRawPermissions());
        const auto roleIds = user->userRoleIds();
        const auto roles = rolesPermissions(roleIds);
        NX_VERBOSE(this, "%1 has raw %2 and roles %3 from %4",
            user, raw, roles, nx::containerString(roleIds));
        return raw | roles;
    }
    else //< Subject is a role.
    {
        auto result = rolesPermissions(std::vector{subject.id()});
        /* If user belongs to group, he cannot be an admin - by design. */
        result &= ~GlobalPermissions(GlobalPermission::admin);
        NX_VERBOSE(this, "Role %1 has %3", subject.id(), result);
        return result;
    }
}

void QnGlobalPermissionsManager::setGlobalPermissionsInternal(
    const QnResourceAccessSubject& subject, GlobalPermissions permissions)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
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

        connect(user.get(), &QnUserResource::permissionsChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
        connect(user.get(), &QnUserResource::userRolesChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
        connect(user.get(), &QnUserResource::enabledChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions);
    }
}

void QnGlobalPermissionsManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
}

void QnGlobalPermissionsManager::handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole)
{
    updateGlobalPermissions(userRole);
    for (auto subject: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(userRole.id))
        updateGlobalPermissions(subject);
}

void QnGlobalPermissionsManager::handleRoleRemoved(const nx::vms::api::UserRoleData& userRole)
{
    handleSubjectRemoved(userRole);
    for (auto subject: m_context->resourceAccessSubjectsCache()->allSubjectsInRole(userRole.id))
        updateGlobalPermissions(subject);
}

void QnGlobalPermissionsManager::handleSubjectRemoved(const QnResourceAccessSubject& subject)
{
    auto id = subject.id();
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        NX_ASSERT(m_cache.contains(id));
        m_cache.remove(id);
    }
    emit globalPermissionsChanged(subject, {});
}
