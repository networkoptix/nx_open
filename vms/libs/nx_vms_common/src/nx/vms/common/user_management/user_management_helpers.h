// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <QtCore/QSet>

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::common {

template<class IdList>
void getUsersAndGroups(
    SystemContext* systemContext,
    const IdList& idList,
    QnUserResourceList& users,
    nx::vms::api::UserGroupDataList& groups)
{
    if (!NX_ASSERT(systemContext))
        return;

    users = systemContext->resourcePool()->getResourcesByIds<QnUserResource>(idList);
    groups = systemContext->userGroupManager()->getGroupsByIds(idList);
}

template<class IdList>
void getUsersAndGroups(
    SystemContext* systemContext,
    const IdList& idList,
    QnUserResourceList& users,
    QSet<nx::Uuid>& groupIds)
{
    nx::vms::api::UserGroupDataList groups;
    getUsersAndGroups(systemContext, idList, users, groups);

    groupIds = {};
    groupIds.reserve(groups.size());

    for (const auto& group: groups)
        groupIds.insert(group.id);
}

template<class IdList>
QnUserResourceSet allUsers(SystemContext* context, const IdList& ids)
{
    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;
    nx::vms::common::getUsersAndGroups(context, ids, users, groupIds);

    auto result = nx::utils::toQSet(users);

    const auto groupUsers = context->accessSubjectHierarchy()->usersInGroups(groupIds);

    for (const auto& user: groupUsers)
        result.insert(user);

    return result;
}

template<class IdList>
bool allUserGroupsExist(SystemContext* systemContext, const IdList& groupIds)
{
    if (!NX_ASSERT(systemContext))
        return false;

    const auto manager = systemContext->userGroupManager();
    return std::all_of(groupIds.begin(), groupIds.end(),
        [manager](const auto& id) { return manager->contains(id); });
}

template<class IdList, class LessFunc>
QStringList userGroupNames(
    const SystemContext* systemContext,
    const IdList& groupIds,
    LessFunc lessFunc)
{
    if (!NX_ASSERT(systemContext))
        return {};

    auto groups = systemContext->userGroupManager()->getGroupsByIds(groupIds);

    std::sort(groups.begin(), groups.end(), lessFunc);

    QStringList result;
    for (const auto& group: groups)
        result.push_back(group.name);

    return result;
}

template<class IdList>
QStringList userGroupNames(const SystemContext* systemContext, const IdList& groupIds)
{
    if (!NX_ASSERT(systemContext))
        return {};

    QStringList result;
    for (const auto& group: systemContext->userGroupManager()->getGroupsByIds(groupIds))
        result.push_back(group.name);

    return result;
}

inline QStringList userGroupNames(const QnUserResourcePtr& user)
{
    return user && user->systemContext()
        ? userGroupNames(user->systemContext(), user->groupIds())
        : QStringList{};
}

template<class IdList>
QSet<nx::Uuid> userGroupsWithParents(SystemContext* systemContext, const IdList& groupIds)
{
    QSet<nx::Uuid> result{groupIds.begin(), groupIds.end()};
    if (!NX_ASSERT(systemContext))
        return result;

    result += systemContext->accessSubjectHierarchy()->recursiveParents(result);
    return result;
}

inline QSet<nx::Uuid> userGroupsWithParents(const QnUserResourcePtr& user)
{
    return userGroupsWithParents(user->systemContext(), user->groupIds());
}

} // namespace nx::vms::common
