// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_editable_users_and_groups.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

NonEditableUsersAndGroups::NonEditableUsersAndGroups(SystemContext* systemContext):
    base_type(systemContext)
{
    const auto addResource =
        [this](const QnUserResourcePtr& user)
        {
            const auto updateUser =
                [this](const QnResourcePtr& resource)
                {
                    auto user = resource.dynamicCast<QnUserResource>();
                    if (!NX_ASSERT(user))
                        return;

                    addOrUpdateUser(user);
                };

            connect(user.get(), &QnUserResource::permissionsChanged, this, updateUser);
            connect(user.get(), &QnUserResource::userGroupsChanged, this, updateUser);

            addOrUpdateUser(user);
        };

    connect(systemContext->resourcePool(), &QnResourcePool::resourcesAdded, this,
        [addResource](const QnResourceList &resources)
        {
            for (const auto& resource: resources)
            {
                if (auto user = resource.dynamicCast<QnUserResource>())
                    addResource(user);
            }
        });

    connect(systemContext->resourcePool(), &QnResourcePool::resourcesRemoved, this,
        [this](const QnResourceList &resources)
        {
            for (const auto& resource: resources)
            {
                if (auto user = resource.dynamicCast<QnUserResource>())
                {
                    disconnect(user.get(), nullptr, this, nullptr);
                    removeUser(user);
                }
            }
        });

    for (const auto& user: systemContext->resourcePool()->getResources<QnUserResource>())
        addResource(user);

    auto manager = systemContext->userGroupManager();

    connect(manager, &nx::vms::common::UserGroupManager::addedOrUpdated, this,
        [this](const nx::vms::api::UserGroupData& group)
        {
            addOrUpdateGroup(group);
        });

    connect(manager, &nx::vms::common::UserGroupManager::removed, this,
        [this](const nx::vms::api::UserGroupData& group)
        {
            removeGroup(group.id);
        });

    connect(manager, &nx::vms::common::UserGroupManager::reset, this,
        [this]()
        {
            const auto groupsCopy = m_nonEditableGroups.keys();
            for (const auto& id: groupsCopy)
            {
                if (!nx::vms::common::PredefinedUserGroups::contains(id))
                    removeGroup(id);
            }
        });

    for (const auto& group: manager->groups())
        addOrUpdateGroup(group);
}

NonEditableUsersAndGroups::~NonEditableUsersAndGroups()
{}

void NonEditableUsersAndGroups::modifyGroups(const QSet<QnUuid> ids, int diff)
{
    for (const auto& id: ids)
    {
        const auto count = m_groupsWithNonEditableMembers.value(id, 0) + diff;

        if (count <= 0)
        {
            m_groupsWithNonEditableMembers.remove(id);
            if (!m_nonEditableGroups.contains(id))
                emit groupModified(id);
        }
        else
        {
            m_groupsWithNonEditableMembers[id] = count;
            if (diff == 1 && count == 1 && !m_nonEditableGroups.contains(id))
                emit groupModified(id);
        }
    }
}

bool NonEditableUsersAndGroups::canDelete(const nx::vms::api::UserGroupData& group)
{
    return systemContext()->accessController()->hasPermissions(group.id, Qn::RemovePermission);
}

bool NonEditableUsersAndGroups::canDelete(const QnUserResourcePtr& user) const
{
    return accessController()->hasPermissions(user, Qn::RemovePermission);
}

bool NonEditableUsersAndGroups::canEnableDisable(const QnUserResourcePtr& user) const
{
    return accessController()->hasPermissions(user,
        Qn::WritePermission | Qn::WriteAccessRightsPermission | Qn::SavePermission);
}

bool NonEditableUsersAndGroups::canChangeAuthentication(const QnUserResourcePtr& user) const
{
    return accessController()->hasPermissions(user,
        Qn::WritePermission | Qn::WriteDigestPermission | Qn::SavePermission);
}


bool NonEditableUsersAndGroups::canEdit(const QnUserResourcePtr& user)
{
    // Do not allow mass editing to be used with Administrators.
    if (user->isAdministrator())
        return false;

    return canDelete(user)
        || canEnableDisable(user)
        || canChangeAuthentication(user);
}

void NonEditableUsersAndGroups::addOrUpdateUser(const QnUserResourcePtr& user)
{
    if (canEdit(user))
        removeUser(user);
    else
        addUser(user);
}

void NonEditableUsersAndGroups::addOrUpdateGroup(const nx::vms::api::UserGroupData& group)
{
    if (canDelete(group))
        removeGroup(group.id);
    else
        addGroup(group.id);
}

void NonEditableUsersAndGroups::updateMembers(const QnUuid& groupId)
{
    const QSet<QnUuid> members =
        systemContext()->accessSubjectHierarchy()->recursiveMembers({groupId});

    const auto context = systemContext();

    for (const auto& id: members)
    {
        if (const auto group = context->userGroupManager()->find(id))
            addOrUpdateGroup(*group);
        else if (const auto user = context->resourcePool()->getResourceById<QnUserResource>(id))
            addOrUpdateUser(user);
    }
}

void NonEditableUsersAndGroups::addUser(const QnUserResourcePtr& user)
{
    const auto ids = user->groupIds();
    QSet<QnUuid> parentIds(ids.begin(), ids.end());

    const bool newUser = !m_nonEditableUsers.contains(user);

    const QSet<QnUuid> prevGroupIds = m_nonEditableUsers.value(user);

    const auto removed = prevGroupIds - parentIds;
    const auto added = parentIds - prevGroupIds;

    modifyGroups(removed, -1);
    modifyGroups(added, 1);

    m_nonEditableUsers[user] = parentIds;

    if (newUser)
        emit userModified(user);
}

void NonEditableUsersAndGroups::removeUser(const QnUserResourcePtr& user)
{
    if (!m_nonEditableUsers.contains(user))
        return;

    const auto ids = m_nonEditableUsers.value(user);
    m_nonEditableUsers.remove(user);
    emit userModified(user);

    modifyGroups(ids, -1);
}

qsizetype NonEditableUsersAndGroups::userCount() const
{
    return m_nonEditableUsers.size();
}

bool NonEditableUsersAndGroups::containsUser(const QnUserResourcePtr& user) const
{
    return m_nonEditableUsers.contains(user);
}

void NonEditableUsersAndGroups::addGroup(const QnUuid& id)
{
    const auto group = systemContext()->userGroupManager()->find(id);
    if (!NX_ASSERT(group))
        return;

    const QSet<QnUuid> groupIds(group->parentGroupIds.begin(), group->parentGroupIds.end());

    const bool newGroup = !m_nonEditableGroups.contains(id);

    const QSet<QnUuid> prevGroupIds = m_nonEditableGroups.value(id);

    const auto removed = prevGroupIds - groupIds;
    const auto added = groupIds - prevGroupIds;

    modifyGroups(removed, -1);
    modifyGroups(added, 1);

    m_nonEditableGroups[id] = groupIds;

    if (!newGroup)
        return;

    if (!m_groupsWithNonEditableMembers.contains(id))
        emit groupModified(id);

    updateMembers(id);
}

void NonEditableUsersAndGroups::removeGroup(const QnUuid& id)
{
    if (!m_nonEditableGroups.contains(id))
        return;

    const auto parentIds = m_nonEditableGroups.value(id);

    m_nonEditableGroups.remove(id);

    if (!m_groupsWithNonEditableMembers.contains(id))
        emit groupModified(id);

    modifyGroups(parentIds, -1);

    updateMembers(id);
}

bool NonEditableUsersAndGroups::containsGroup(const QnUuid& groupId) const
{
    return m_nonEditableGroups.contains(groupId)
        || m_groupsWithNonEditableMembers.contains(groupId);
}

QSet<QnUuid> NonEditableUsersAndGroups::groups() const
{
    const auto listsOfGroupIds = {
        m_nonEditableGroups.keys(),
        m_groupsWithNonEditableMembers.keys()
    };

    QSet<QnUuid> result;

    for (const auto& ids: listsOfGroupIds)
        result += QSet(ids.begin(), ids.end());

    return result;
}

qsizetype NonEditableUsersAndGroups::groupCount() const
{
    return groups().size();
}

} // namespace nx::vms::client::desktop
