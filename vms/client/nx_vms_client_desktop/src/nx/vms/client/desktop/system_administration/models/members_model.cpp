// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "members_model.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtQml/QtQml>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/system_administration/watchers/non_editable_users_and_groups.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "../globals/user_settings_global.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

static const QSet<nx::Uuid> kRestrictedForTemp = {
    nx::vms::api::kAdministratorsGroupId,
    nx::vms::api::kPowerUsersGroupId
};

} // namespace

MembersModelGroup MembersModelGroup::fromId(SystemContext* systemContext, const nx::Uuid& id)
{
    const auto group = systemContext->userGroupManager()->find(id).value_or(api::UserGroupData{});

    return {
        .id = group.id,
        .text = group.name,
        .description = group.description,
        .isLdap = (group.type == nx::vms::api::UserType::ldap),
        .isPredefined = group.attributes.testFlag(nx::vms::api::UserAttribute::readonly)};
}

MembersModel::MembersModel(SystemContext* systemContext):
    SystemContextAware(systemContext)
{
}

MembersModel::MembersModel():
    SystemContextAware(appContext()->currentSystemContext())
{
}

MembersModel::~MembersModel()
{
}

int MembersModel::customGroupCount() const
{
    if (!m_cache)
        return 0;

    const auto stats = m_cache->stats();

    return stats.localGroups + stats.cloudGroups;
}

void MembersModel::loadModelData()
{
    beginResetModel();
    m_cache->loadInfo(systemContext());
    m_cache->sorted(m_subjectId); //< Force storage to report changes for current subject.
    endResetModel();

    checkCycles();

    emit customGroupCountChanged();

    emit parentGroupsChanged();
    emit usersChanged();
    emit groupsChanged();
    emit globalPermissionsChanged();
    emit sharedResourcesChanged();
}

void addToSorted(QList<nx::Uuid>& list, const nx::Uuid& id)
{
    auto it = std::lower_bound(list.begin(), list.end(), id);
    if (it != list.end() && *it == id)
        return;

    list.insert(it, id);
}

void removeFromSorted(QList<nx::Uuid>& list, const nx::Uuid& id)
{
    auto it = std::lower_bound(list.begin(), list.end(), id);

    if (it == list.end() || *it != id)
        return;

    list.erase(it);
}

void MembersModel::subscribeToUser(const QnUserResourcePtr& user)
{
    const auto updateUser =
        [this](const nx::Uuid& id)
        {
            m_cache->modify({id}, {id}, {}, {});
        };

    nx::utils::ScopedConnections userConnections;

    userConnections << connect(user.get(), &QnResource::nameChanged, this,
        [updateUser](const QnResourcePtr& resource)
        {
            updateUser(resource->getId());
        });

    userConnections << connect(user.get(), &QnUserResource::userGroupsChanged, this,
        [updateUser](
            const QnUserResourcePtr& user)
        {
            updateUser(user->getId());
        });

    m_userConnections[user->getId()] = std::move(userConnections);
}

void MembersModel::unsubscribeFromUser(const QnUserResourcePtr& user)
{
    m_userConnections.erase(user->getId());
}

void MembersModel::checkCycles()
{
    if (m_subjectId.isNull())
        return;

    // Check direct parents.
    QSet<nx::Uuid> groupsWithCycles;
    const auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    for (const auto& parent: parents)
    {
        if (m_subjectContext->subjectHierarchy()->recursiveParents({parent}).contains(m_subjectId))
            groupsWithCycles.insert(parent);
    }

    const auto allParents = m_subjectContext->subjectHierarchy()->recursiveParents({m_subjectId});

    // Check direct member groups.
    for (const auto& id: m_subjectMembers.groups)
    {
        if (allParents.contains(id))
            groupsWithCycles.insert(id);
    }

    if (groupsWithCycles == m_groupsWithCycles)
        return;

    // Notify about groups that (maybe no longer) cause cycles.
    const auto modifiedGroups = m_groupsWithCycles + groupsWithCycles;

    const bool cycledChanged = m_groupsWithCycles.isEmpty() != groupsWithCycles.isEmpty();
    m_groupsWithCycles = groupsWithCycles;
    if (cycledChanged)
        emit cycledGroupChanged();

    for (const auto& id: modifiedGroups)
    {
        const auto row = m_cache->sorted().users.size()
            + m_cache->indexIn(m_cache->sorted().groups, id);
        emit dataChanged(index(row, 0), index(row, 0));
    }
}

void MembersModel::readUsersAndGroups()
{
    const auto resourcePool = systemContext()->resourcePool();
    const auto userGroupManager = systemContext()->userGroupManager();

    if (!m_subjectContext && !m_subjectId.isNull())
    {
        m_subjectContext.reset(new AccessSubjectEditingContext(systemContext()));

        m_subjectContext->setCurrentSubject(m_subjectId, m_subjectIsUser
            ? AccessSubjectEditingContext::SubjectType::user
            : AccessSubjectEditingContext::SubjectType::group);

        emit editingContextChanged();

        m_cache.reset(new MembersCache());
        m_cache->setContext(m_subjectContext.get());
        emit membersCacheChanged();

        connect(m_cache.get(), &MembersCache::beginInsert, this,
            [this](int at, const nx::Uuid& id, const nx::Uuid& parentId)
            {
                if (!parentId.isNull())
                {
                    checkCycles();
                    return;
                }

                const int row = m_cache->info(id).isGroup
                    ? m_cache->sorted().users.size() + at
                    : at;

                this->beginInsertRows({}, row, row);
            });

        connect(m_cache.get(), &MembersCache::endInsert, this,
            [this](int, const nx::Uuid& id, const nx::Uuid& parentId)
            {
                if (parentId.isNull())
                {
                    this->endInsertRows();
                    return;
                }

                checkCycles();

                if (parentId == m_subjectId)
                {
                    const bool isGroup = m_cache->info(id).isGroup;
                    const auto& members = isGroup
                        ? m_cache->sorted().groups
                        : m_cache->sorted().users;

                    const auto row = m_cache->indexIn(members, id)
                        + (isGroup ? m_cache->sorted().users.size() : 0);

                    emit dataChanged(index(row, 0), index(row, 0));

                    addToSorted(isGroup ? m_subjectMembers.groups : m_subjectMembers.users, id);
                    if (isGroup)
                        emit groupsChanged();
                    else
                        emit usersChanged();
                }
            });

        connect(m_cache.get(), &MembersCache::beginRemove, this,
            [this](int at, const nx::Uuid& id, const nx::Uuid& parentId)
            {
                if (!parentId.isNull())
                {
                    checkCycles();
                    return;
                }

                const int row = m_cache->info(id).isGroup
                    ? m_cache->sorted().users.size() + at
                    : at;

                this->beginRemoveRows({}, row, row);
            });

        connect(m_cache.get(), &MembersCache::endRemove, this,
            [this](int, const nx::Uuid& id, const nx::Uuid& parentId)
            {
                if (parentId.isNull())
                {
                    this->endRemoveRows();
                    return;
                }

                checkCycles();

                if (parentId == m_subjectId)
                {
                    const bool isGroup = m_cache->info(id).isGroup;
                    const auto& members = isGroup
                        ? m_cache->sorted().groups
                        : m_cache->sorted().users;

                    const auto row = m_cache->indexIn(members, id)
                        + (isGroup ? m_cache->sorted().users.size() : 0);

                    emit dataChanged(index(row, 0), index(row, 0));

                    removeFromSorted(isGroup ? m_subjectMembers.groups : m_subjectMembers.users, id);
                    if (isGroup)
                        emit groupsChanged();
                    else
                        emit usersChanged();
                }
            });

        m_connections << connect(userGroupManager, &UserGroupManager::addedOrUpdated, this,
            [this](const nx::vms::api::UserGroupData& userGroup)
            {
                if (!m_cache)
                    return;

                // For now we assume that UserAttribute::hidden cannot change,
                // so we don't remove hidden groups, assuming they were never added.
                if (nx::vms::common::isGroupHidden(userGroup))
                    return;

                // Removal happens before addition, this way we can update the cache.
                m_cache->modify(
                    /*added*/ {userGroup.id}, /*removed*/ {userGroup.id}, {}, {});

                emit customGroupCountChanged();

                checkCycles();
            });

        m_connections << connect(userGroupManager, &UserGroupManager::removed, this,
            [this](const nx::vms::api::UserGroupData& userGroup)
            {
                if (!m_cache)
                    return;

                const QSet<nx::Uuid> groupsWithChangedMembers =
                    {userGroup.parentGroupIds.begin(), userGroup.parentGroupIds.end()};

                m_cache->modify({}, {userGroup.id}, groupsWithChangedMembers, {});

                emit customGroupCountChanged();

                checkCycles();
            });

        const auto allUsers = resourcePool->getResources<QnUserResource>(
            [](const QnUserResourcePtr& user) { return !nx::vms::common::isUserHidden(user); });

        for (const auto& user: allUsers)
            subscribeToUser(user);

        m_connections << connect(resourcePool, &QnResourcePool::resourcesAdded, this,
            [this](const QnResourceList &resources)
            {
                if (m_subjectId.isNull())
                    return;

                const QnUserResourceList users = resources.filtered<QnUserResource>();

                QSet<nx::Uuid> addedIds;
                QSet<nx::Uuid> modifiedGroups;

                for (const auto& user: users)
                {
                    // For now we assume that UserAttribute::hidden cannot change,
                    // so we don't subscribe to QnUserResource::attributesChanged.
                    if (nx::vms::common::isUserHidden(user))
                        continue;

                    addedIds.insert(user->getId());
                    const auto groupIds = user->allGroupIds();
                    modifiedGroups += QSet<nx::Uuid>{groupIds.begin(), groupIds.end()};

                    subscribeToUser(user);
                }

                m_cache->modify(addedIds, {}, modifiedGroups, {});
            });

        m_connections << connect(
            resourcePool, &QnResourcePool::resourcesRemoved, this,
            [this](const QnResourceList &resources)
            {
                if (m_subjectId.isNull())
                    return;

                const QnUserResourceList users = resources.filtered<QnUserResource>();

                QSet<nx::Uuid> removedIds;
                QSet<nx::Uuid> modifiedGroups;

                for (const auto& user: users)
                {
                    unsubscribeFromUser(user);
                    removedIds.insert(user->getId());
                    const auto groupIds = user->allGroupIds();
                    modifiedGroups += QSet<nx::Uuid>{groupIds.begin(), groupIds.end()};
                }

                m_cache->modify({}, removedIds, modifiedGroups, {});
            });

        m_connections << connect(
            m_subjectContext->subjectHierarchy(), &nx::core::access::SubjectHierarchy::changed,
            this, [this](
                const QSet<nx::Uuid>& added,
                const QSet<nx::Uuid>& removed,
                const QSet<nx::Uuid>& groupsWithChangedMembers,
                const QSet<nx::Uuid>& subjectsWithChangedParents)
            {
                if (!m_cache)
                    return;

                m_cache->modify(
                    added,
                    removed,
                    groupsWithChangedMembers,
                    subjectsWithChangedParents);

                checkCycles();

                if (subjectsWithChangedParents.contains(m_subjectId))
                {
                    if (!m_cache->sorted().groups.empty())
                    {
                        const auto groupsStart = m_cache->sorted().users.size();
                        emit dataChanged(
                            this->index(groupsStart),
                            this->index(groupsStart + m_cache->sorted().groups.size() - 1));
                    }
                    emit parentGroupsChanged();

                    // Users may not be allowed due to group change.
                    for (int row: m_cache->temporaryUserIndexes())
                        emit dataChanged(this->index(row), this->index(row), {IsAllowedMember});
                }
            });

        m_connections << connect(
            m_subjectContext.get(), &AccessSubjectEditingContext::resourceAccessChanged,
            this, &MembersModel::sharedResourcesChanged);

        m_connections << connect(
            systemContext()->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::userModified,
            this,
            [this](const QnUserResourcePtr& user)
            {
                const int row = m_cache->indexIn(m_cache->sorted().users, user->getId());
                emit dataChanged(index(row, 0), index(row, 0), {CanEditParents});
            }
        );

        m_connections << connect(
            systemContext()->nonEditableUsersAndGroups(),
            &NonEditableUsersAndGroups::groupModified,
            this,
            [this](const nx::Uuid& groupId)
            {
                const auto groupsStart = m_cache->sorted().users.size();
                const int row = groupsStart + m_cache->indexIn(m_cache->sorted().groups, groupId);
                emit dataChanged(index(row, 0), index(row, 0), {CanEditParents});
            }
        );
    }
    else if (m_subjectId.isNull())
    {
        beginResetModel();
        m_userConnections.clear();
        m_connections.reset();
        m_subjectMembers = {};
        m_cache.reset();
        m_groupsWithCycles.clear();
        endResetModel();

        m_subjectContext.reset(nullptr);
        emit editingContextChanged();

        emit parentGroupsChanged();
        emit usersChanged();
        emit groupsChanged();
        emit globalPermissionsChanged();
        emit sharedResourcesChanged();
        emit cycledGroupChanged();

        return;
    }
    else
    {
        m_subjectContext->setCurrentSubject(m_subjectId, m_subjectIsUser
            ? AccessSubjectEditingContext::SubjectType::user
            : AccessSubjectEditingContext::SubjectType::group);
    }

    m_subjectMembers = {};

    std::set<nx::Uuid> memberUsers;
    std::set<nx::Uuid> memberGroups;
    nx::vms::common::getUsersAndGroups(systemContext(),
        m_subjectContext->subjectHierarchy()->directMembers(m_subjectId),
        memberUsers, memberGroups, /*includeHidden*/ false);

    m_subjectMembers.users =
        {std::make_move_iterator(memberUsers.begin()), std::make_move_iterator(memberUsers.end())};

    m_subjectMembers.groups =
        {std::make_move_iterator(memberGroups.begin()), std::make_move_iterator(memberGroups.end())};

    loadModelData();
}

void MembersModel::setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources)
{
    if (!m_subjectContext)
        return;

    m_subjectContext->setOwnResourceAccessMap(resources);
}

nx::core::access::ResourceAccessMap MembersModel::ownSharedResources() const
{
    if (!m_subjectContext)
        return {};

    return m_subjectContext->ownResourceAccessMap();
}

MembersListWrapper MembersModel::users() const
{
    if (!m_subjectContext)
        return {};

    return m_subjectMembers.users;
}

void MembersModel::setUsers(const MembersListWrapper& users)
{
    if (!m_subjectContext)
        return;

    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<nx::Uuid> newMembers;

    // Copy groups.
    for (const auto& id: members)
    {
        if (systemContext()->userGroupManager()->contains(id))
            newMembers.insert(id);
    }

    // Add users.
    for (const auto& id: users.list())
        newMembers.insert(id);

    m_subjectContext->setRelations(parents, members);
}

QList<nx::Uuid> MembersModel::groups() const
{
    if (!m_subjectContext)
        return {};

    return m_subjectMembers.groups;
}

void MembersModel::setGroups(const QList<nx::Uuid>& groups)
{
    if (!m_subjectContext)
        return;

    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<nx::Uuid> newMembers;

    // Copy users.
    for (const auto& id: members)
    {
        if (!systemContext()->userGroupManager()->contains(id))
            newMembers.insert(id);
    }

    // Add groups.
    for (const auto& id: groups)
        newMembers.insert(id);

    m_subjectContext->setRelations(parents, members);
}

void MembersModel::addParent(const nx::Uuid& groupId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    parents.insert(groupId);
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::removeParent(const nx::Uuid& groupId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    parents.remove(groupId);
    m_subjectContext->setRelations(parents, members);
}

bool MembersModel::isCycledGroup() const
{
    return !m_groupsWithCycles.isEmpty();
}

void MembersModel::addMember(const nx::Uuid& memberId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    members.insert(memberId);
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::removeMember(const nx::Uuid& memberId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    members.remove(memberId);
    m_subjectContext->setRelations(parents, members);
}

QHash<int, QByteArray> MembersModel::roleNames() const
{
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles[Qt::DisplayRole] = "text";
    roles[DescriptionRole] = "description";
    roles[core::DecorationPathRole] = "decorationPath";
    roles[IdRole] = "id";
    roles[IsUserRole] = "isUser";
    roles[OffsetRole] = "offset";
    roles[IsTopLevelRole] = "isTopLevel";
    roles[IsMemberRole] = "isMember";
    roles[IsParentRole] = "isParent";
    roles[MemberSectionRole] = "memberSection";
    roles[GroupSectionRole] = "groupSection";
    roles[IsLdap] = "isLdap";
    roles[IsTemporary] = "isTemporary";
    roles[Cycle] = "cycle";
    roles[CanEditParents] = "canEditParents";
    roles[CanEditMembers] = "canEditMembers";
    roles[IsPredefined] = "isPredefined";
    roles[UserType] = "userType";
    return roles;
}

bool MembersModel::isAllowed(const nx::Uuid& parentId, const nx::Uuid& childId) const
{
    // Unable to include into itself.
    if (parentId == childId)
        return false;

    // Predefined group cannot be put into another group.
    if (PredefinedUserGroups::contains(childId))
        return false;

    auto parentInfo = m_cache->info(parentId);

    // If we are in a user creation dialog, there is no user info available. But we can still
    // gather some essential data from other properties.
    if (parentInfo.name.isEmpty() && parentId == m_subjectId)
    {
        parentInfo.isGroup = !m_subjectIsUser;
        if (m_temporary)
            parentInfo.userType = UserSettingsGlobal::TemporaryUser;
    }

    // Unable to add users to users.
    if (!parentInfo.isGroup)
        return false;

    // LDAP group membership should be managed via LDAP.
    if (parentInfo.userType == UserSettingsGlobal::LdapUser)
        return false;

    auto childInfo = m_cache->info(childId);

    // If we are in a user creation dialog, gather info for childId also.
    const bool childInfoFromProperties = childInfo.name.isEmpty() && childId == m_subjectId;
    if (childInfoFromProperties)
    {
        childInfo.isGroup = !m_subjectIsUser;
        if (m_temporary)
            childInfo.userType = UserSettingsGlobal::TemporaryUser;
    }

    // Temporary users can not get PowerUsers (or higher) permissions.
    if (kRestrictedForTemp.contains(parentId)
        || m_subjectContext->subjectHierarchy()->isRecursiveMember(parentId, kRestrictedForTemp))
    {
        // Deny if child is a temporary user.
        if (childInfo.userType == UserSettingsGlobal::TemporaryUser)
            return false;

        // Deny if any child subgroup contains a temporary user.
        for (const auto& groupId: m_cache->groupsWithTemporary())
        {
            if (groupId == childId
                || m_subjectContext->subjectHierarchy()->isRecursiveMember(groupId, {childId}))
            {
                return false;
            }
        }
    }

    if (!childInfoFromProperties
        && !systemContext()->accessController()->hasPermissions(
            childId,
            Qn::WriteAccessRightsPermission))
    {
        return false;
    }

    return !m_subjectContext->subjectHierarchy()->isRecursiveMember(parentId, {childId});
}

void MembersModel::setTemporary(bool value)
{
    if (value == m_temporary)
        return;

    m_temporary = value;
    emit temporaryChanged();

    if (!m_subjectContext)
        return;

    if (m_temporary)
    {
        const auto hierarchy = m_subjectContext->subjectHierarchy();

        // Cleanup parent groups that are not allowed for a temporary user.

        auto parents = hierarchy->directParents(m_subjectId);
        auto members = hierarchy->directMembers(m_subjectId);

        QSet<nx::Uuid> parentsToRemove;
        for (const auto& groupId: parents)
        {
            if (kRestrictedForTemp.contains(groupId)
                || hierarchy->isRecursiveMember(groupId, {kRestrictedForTemp}))
            {
                parentsToRemove << groupId;
            }
        }

        m_subjectContext->setRelations(parents - parentsToRemove, members);
    }

    if (!m_cache)
        return;

    const int groupsBegin = m_cache->sorted().users.size();
    const int groupsEnd = groupsBegin + m_cache->sorted().groups.size() - 1;
    if (groupsEnd < groupsBegin)
        return;

    emit dataChanged(index(groupsBegin), index(groupsEnd), {IsAllowedMember, IsAllowedParent});
}

QVariant MembersModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount() || !m_subjectContext)
        return {};

    const bool isUser = index.row() < m_cache->sorted().users.size();
    const auto id = isUser
        ? m_cache->sorted().users.at(index.row())
        : m_cache->sorted().groups.at(index.row() - m_cache->sorted().users.size());

    switch (role)
    {
        case IsMemberRole:
            return !m_subjectIsUser
                && m_subjectContext->subjectHierarchy()->directMembers(m_subjectId)
                    .contains(id);

        case IsParentRole:
            return m_cache->info(id).isGroup
                && m_subjectContext->subjectHierarchy()->directMembers(id).contains(m_subjectId);

        case IsAllowedMember:
            return isAllowed(m_subjectId, id);

        case IsAllowedParent:
            return isAllowed(id, m_subjectId);

        case Qt::DisplayRole:
            return m_cache->info(id).name;

        case DescriptionRole:
            return m_cache->info(id).description;

        case IsUserRole:
            return !m_cache->info(id).isGroup;

        case IsPredefined:
            return PredefinedUserGroups::contains(id);

        case OffsetRole:
            return 0;

        case IdRole:
            return QVariant::fromValue(id);

        case MemberSectionRole:
            return m_cache->info(id).isGroup
                ? UserSettingsGlobal::kGroupsSection
                : UserSettingsGlobal::kUsersSection;

        case GroupSectionRole:
            if (m_cache->info(id).userType == UserSettingsGlobal::LdapUser)
                return UserSettingsGlobal::kLdapGroupsSection;

            return PredefinedUserGroups::contains(id)
                ? UserSettingsGlobal::kBuiltInGroupsSection
                : UserSettingsGlobal::kCustomGroupsSection;

        case IsLdap:
            return m_cache->info(id).userType == UserSettingsGlobal::LdapUser;

        case IsTemporary:
            return m_cache->info(id).userType == UserSettingsGlobal::TemporaryUser;

        case Cycle:
            return m_groupsWithCycles.contains(id);

        case CanEditParents:
        {
            const bool bothAreLdap =
                m_cache->info(m_subjectId).userType == UserSettingsGlobal::LdapUser
                && m_cache->info(id).userType == UserSettingsGlobal::LdapUser;
            return canModifyRelation(id) && !bothAreLdap
                && systemContext()->accessController()->hasPermissions(
                    id, Qn::WriteAccessRightsPermission)
                && systemContext()->nonEditableUsersAndGroups()->canEditParents(id);
        }

        case CanEditMembers:
            return canEditMembers(id);

        case UserType:
            return m_cache->info(id).userType;

        case Qt::ToolTipRole:
        {
            if (!systemContext()->nonEditableUsersAndGroups()->canEditParents(id))
                return systemContext()->nonEditableUsersAndGroups()->tooltip(id);

            if (!canModifyRelation(id))
            {
                return m_subjectIsUser
                    ? tr("This group has been assigned at organization level and can be managed "
                         "only at organization level")
                    : tr("The user has been assigned to this group at organization level and can "
                        "be unassigned only at organization level");
            }

            return {};
        }

        case core::DecorationPathRole:
        {
            if (index.data(MembersModel::IsPredefined).toBool())
                return "20x20/Solid/group_default.svg";

            return index.data(MembersModel::IsLdap).toBool()
                ? "20x20/Solid/group_ldap.svg"
                : "20x20/Solid/group.svg";
        }

        default:
            return {};
    }
}

bool MembersModel::canModifyRelation(const nx::Uuid& id) const
{
    const auto resourcePool = systemContext()->resourcePool();
    const auto userId = m_subjectIsUser ? m_subjectId : id;
    const auto groupId = m_subjectIsUser ? id : m_subjectId;
    if (const auto user = resourcePool->getResourceById<QnUserResource>(userId))
    {
        const auto orgGroupIds = user->orgGroupIds();
        return std::find(orgGroupIds.begin(), orgGroupIds.end(), groupId) == orgGroupIds.end();
    }

    return true;
}

bool MembersModel::canEditMembers(const nx::Uuid& id) const
{
    const auto accessController = qobject_cast<AccessController*>(
        systemContext()->accessController());

    const auto canModify = canModifyRelation(id) && m_cache
        && m_cache->info(id).userType != UserSettingsGlobal::LdapUser
        && NX_ASSERT(accessController)
        && accessController->canCreateUser(
            /*targetPermissions*/ {},
            /*targetGroups*/ {id});

    if (auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(m_subjectId))
        return canModify && accessController->hasPermissions(m_subjectId, Qn::WriteAccessRightsPermission);

    return canModify;
}

bool MembersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() < 0 || index.row() >= rowCount())
        return false;

    const auto currentValue = data(index, role).toBool();
    if (currentValue == value.toBool())
        return false;

    const bool isUser = data(index, IsUserRole).toBool();
    const auto selectedId = data(index, IdRole).value<nx::Uuid>();

    if (role == IsMemberRole)
    {
        if (m_subjectIsUser) //< Users cannot have members.
            return false;

        if (!data(index, CanEditParents).toBool())
            return false;

        if (currentValue)
            removeMember(selectedId);
        else
            addMember(selectedId);

        if (isUser)
            emit usersChanged();
        else
            emit groupsChanged();

        return true;
    }
    else if (role == IsParentRole)
    {
        if (isUser)
            return false;

        if (!canEditMembers(selectedId))
            return false;

        if (currentValue)
            removeParent(selectedId);
        else
            addParent(selectedId);

        emit parentGroupsChanged();
        return true;
    }
    return false;
}

int MembersModel::rowCount(const QModelIndex&) const
{
    if (!m_cache)
        return 0;

    return m_cache->sorted().users.size() + m_cache->sorted().groups.size();
}

QList<MembersModelGroup> MembersModel::parentGroups() const
{
    if (!m_subjectContext || m_cache.isNull())
        return {};

    auto directParents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId).values();
    m_cache->sortSubjects(directParents);

    QList<MembersModelGroup> result;
    for (const auto& groupId: directParents)
        result << MembersModelGroup::fromId(systemContext(), groupId);

    return result;
}

void MembersModel::setParentGroups(const QList<MembersModelGroup>& groups)
{
    if (!m_subjectContext)
        return;

    const auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<nx::Uuid> groupIds;
    for (const auto& group: groups)
        groupIds.insert(group.id);

    m_subjectContext->setRelations(groupIds, members);
}

void MembersModel::setGroupId(const nx::Uuid& groupId)
{
    // Setting a group or a user forces reload of model data.
    const bool groupChanged = groupId != m_subjectId;

    m_subjectId = groupId;

    const bool userChanged = m_subjectIsUser;
    m_subjectIsUser = false;

    readUsersAndGroups();

    if (userChanged)
        emit userIdChanged();
    if (groupChanged)
        emit groupIdChanged();
}

void MembersModel::setUserId(const nx::Uuid& userId)
{
    const bool userChanged = userId != m_subjectId;

    m_subjectId = userId;

    const bool groupChanged = !m_subjectIsUser;
    m_subjectIsUser = true;

    readUsersAndGroups();

    if (groupChanged)
        emit groupIdChanged();
    if (userChanged)
        emit userIdChanged();
}

class AllowedMembersModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!sourceModel())
            return base_type::filterAcceptsRow(sourceRow, sourceParent);

        const auto sourceIndex = sourceModel()->index(sourceRow, 0);
        const auto regExp = filterRegularExpression();
        const auto getData =
            [this, &sourceIndex](int role) { return sourceModel()->data(sourceIndex, role); };

        return (getData(MembersModel::IsAllowedMember).toBool()
                || getData(MembersModel::IsMemberRole).toBool())
            && (getData(MembersModel::CanEditParents).toBool()
                || getData(MembersModel::IsMemberRole).toBool())
            && (getData(Qt::DisplayRole).toString().contains(regExp)
                || getData(MembersModel::DescriptionRole).toString().contains(regExp));
    }
};

class MembersSummaryModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!sourceModel())
            return base_type::filterAcceptsRow(sourceRow, sourceParent);

        const auto sourceIndex = sourceModel()->index(sourceRow, 0);

        return sourceModel()->data(sourceIndex, MembersModel::IsMemberRole).toBool()
            && base_type::filterAcceptsRow(sourceRow, sourceParent);
    }
};

class AllowedParentsModel: public QSortFilterProxyModel
{
    using base_type = QSortFilterProxyModel;

public:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!sourceModel())
            return base_type::filterAcceptsRow(sourceRow, sourceParent);

        const auto sourceIndex = sourceModel()->index(sourceRow, 0);
        return (sourceModel()->data(sourceIndex, MembersModel::IsAllowedParent).toBool()
                || sourceModel()->data(sourceIndex, MembersModel::IsParentRole).toBool())
            && (sourceModel()->data(sourceIndex, MembersModel::CanEditMembers).toBool()
                || sourceModel()->data(sourceIndex, MembersModel::IsParentRole).toBool())
            && base_type::filterAcceptsRow(sourceRow, sourceParent);
    }
};

class DirectParentsModel: public QSortFilterProxyModel
{
    Q_DECLARE_TR_FUNCTIONS(DirectParentsModel)
    using base_type = QSortFilterProxyModel;

public:
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (index.row() < 0 || index.row() >= rowCount() || !sourceModel())
            return {};

        switch (role)
        {
            case Qt::ToolTipRole:
                if (mapToSource(index).data(MembersModel::IsLdap).toBool())
                    return tr("LDAP group membership is managed on LDAP server");

            default:
                return mapToSource(index).data(role);
        }

        return {};
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        if (!sourceModel())
            return base_type::filterAcceptsRow(sourceRow, sourceParent);

        const auto sourceIndex = sourceModel()->index(sourceRow, 0);
        return sourceModel()->data(sourceIndex, MembersModel::IsParentRole).toBool()
            && base_type::filterAcceptsRow(sourceRow, sourceParent);
    }
};

void MembersModel::registerQmlType()
{
    qRegisterMetaType<nx::vms::client::desktop::MembersModelGroup>();

    // Main model.
    qmlRegisterType<MembersModel>("nx.vms.client.desktop", 1, 0, "MembersModel");

    // Groups tab.
    qmlRegisterType<AllowedParentsModel>("nx.vms.client.desktop", 1, 0, "AllowedParentsModel");
    qmlRegisterType<DirectParentsModel>("nx.vms.client.desktop", 1, 0, "DirectParentsModel");

    // Members tab.
    qmlRegisterType<AllowedMembersModel>("nx.vms.client.desktop", 1, 0, "AllowedMembersModel");
    qmlRegisterType<MembersSummaryModel>("nx.vms.client.desktop", 1, 0, "MembersSummaryModel");

    qRegisterMetaType<nx::core::access::ResourceAccessMap>();
}

} // namespace nx::vms::client::desktop
