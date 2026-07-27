// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>

#include <QtCore/QSet>

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/uuid.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::common {

inline bool isUserHidden(const QnUserResourcePtr& user)
{
    return !NX_ASSERT(user) || user->attributes().testFlag(nx::vms::api::UserAttribute::hidden);
}

inline bool isGroupHidden(const nx::vms::api::UserGroupData& group)
{
    return group.attributes.testFlag(nx::vms::api::UserAttribute::hidden);
}

template<typename IdList>
void getUsersAndGroups(
    const QnResourcePool* resourcePool,
    const UserGroupManager* userGroupManager,
    const IdList& idList,
    QnUserResourceList& users,
    nx::vms::api::UserGroupDataList& groups,
    bool includeHidden = true)
{
    if (!NX_ASSERT(resourcePool) || !NX_ASSERT(userGroupManager))
        return;

    users = resourcePool->getResourcesByIds<QnUserResource>(idList);
    groups = userGroupManager->getGroupsByIds(idList);

    if (!includeHidden)
    {
        nx::utils::erase_if(users, &isUserHidden);
        nx::utils::erase_if(groups, &isGroupHidden);
    }
}

template<typename IdList, typename GroupIdSet>
void getUsersAndGroups(
    const QnResourcePool* resourcePool,
    const UserGroupManager* userGroupManager,
    const IdList& idList,
    QnUserResourceList& users,
    GroupIdSet& groupIds,
    bool includeHidden = true)
{
    nx::vms::api::UserGroupDataList groups;
    getUsersAndGroups(resourcePool, userGroupManager, idList, users, groups, includeHidden);

    groupIds.clear();
    for (const auto& group: groups)
        groupIds.insert(group.id);
}

template<typename IdList, typename UserIdSet, typename GroupIdSet>
void getUsersAndGroups(
    const QnResourcePool* resourcePool,
    const UserGroupManager* userGroupManager,
    const IdList& idList,
    UserIdSet& userIds,
    GroupIdSet& groupIds,
    bool includeHidden = true)
{
    QnUserResourceList users;
    getUsersAndGroups(resourcePool, userGroupManager, idList, users, groupIds, includeHidden);

    userIds.clear();
    for (const auto& user: users)
        userIds.insert(user->getId());
}

template<typename IdList>
QnUserResourceSet allUsers(const SystemContext* context, const IdList& ids)
{
    QnUserResourceList users;
    QSet<nx::Uuid> groupIds;
    nx::vms::common::getUsersAndGroups(context->resourcePool(), context->userGroupManager(), ids, users, groupIds);

    auto result = nx::utils::toQSet(users);

    const auto groupUsers = context->accessSubjectHierarchy()->usersInGroups(groupIds);

    for (const auto& user: groupUsers)
        result.insert(user);

    return result;
}

template<typename IdList>
bool allUserGroupsExist(const UserGroupManager* userGroupManager, const IdList& groupIds)
{
    if (!NX_ASSERT(userGroupManager))
        return false;

    return std::ranges::all_of(
        groupIds,
        [userGroupManager](const auto& id) { return userGroupManager->contains(id); });
}

template<typename IdList, typename LessFunc>
QStringList userGroupNames(
    const UserGroupManager* userGroupManager,
    const IdList& groupIds,
    LessFunc lessFunc)
{
    if (!NX_ASSERT(userGroupManager))
        return {};

    auto groups = userGroupManager->getGroupsByIds(groupIds);

    std::ranges::sort(groups, lessFunc);

    QStringList result;
    for (const auto& group: groups)
        result.push_back(group.name);

    return result;
}

template<typename IdList>
QStringList userGroupNames(const UserGroupManager* userGroupManager, const IdList& groupIds)
{
    if (!NX_ASSERT(userGroupManager))
        return {};

    QStringList result;
    for (const auto& group: userGroupManager->getGroupsByIds(groupIds))
        result.push_back(group.name);

    return result;
}

inline QStringList userGroupNames(const QnUserResourcePtr& user)
{
    return user && user->systemContext()
        ? userGroupNames(user->systemContext()->userGroupManager(), user->allGroupIds())
        : QStringList{};
}

template<typename IdList>
QSet<nx::Uuid> userGroupsWithParents(
    nx::core::access::ResourceAccessSubjectHierarchy* accessSubjectHierarchy,
    const IdList& groupIds)
{
    QSet<nx::Uuid> result{groupIds.begin(), groupIds.end()};
    if (!NX_ASSERT(accessSubjectHierarchy))
        return result;

    result += accessSubjectHierarchy->recursiveParents(result);
    return result;
}

inline QSet<nx::Uuid> userGroupsWithParents(const QnUserResourcePtr& user)
{
    return userGroupsWithParents(
        user->systemContext()->accessSubjectHierarchy(), user->allGroupIds());
}

} // namespace nx::vms::common
