// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "members_model.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include "../globals/user_settings_global.h"
#include "core/resource/resource_fwd.h"


namespace nx::vms::client::desktop {

MembersModelGroup MembersModelGroup::fromId(SystemContext* systemContext, const QnUuid& id)
{
    const auto group = systemContext->userRolesManager()->userRole(id);

    return {
        .id = group.id,
        .text = group.name,
        .description = group.description,
        .isLdap = group.isLdap,
        .isPredefined = group.isPredefined};
}

MembersModel::MembersModel(SystemContext* systemContext):
    SystemContextAware(systemContext)
{
}

MembersModel::MembersModel():
    SystemContextAware(SystemContext::fromQmlContext(this))
{
}

MembersModel::~MembersModel() {}

int MembersModel::customGroupCount() const
{
    if (!m_cache)
        return 0;

    const auto result =
        m_cache->sorted().groups.size() - QnPredefinedUserRoles::enumValues().size();

    return result < 0 ? 0 : result;
}

void MembersModel::loadModelData()
{
    beginResetModel();
    m_cache->loadInfo(systemContext());
    m_cache->sorted(m_subjectId); //< Force storage to report changes for current subject.
    endResetModel();

    emit customGroupCountChanged();

    emit parentGroupsChanged();
    emit usersChanged();
    emit groupsChanged();
    emit globalPermissionsChanged();
    emit sharedResourcesChanged();
}

void addToSorted(QList<QnUuid>& list, const QnUuid& id)
{
    auto it = std::lower_bound(list.begin(), list.end(), id);
    if (it != list.end() && *it == id)
        return;

    list.insert(it, id);
}

void removeFromSorted(QList<QnUuid>& list, const QnUuid& id)
{
    auto it = std::lower_bound(list.begin(), list.end(), id);

    if (it == list.end() || *it != id)
        return;

    list.erase(it);
}

void MembersModel::subscribeToUser(const QnUserResourcePtr& user)
{
    const auto updateUser =
        [this](const QnUuid& id)
        {
            m_cache->modify({id}, {id}, {}, {});
        };

    nx::utils::ScopedConnections userConnections;

    userConnections << connect(user.get(), &QnResource::nameChanged, this,
        [updateUser](const QnResourcePtr& resource)
        {
            updateUser(resource->getId());
        });

    userConnections << connect(user.get(), &QnUserResource::userRolesChanged, this,
        [updateUser](
            const QnUserResourcePtr& user,
            const std::vector<QnUuid>&)
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
    QSet<QnUuid> groupsWithCycles;
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

    m_groupsWithCycles = groupsWithCycles;

    for (const auto& id: modifiedGroups)
    {
        const auto row = m_cache->sorted().users.size()
            + m_cache->indexIn(m_cache->sorted().groups, id);
        emit dataChanged(index(row, 0), index(row, 0));
    }
}

void MembersModel::readUsersAndGroups()
{
    if (m_subjectContext.isNull() && !m_subjectId.isNull())
    {
        m_subjectContext.reset(new AccessSubjectEditingContext(systemContext()));
        m_subjectContext->setCurrentSubjectId(m_subjectId);
        emit editingContextChanged();

        m_cache.reset(new MembersCache());
        m_cache->setContext(m_subjectContext.get());
        emit membersCacheChanged();

        connect(m_cache.get(), &MembersCache::beginInsert, this,
            [this](int at, const QnUuid& id, const QnUuid& parentId)
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
            [this](int, const QnUuid& id, const QnUuid& parentId)
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
            [this](int at, const QnUuid& id, const QnUuid& parentId)
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
            [this](int, const QnUuid& id, const QnUuid& parentId)
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

        auto userRolesManager = systemContext()->userRolesManager();

        m_connections << connect(
            userRolesManager, &QnUserRolesManager::userRoleAddedOrUpdated, this,
            [this](const nx::vms::api::UserRoleData& userGroup)
            {
                if (!m_cache)
                    return;

                // Removal happens before addition, this way we can update the cache.
                m_cache->modify(
                    /*removed*/ {userGroup.id}, /*added*/ {userGroup.id}, {}, {});

                emit customGroupCountChanged();

                checkCycles();
            });

        m_connections << connect(userRolesManager, &QnUserRolesManager::userRoleRemoved, this,
            [this](const nx::vms::api::UserRoleData& userGroup)
            {
                if (!m_cache)
                    return;

                const QSet<QnUuid> groupsWithChangedMembers =
                    {userGroup.parentRoleIds.begin(), userGroup.parentRoleIds.end()};

                m_cache->modify({}, {userGroup.id}, groupsWithChangedMembers, {});

                emit customGroupCountChanged();

                checkCycles();
            });

        auto resourcePool = systemContext()->resourcePool();

        const auto allUsers = resourcePool->getResources<QnUserResource>();
        for (const auto& user: allUsers)
            subscribeToUser(user);

        m_connections << connect(resourcePool, &QnResourcePool::resourcesAdded, this,
            [this](const QnResourceList &resources)
            {
                if (m_subjectId.isNull())
                    return;

                const QnUserResourceList users = resources.filtered<QnUserResource>();

                QSet<QnUuid> addedIds;
                QSet<QnUuid> modifiedGroups;

                for (const auto& user: users)
                {
                    addedIds.insert(user->getId());
                    const auto groupIds = user->userRoleIds();
                    modifiedGroups += QSet<QnUuid>{groupIds.begin(), groupIds.end()};

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

                QSet<QnUuid> removedIds;
                QSet<QnUuid> modifiedGroups;

                for (const auto& user: users)
                {
                    unsubscribeFromUser(user);
                    removedIds.insert(user->getId());
                    const auto groupIds = user->userRoleIds();
                    modifiedGroups += QSet<QnUuid>{groupIds.begin(), groupIds.end()};
                }

                m_cache->modify({}, removedIds, modifiedGroups, {});
            });

        m_connections << connect(
            m_subjectContext->subjectHierarchy(), &nx::core::access::SubjectHierarchy::changed,
            this, [this](
                const QSet<QnUuid>& added,
                const QSet<QnUuid>& removed,
                const QSet<QnUuid>& groupsWithChangedMembers,
                const QSet<QnUuid>& subjectsWithChangedParents)
            {
                if (!m_cache)
                    return;

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
                }

                m_cache->modify(
                    added,
                    removed,
                    groupsWithChangedMembers,
                    subjectsWithChangedParents);

                checkCycles();
            });

        m_connections << connect(
            m_subjectContext.get(), &AccessSubjectEditingContext::resourceAccessChanged,
            this, &MembersModel::sharedResourcesChanged);
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

        return;
    }
    else
    {
        m_subjectContext->setCurrentSubjectId(m_subjectId);
    }

    const auto userRolesManager = systemContext()->userRolesManager();

    m_subjectMembers = {};
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    for (const auto& id: members)
        (userRolesManager->hasRole(id) ? m_subjectMembers.groups : m_subjectMembers.users) << id;

    std::sort(m_subjectMembers.users.begin(), m_subjectMembers.users.end());
    std::sort(m_subjectMembers.groups.begin(), m_subjectMembers.groups.end());

    loadModelData();
}

void MembersModel::setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources)
{
    if (m_subjectContext.isNull())
        return;

    m_subjectContext->setOwnResourceAccessMap(resources);
}

nx::core::access::ResourceAccessMap MembersModel::ownSharedResources() const
{
    if (m_subjectContext.isNull())
        return {};

    return m_subjectContext->ownResourceAccessMap();
}

MembersListWrapper MembersModel::users() const
{
    if (m_subjectContext.isNull())
        return {};

    return m_subjectMembers.users;
}

void MembersModel::setUsers(const MembersListWrapper& users)
{
    if (m_subjectContext.isNull())
        return;

    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<QnUuid> newMembers;

    // Copy groups.
    for (const auto& id: members)
    {
        if (systemContext()->userRolesManager()->hasRole(id))
            newMembers.insert(id);
    }

    // Add users.
    for (const auto& id: users.list())
        newMembers.insert(id);

    m_subjectContext->setRelations(parents, members);
}

QList<QnUuid> MembersModel::groups() const
{
    if (m_subjectContext.isNull())
        return {};

    return m_subjectMembers.groups;
}

void MembersModel::setGroups(const QList<QnUuid>& groups)
{
    if (m_subjectContext.isNull())
        return;

    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<QnUuid> newMembers;

    // Copy users.
    for (const auto& id: members)
    {
        if (!systemContext()->userRolesManager()->hasRole(id))
            newMembers.insert(id);
    }

    // Add groups.
    for (const auto& id: groups)
        newMembers.insert(id);

    m_subjectContext->setRelations(parents, members);
}

void MembersModel::addParent(const QnUuid& groupId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    parents.insert(groupId);
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::removeParent(const QnUuid& groupId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    parents.remove(groupId);
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::addMember(const QnUuid& memberId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    members.insert(memberId);
//    m_membersCache.remove(m_subjectId);
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::removeMember(const QnUuid& memberId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    members.remove(memberId);
//    m_membersCache.remove(m_subjectId);
    m_subjectContext->setRelations(parents, members);
}

QHash<int, QByteArray> MembersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "text";
    roles[DescriptionRole] = "description";
    roles[IdRole] = "id";
    roles[IsUserRole] = "isUser";
    roles[OffsetRole] = "offset";
    roles[IsTopLevelRole] = "isTopLevel";
    roles[IsMemberRole] = "isMember";
    roles[IsParentRole] = "isParent";
    roles[MemberSectionRole] = "memberSection";
    roles[GroupSectionRole] = "groupSection";
    roles[IsLdap] = "isLdap";
    roles[Cycle] = "cycle";
    return roles;
}

bool MembersModel::isAllowedMember(const QnUuid& groupId) const
{
    if (groupId == m_subjectId)
        return false;

    if (MembersCache::isPredefined(groupId))
        return false;

    return !m_subjectContext->subjectHierarchy()->isRecursiveMember(m_subjectId, {groupId});
}

bool MembersModel::isAllowedParent(const QnUuid& groupId) const
{
    if (groupId == m_subjectId)
        return false;

    if (MembersCache::isPredefined(m_subjectId))
        return false;

    if (const auto user = systemContext()->accessController()->user(); user && !user->isOwner())
    {
        static const auto kOwnerId = QnPredefinedUserRoles::id(Qn::UserRole::owner);
        if (groupId == kOwnerId)
            return false;
    }

    return !m_subjectContext->subjectHierarchy()->recursiveParents({groupId}).contains(m_subjectId);
}

QVariant MembersModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount() || m_subjectContext.isNull())
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
            return !m_subjectIsUser
                && !MembersCache::isPredefined(id)
                && isAllowedMember(id);

        case IsAllowedParent:
            return m_cache->info(id).isGroup
                && isAllowedParent(id)
                && !m_cache->info(id).isLdap;

        case Qt::DisplayRole:
            return m_cache->info(id).name;

        case DescriptionRole:
            return m_cache->info(id).description;

        case IsUserRole:
            return !m_cache->info(id).isGroup;

        case OffsetRole:
            return 0;

        case IdRole:
            return QVariant::fromValue(id);

        case MemberSectionRole:
            return m_cache->info(id).isGroup
                ? UserSettingsGlobal::kGroupsSection
                : UserSettingsGlobal::kUsersSection;

        case GroupSectionRole:
            return MembersCache::isPredefined(id)
                ? UserSettingsGlobal::kBuiltInGroupsSection
                : UserSettingsGlobal::kCustomGroupsSection;

        case IsLdap:
            return m_cache->info(id).isLdap;

        case Cycle:
            return m_groupsWithCycles.contains(id);

        default:
            return {};
    }
}

bool MembersModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() < 0 || index.row() >= rowCount())
        return false;

    const auto currentValue = data(index, role).toBool();
    if (currentValue == value.toBool())
        return false;

    const bool isUser = data(index, IsUserRole).toBool();
    const auto selectedId = data(index, IdRole).value<QnUuid>();

    if (role == IsMemberRole)
    {
        if (m_subjectIsUser) //< Users cannot have members.
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
    if (m_subjectContext.isNull() || m_cache.isNull())
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
    if (m_subjectContext.isNull())
        return;

    const auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);

    QSet<QnUuid> groupIds;
    for (const auto& group: groups)
        groupIds.insert(group.id);

    m_subjectContext->setRelations(groupIds, members);
}

void MembersModel::setGroupId(const QnUuid& groupId)
{
    // Setting a group or a user forces reload of model data.
    const bool groupChanged = groupId == m_subjectId;

    m_subjectId = groupId;

    const bool userChanged = m_subjectIsUser;
    m_subjectIsUser = false;

    readUsersAndGroups();

    if (userChanged)
        emit userIdChanged();
    if (groupChanged)
        emit groupIdChanged();
}

void MembersModel::setUserId(const QnUuid& userId)
{
    const bool userChanged = userId == m_subjectId;

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
        return (sourceModel()->data(sourceIndex, MembersModel::IsAllowedMember).toBool()
            || sourceModel()->data(sourceIndex, MembersModel::IsMemberRole).toBool())
            && base_type::filterAcceptsRow(sourceRow, sourceParent);
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

    // Members tab.
    qmlRegisterType<AllowedMembersModel>("nx.vms.client.desktop", 1, 0, "AllowedMembersModel");
    qmlRegisterType<MembersSummaryModel>("nx.vms.client.desktop", 1, 0, "MembersSummaryModel");

    qRegisterMetaType<nx::core::access::ResourceAccessMap>();

}

} // namespace nx::vms::client::desktop
