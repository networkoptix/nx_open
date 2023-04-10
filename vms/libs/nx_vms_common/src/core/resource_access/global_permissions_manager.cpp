// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "global_permissions_manager.h"

#include <common/common_module.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/utils/log/log.h>

using namespace nx::core::access;
using namespace nx::vms::common;

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
            &QnGlobalPermissionsManager::handleResourceAdded, Qt::DirectConnection);
        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            &QnGlobalPermissionsManager::handleResourceRemoved, Qt::DirectConnection);

        connect(m_context->userGroupManager(), &UserGroupManager::addedOrUpdated, this,
            &QnGlobalPermissionsManager::handleGroupAddedOrUpdated, Qt::DirectConnection);
        connect(m_context->userGroupManager(), &UserGroupManager::removed, this,
            &QnGlobalPermissionsManager::handleGroupRemoved, Qt::DirectConnection);
        connect(m_context->userGroupManager(), &UserGroupManager::reset, this,
            &QnGlobalPermissionsManager::handleGroupReset, Qt::DirectConnection);

        connect(m_context->accessSubjectHierarchy(), &SubjectHierarchy::changed, this,
            &QnGlobalPermissionsManager::handleHierarchyChanged, Qt::DirectConnection);
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

        QSet<QnUuid> subjects{{subject.id()}};
        subjects += nx::vms::common::userGroupsWithParents(systemContext(), subjects);

        for (const auto& effectiveId: subjects)
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
    return hasGlobalPermission(accessRights, nx::vms::api::GlobalPermission::powerUser);
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

    const auto permissionsFromGroups =
        [this](const auto& groupIds)
        {
            const auto allGroupIds = userGroupsWithParents(m_context, groupIds);
            const auto allGroups = m_context->userGroupManager()->getGroupsByIds(allGroupIds);

            GlobalPermissions result;
            for (const auto& group: allGroups)
                result |= group.permissions;

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

        if (user->isAdministrator() || user->getRawPermissions().testFlag(GlobalPermission::powerUser))
        {
            NX_VERBOSE(this, "%1 is a power user", user);
            return GlobalPermission::powerUserPermissions;
        }

        const auto raw = filterDependentPermissions(user->getRawPermissions());
        const auto groupIds = user->groupIds();
        const auto fromGroups = permissionsFromGroups(groupIds);
        NX_VERBOSE(this, "%1 has raw %2 and groups %3 from %4",
            user, raw, fromGroups, nx::containerString(groupIds));
        return raw | fromGroups;
    }
    else //< Subject is a role.
    {
        auto result = permissionsFromGroups(std::vector{subject.id()});
        /* If user belongs to group, he cannot be an admin - by design. */
        result &= ~GlobalPermissions(GlobalPermission::powerUser);
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
            &QnGlobalPermissionsManager::updateGlobalPermissions, Qt::DirectConnection);
        connect(user.get(), &QnUserResource::userGroupsChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions, Qt::DirectConnection);
        connect(user.get(), &QnUserResource::enabledChanged, this,
            &QnGlobalPermissionsManager::updateGlobalPermissions, Qt::DirectConnection);
    }
}

void QnGlobalPermissionsManager::handleResourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        handleSubjectRemoved(user);
}

void QnGlobalPermissionsManager::handleGroupAddedOrUpdated(
    const nx::vms::api::UserGroupData& userGroup)
{
    updateGlobalPermissions(userGroup);
}

void QnGlobalPermissionsManager::handleGroupRemoved(
    const nx::vms::api::UserGroupData& userGroup)
{
    handleSubjectRemoved(userGroup);
}

void QnGlobalPermissionsManager::handleGroupReset()
{
    for (const auto& group: m_context->userGroupManager()->groups())
        handleGroupAddedOrUpdated(group);
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

void QnGlobalPermissionsManager::handleHierarchyChanged(
    const QSet<QnUuid>& /*added*/,
    const QSet<QnUuid>& /*removed*/,
    const QSet<QnUuid>& /*groupsWithChangedMembers*/,
    const QSet<QnUuid>& subjectsWithChangedParents)
{
    const auto recursivelyChanged = m_context->accessSubjectHierarchy()->recursiveMembers(
        subjectsWithChangedParents) + subjectsWithChangedParents;

    for (const auto& id: recursivelyChanged)
        updateGlobalPermissions(m_context->accessSubjectHierarchy()->subjectById(id));
}
