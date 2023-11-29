// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "non_editable_users_and_groups.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
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
            connect(user.get(), &QnUserResource::enabledChanged, this, updateUser);
            connect(user.get(), &QnUserResource::nameChanged, this, updateUser);

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

                    if (m_nonUniqueUserTracker.remove(user->getId()))
                        emit nonUniqueUsersChanged();

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
            if (m_nonUniqueGroupTracker.remove(group.id))
                emit nonUniqueGroupsChanged();
            removeGroup(group.id);
        });

    connect(manager, &nx::vms::common::UserGroupManager::reset, this,
        [this, manager]()
        {
            const auto groupsCopy = m_nonEditableGroups.keys();
            for (const auto& id: groupsCopy)
            {
                if (!nx::vms::common::PredefinedUserGroups::contains(id))
                {
                    if (m_nonUniqueGroupTracker.remove(id))
                        emit nonUniqueGroupsChanged();

                    removeGroup(id);
                }
            }

            for (const auto& group: manager->groups())
                addOrUpdateGroup(group);
        },
        Qt::QueuedConnection); //< When using direct connection users are not loaded yet.

    for (const auto& group: manager->groups())
        addOrUpdateGroup(group);
}

NonEditableUsersAndGroups::~NonEditableUsersAndGroups()
{}

void NonEditableUsersAndGroups::modifyGroups(const QSet<QnUuid>& ids, int diff)
{
    for (const auto& id: ids)
    {
        const auto count = m_groupsWithNonEditableMembers.value(id, 0) + diff;

        if (count <= 0)
        {
            m_groupsWithNonEditableMembers.remove(id);
            if (!m_nonRemovableGroups.contains(id))
                emit groupModified(id);
        }
        else
        {
            m_groupsWithNonEditableMembers[id] = count;
            if (diff == 1 && count == 1 && !m_nonRemovableGroups.contains(id))
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

bool NonEditableUsersAndGroups::canEditParents(const QnUuid& id) const
{
    if (const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(id))
    {
        if (user->attributes().testFlag(api::UserAttribute::readonly))
            return false;
        if (!accessController()->hasPermissions(user, Qn::WriteAccessRightsPermission))
            return false;
    }

    return !m_usersWithUnchangableParents.contains(id) && m_nonUniqueGroupTracker.isUnique(id);
}

bool NonEditableUsersAndGroups::canMassEdit(const QnUserResourcePtr& user) const
{
    // Do not allow mass editing to be used with Administrators.
    if (user->isAdministrator())
        return false;

    if (user->attributes().testFlag(api::UserAttribute::readonly))
        return false;

    return canDelete(user)
        || canEnableDisable(user)
        || canChangeAuthentication(user);
}

QSet<QnUuid> NonEditableUsersAndGroups::nonUniqueWithEditableParents()
{
    const auto nonUniqueIds = m_nonUniqueUserTracker.nonUniqueNameIds();
    QSet<QnUuid> result;

    for (const QnUuid& nonUniqueId: nonUniqueIds)
    {
        const auto nonUniqueUser =
            systemContext()->resourcePool()->getResourceById<QnUserResource>(nonUniqueId);

        if (!NX_ASSERT(nonUniqueUser))
            continue;

        const auto idsWithTheSameName =
            m_nonUniqueUserTracker.idsByName(nonUniqueUser->getName().toLower());

        QSet<QnUuid> enabled;

        if (idsWithTheSameName.size() > 1)
        {
            for (const auto& id: idsWithTheSameName)
            {
                if (const auto user =
                    systemContext()->resourcePool()->getResourceById<QnUserResource>(id))
                {
                    if (user->isEnabled())
                        enabled << id;
                }
            }
        }

        const bool canEditParents = idsWithTheSameName.size() == 1 || enabled.size() < 2;

        if (canEditParents)
            result << nonUniqueId;
    }

    return result;
}

void NonEditableUsersAndGroups::addOrUpdateUser(const QnUserResourcePtr& user)
{
    const auto prevMassEditableSize = m_nonMassEditableUsers.size();
    if (canMassEdit(user))
        m_nonMassEditableUsers.remove(user);
    else
        m_nonMassEditableUsers.insert(user);

    const bool emitUserModified = prevMassEditableSize != m_nonMassEditableUsers.size();

    if (m_nonUniqueUserTracker.update(user->getId(), user->getName().toLower()))
        emit nonUniqueUsersChanged();

    const auto nonUniqueIds = m_nonUniqueUserTracker.nonUniqueNameIds();
    const auto newUsersWithUnchangableParents = nonUniqueIds - nonUniqueWithEditableParents();

    const auto added = newUsersWithUnchangableParents - m_usersWithUnchangableParents;
    const auto removed = m_usersWithUnchangableParents - newUsersWithUnchangableParents;

    m_usersWithUnchangableParents = newUsersWithUnchangableParents;

    QSet<QnUuid> changedUsers = added + removed;
    changedUsers << user->getId();

    for (const QnUuid& changedUserId: changedUsers)
    {
        const auto changedUser =
            systemContext()->resourcePool()->getResourceById<QnUserResource>(changedUserId);

        const bool userIsNonEditable =
            m_nonMassEditableUsers.contains(changedUser)
            || m_usersWithUnchangableParents.contains(changedUserId);

        const bool modified = userIsNonEditable
            ? addUser(changedUser)
            : removeUser(changedUser);

        if (user == changedUser && !modified && emitUserModified)
            emit userModified(user);
    }
}

void NonEditableUsersAndGroups::addOrUpdateGroup(const nx::vms::api::UserGroupData& group)
{
    const auto prevMassEditableSize = m_nonRemovableGroups.size();
    if (canDelete(group))
        m_nonRemovableGroups.remove(group.id);
    else
        m_nonRemovableGroups.insert(group.id);

    const bool emitGroupModified = prevMassEditableSize != m_nonRemovableGroups.size();

    const auto prevNonUniqueGroups = m_nonUniqueGroupTracker.nonUniqueNameIds();

    if (m_nonUniqueGroupTracker.update(group.id, group.name.toLower()))
        emit nonUniqueGroupsChanged();

    const auto nonUniqueGroups = m_nonUniqueGroupTracker.nonUniqueNameIds();
    const auto added = nonUniqueGroups - prevNonUniqueGroups;
    const auto removed = prevNonUniqueGroups - nonUniqueGroups;

    QSet<QnUuid> changedGroups = added + removed;
    changedGroups << group.id;

    for (const auto& changedGroupId: changedGroups)
    {
        const bool groupIsNonEditable =
            m_nonRemovableGroups.contains(changedGroupId)
            || !m_nonUniqueGroupTracker.isUnique(changedGroupId);

        const bool modified = groupIsNonEditable
            ? addGroup(changedGroupId)
            : removeGroup(changedGroupId);

        if (changedGroupId == group.id && !modified && emitGroupModified)
            emit groupModified(group.id);
    }
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

bool NonEditableUsersAndGroups::addUser(const QnUserResourcePtr& user)
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

    return newUser;
}

bool NonEditableUsersAndGroups::removeUser(const QnUserResourcePtr& user)
{
    if (!m_nonEditableUsers.contains(user))
        return false;

    const auto ids = m_nonEditableUsers.value(user);
    m_nonEditableUsers.remove(user);
    m_nonMassEditableUsers.remove(user);
    m_usersWithUnchangableParents.remove(user->getId());

    emit userModified(user);

    modifyGroups(ids, -1);
    return true;
}

qsizetype NonEditableUsersAndGroups::userCount() const
{
    return m_nonMassEditableUsers.size();
}

bool NonEditableUsersAndGroups::containsUser(const QnUserResourcePtr& user) const
{
    return m_nonMassEditableUsers.contains(user);
}

bool NonEditableUsersAndGroups::addGroup(const QnUuid& id)
{
    const auto group = systemContext()->userGroupManager()->find(id);
    if (!group)
        return false;

    const QSet<QnUuid> groupIds(group->parentGroupIds.begin(), group->parentGroupIds.end());

    const bool newGroup = !m_nonEditableGroups.contains(id);

    const QSet<QnUuid> prevGroupIds = m_nonEditableGroups.value(id);

    auto removed = prevGroupIds - groupIds;
    auto added = groupIds - prevGroupIds;

    const auto isLdap =
        [this](const QnUuid& id)
        {
            const auto group = systemContext()->userGroupManager()->find(id);
            return group && group->type == api::UserType::ldap;
        };

    // Non-editable LDAP groups do not prevent their LDAP parents from removal.
    if (group->type == api::UserType::ldap)
    {
        erase_if(removed, isLdap);
        erase_if(added, isLdap);
    }

    modifyGroups(removed, -1);
    modifyGroups(added, 1);

    m_nonEditableGroups[id] = groupIds;

    if (!newGroup)
        return false;

    if (!m_groupsWithNonEditableMembers.contains(id))
        emit groupModified(id);

    updateMembers(id);
    return true;
}

bool NonEditableUsersAndGroups::removeGroup(const QnUuid& id)
{
    if (!m_nonEditableGroups.contains(id))
        return false;

    const auto parentIds = m_nonEditableGroups.value(id);

    m_nonEditableGroups.remove(id);
    m_nonRemovableGroups.remove(id);

    if (!m_groupsWithNonEditableMembers.contains(id))
        emit groupModified(id);

    modifyGroups(parentIds, -1);

    updateMembers(id);
    return true;
}

bool NonEditableUsersAndGroups::containsGroup(const QnUuid& groupId) const
{
    return m_nonRemovableGroups.contains(groupId)
        || m_groupsWithNonEditableMembers.contains(groupId);
}

QSet<QnUuid> NonEditableUsersAndGroups::groups() const
{
    return m_nonRemovableGroups + nx::utils::toQSet(m_groupsWithNonEditableMembers.keys());
}

qsizetype NonEditableUsersAndGroups::groupCount() const
{
    return groups().size();
}

QString NonEditableUsersAndGroups::tooltip(const QnUuid& id) const
{
    if (const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(id))
    {
        if (user->attributes().testFlag(api::UserAttribute::readonly))
        {
            return tr("User management for organization users is available only at the "
                "organization level, not the system level");
        }
        if (!canEnableDisable(user))
            return tr("You do not have permissions to modify this user");
        if (!m_nonUniqueUserTracker.isUnique(id))
            return tr("You cannot modify a user with a non-unique login");
    }
    else if (m_groupsWithNonEditableMembers.contains(id))
    {
        return tr("You may not have permissions to modify certain members of this group, or it "
            "includes users with duplicate logins");
    }
    else if (m_nonEditableGroups.contains(id))
    {
        return tr("You do not have permissions to modify this group");
    }
    return {};
}

} // namespace nx::vms::client::desktop
