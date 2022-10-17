// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "members_model.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtQml/QtQml>

#include <client_core/client_core_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/client/desktop/system_administration/models/user_list_model.h>
#include <nx/vms/client/desktop/system_context.h>
#include "../globals/user_settings_global.h"

namespace nx::vms::client::desktop {

namespace {

// Return sorted list if predefined groups.
static const QList<QnUuid> predefinedGroupIds()
{
    QList<QnUuid> result;
    for (auto role: QnPredefinedUserRoles::enumValues())
        result << QnPredefinedUserRoles::id(role);
    return result;
}

static const QSet<QnUuid> predefinedGroupIdsSet()
{
    QSet<QnUuid> result;
    for (auto role: QnPredefinedUserRoles::enumValues())
        result << QnPredefinedUserRoles::id(role);
    return result;
}

std::pair<QString, bool> getIdInfo(nx::vms::common::SystemContext* systemContext, const QnUuid& id)
{
    const auto groupsManager = systemContext->userRolesManager();
    if (const auto group = groupsManager->userRole(id); !group.id.isNull())
    {
        return {group.name, group.isLdap};
    }
    const auto user = systemContext->resourcePool()->getResourceById<QnUserResource>(id);
    if (!user)
        return {{}, false};

    return {user->getName(), user->isLdap()};
}

} // namespace

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

bool MembersModel::isPredefined(const QnUuid& groupId)
{
    static const auto predefinedIds = predefinedGroupIdsSet();
    return predefinedIds.contains(groupId);
}

void MembersModel::sortSubjects(
    QList<QnUuid>& subjects,
    nx::vms::common::SystemContext* systemContext)
{
    std::sort(subjects.begin(), subjects.end(),
        [systemContext](const QnUuid& l, const QnUuid& r)
        {
            const bool leftIsGroup = systemContext->userRolesManager()->hasRole(l);
            const bool rightIsGroup = systemContext->userRolesManager()->hasRole(r);

            // Users go first.
            if (leftIsGroup != rightIsGroup)
                return rightIsGroup;

            // Predefined groups go first.
            const auto predefinedLeft = isPredefined(l);
            const auto predefinedRight = isPredefined(r);
            if (predefinedLeft != predefinedRight)
            {
                return predefinedLeft;
            }
            else if(predefinedLeft)
            {
                const auto predefList = predefinedGroupIds();
                return predefList.indexOf(l) < predefList.indexOf(r);
            }

            const auto [leftName, leftIsLdap] = getIdInfo(systemContext, l);
            const auto [rightName, rightIsLdap] = getIdInfo(systemContext, r);

            // LDAP groups go last.
            if (leftIsLdap != rightIsLdap)
                return rightIsLdap;

            // Case Insensitive sort.
            return nx::utils::naturalStringCompare(leftName, rightName, Qt::CaseInsensitive) < 0;
        });
}

std::pair<QList<QnUuid>, QList<QnUuid>> MembersModel::sortedSubjects(
    const QSet<QnUuid>& subjectSet) const
{
    QList<QnUuid> subjects = subjectSet.values();
    const auto groupsManager = systemContext()->userRolesManager();

    QList<QnUuid> users;
    QList<QnUuid> groups;

    for (const auto& id: subjects)
        (groupsManager->hasRole(id) ? groups : users) << id;

    sortSubjects(users, systemContext());
    sortSubjects(groups, systemContext());

    return {users, groups};
}

int MembersModel::selectedCustomGroupsCount() const
{
    const auto parentGroups = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);

    if (parentGroups.empty())
        return 0;

    return (parentGroups - predefinedGroupIdsSet()).size();
}

void MembersModel::loadModelData()
{
    auto rolesManager = systemContext()->userRolesManager();

    m_allUsers.clear();
    m_allGroups.clear();

    // Built-in groups.
    for (const auto& role: QnPredefinedUserRoles::enumValues())
    {
        const auto id = QnPredefinedUserRoles::id(role);
        m_allGroups << id;
    }

    const auto customGroups = rolesManager->userRoles();
    for (const auto& group: customGroups)
    {
        if (isPredefined(group.id))
            continue;

        m_allGroups << group.id;
    }

    // Cache list of users in each group.
    const auto allUsers = systemContext()->resourcePool()->getResources<QnUserResource>();
    for (const auto& user: allUsers)
        m_allUsers << user->getId();

    sortSubjects(m_allUsers, systemContext());
    sortSubjects(m_allGroups, systemContext());

    emit parentGroupsChanged();
    emit usersChanged();
    emit groupsChanged();
    emit globalPermissionsChanged();
    emit sharedResourcesChanged();

    rebuildModel();
}

void MembersModel::readUsersAndGroups()
{
    if (m_subjectContext.isNull())
    {
        m_subjectContext.reset(new AccessSubjectEditingContext(systemContext()));
        m_subjectContext->setCurrentSubjectId(m_subjectId);
        emit editingContextChanged();

        connect(m_subjectContext.get(), &AccessSubjectEditingContext::hierarchyChanged,
            this, [this] { loadModelData(); });

        connect(m_subjectContext->subjectHierarchy(), &nx::core::access::SubjectHierarchy::changed,
            this, [this](
                const QSet<QnUuid>& added,
                const QSet<QnUuid>& removed,
                const QSet<QnUuid>& groupsWithChangedMembers,
                const QSet<QnUuid>& subjectsWithChangedParents)
            {
                if (subjectsWithChangedParents.contains(m_subjectId))
                    emit parentGroupsChanged();

                bool groupsNotified = false;
                bool usersNotified = false;
                for (const auto& id: subjectsWithChangedParents)
                {
                    if (id == m_subjectId)
                        continue;

                    if (systemContext()->userRolesManager()->hasRole(id) && !groupsNotified)
                    {
                        emit groupsChanged();
                        groupsNotified = true;
                    }
                    else if (!usersNotified)
                    {
                        emit usersChanged();
                        usersNotified = true;
                    }

                    if (groupsNotified && usersNotified)
                        break;
                }

                loadModelData();
            });

        connect(m_subjectContext.get(), &AccessSubjectEditingContext::resourceAccessChanged,
            this, &MembersModel::sharedResourcesChanged);
    }

    loadModelData();
}

MembersModel::MemberInfo MembersModel::info(const QnUuid& id) const
{
    const auto groupsManager = systemContext()->userRolesManager();

    if (const auto data = QnPredefinedUserRoles::get(id))
        return {data->name, data->description, /*isGroup*/ true, data->isLdap};
    if (const auto group = groupsManager->userRole(id); !group.id.isNull())
        return {group.name, group.description, /*isGroup*/ true, group.isLdap};

    if (const auto user = systemContext()->resourcePool()->getResourceById<QnUserResource>(id))
        return {user->getName(), user->fullName(), /*isGroup*/ false, user->isLdap()};

    return {};
}

void MembersModel::setOwnSharedResources(const nx::core::access::ResourceAccessMap& resources)
{
    m_subjectContext->setOwnResourceAccessMap(resources);
}

nx::core::access::ResourceAccessMap MembersModel::ownSharedResources() const
{
    if (m_subjectContext.isNull())
        return {};

    return m_subjectContext->ownResourceAccessMap();
}

QList<QnUuid> MembersModel::users() const
{
    if (m_subjectContext.isNull())
        return {};

    auto directMembers = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    const auto [users, groups] = sortedSubjects(directMembers);
    return users;
}

void MembersModel::setUsers(const QList<QnUuid>& users)
{
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
    for (const auto& id: users)
        newMembers.insert(id);

    m_subjectContext->setRelations(parents, members);
}

QList<QnUuid> MembersModel::groups() const
{
    if (m_subjectContext.isNull())
        return {};

    auto directMembers = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    const auto [users, groups] = sortedSubjects(directMembers);
    return groups;
}

void MembersModel::setGroups(const QList<QnUuid>& groups)
{
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

bool MembersModel::rebuildModel()
{
    int itemCount = m_allUsers.size();
    QHash<QnUuid, int> totalSubItems;

    static constexpr int kCycleGuardValue = -1;
    static constexpr int kEmptyValue = -2;

    const std::function<int(QnUuid)> subItemsCount =
        [this, &totalSubItems, &subItemsCount](QnUuid id) -> int
        {
            const int visitedSubItems = totalSubItems.value(id, kEmptyValue);
            if (visitedSubItems == kCycleGuardValue)
                return kCycleGuardValue;

            if (visitedSubItems >= 0)
                return visitedSubItems;

            totalSubItems[id] = kCycleGuardValue;

            const auto members = m_subjectContext->subjectHierarchy()->directMembers(id);
            const auto groupsManager = systemContext()->userRolesManager();

            int total = 0;
            for (const auto& subGroupId: members)
            {
                // User adds 1 row.
                if (!groupsManager->hasRole(subGroupId))
                {
                    total += 1;
                    continue;
                }

                const int subSize = subItemsCount(subGroupId);
                if (subSize == kCycleGuardValue)
                {
                    return kCycleGuardValue; // Found a cycle, the configuration is invalid.
                }

                total += 1 + subSize;
            }

            totalSubItems[id] = total;

            return total;
        };

    for (const auto& groupId: m_allGroups)
    {
        const int addCount = subItemsCount(groupId);

        if (addCount == kCycleGuardValue)
            return false;

        itemCount += 1 + addCount;
    }

    const int diff = itemCount - m_itemCount;

    if (diff > 0)
    {
        beginInsertRows({}, m_itemCount, m_itemCount + diff - 1);
        m_itemCount = itemCount;
        m_totalSubItems = totalSubItems;
        endInsertRows();
    }
    else if (diff < 0)
    {
        beginRemoveRows({}, m_itemCount + diff, m_itemCount - 1);
        m_itemCount = itemCount;
        m_totalSubItems = totalSubItems;
        endRemoveRows();
    }
    else
    {
        m_totalSubItems = totalSubItems;
    }

    if (m_itemCount > 0)
    {
        emit dataChanged(
            this->index(0, 0),
            this->index(m_itemCount - 1, 0));
    }

    return true;
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
    m_subjectContext->setRelations(parents, members);
}

void MembersModel::removeMember(const QnUuid& memberId)
{
    auto parents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId);
    auto members = m_subjectContext->subjectHierarchy()->directMembers(m_subjectId);
    members.remove(memberId);
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
    return roles;
}

QVariant MembersModel::getGroupData(int offset, const QnUuid& groupId, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return info(groupId).name;
        case DescriptionRole:
            return info(groupId).description;
        case IsUserRole:
            return false;
        case OffsetRole:
            return offset;
        case IsTopLevelRole:
            return offset == 0;
        case IdRole:
            return QVariant::fromValue(groupId);
        case MemberSectionRole:
            return UserSettingsGlobal::kGroupsSection;
        case GroupSectionRole:
            return isPredefined(groupId)
                ? UserSettingsGlobal::kBuiltInGroupsSection
                : UserSettingsGlobal::kCustomGroupsSection;
        case IsLdap:
            return info(groupId).isLdap;
    }
    return {};
}

QVariant MembersModel::getUserData(int offset, const QnUuid& userId, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
            return info(userId).name;
        case DescriptionRole:
            return info(userId).description;
        case IsUserRole:
            return true;
        case OffsetRole:
            return offset;
        case IsTopLevelRole:
            return offset == 0;
        case IdRole:
            return QVariant::fromValue(userId);
        case MemberSectionRole:
            // Users under groups should go into 'Groups' section.
            return offset > 0
                ? UserSettingsGlobal::kGroupsSection
                : UserSettingsGlobal::kUsersSection;
        case IsLdap:
            return info(userId).isLdap;
    }
    return {};
}

std::pair<QnUuid, int> MembersModel::findGroupRow(const QList<QnUuid>& groups, int row) const
{
    int currentRow = 0;

    for (const QnUuid& groupId: groups)
    {
        const int itemsInGroup = 1 + m_totalSubItems.value(groupId, 0);
        const int rowOffsetInGroup = row - currentRow;

        if (rowOffsetInGroup >= 0 && rowOffsetInGroup < itemsInGroup)
            return {groupId, rowOffsetInGroup};

        currentRow += itemsInGroup;
    }

    return {{}, -1};
}

QVariant MembersModel::getData(int offset, const QnUuid& groupId, int row, int role) const
{
    if (row == 0)
        return getGroupData(offset, groupId, role);

    const auto [users, groups] =
        sortedSubjects(m_subjectContext->subjectHierarchy()->directMembers(groupId));

    if (row > 0 && (size_t) row <= users.size())
        return getUserData(offset + 1, users[row - 1], role);

    const int groupRow = row - users.size() - 1;

    const auto [subGroupId, subGroupRow] = findGroupRow(groups, groupRow);

    return getData(offset + 1, subGroupId, subGroupRow, role);
}

bool MembersModel::isAllowedMember(const QnUuid& groupId) const
{
    if (groupId == m_subjectId)
        return false;

    if (isPredefined(groupId))
        return false;

    return !m_subjectContext->subjectHierarchy()->isRecursiveMember(m_subjectId, {groupId});
}

bool MembersModel::isAllowedParent(const QnUuid& groupId) const
{
    if (groupId == m_subjectId)
        return false;

    if (isPredefined(m_subjectId))
        return false;

    static const auto kOwnerId = QnPredefinedUserRoles::id(Qn::UserRole::owner);
    if (groupId == kOwnerId)
        return false;

    return !m_subjectContext->subjectHierarchy()->recursiveParents({groupId}).contains(m_subjectId);
}

QVariant MembersModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= rowCount())
        return {};

    if (index.row() < m_allUsers.size())
    {
        // Top level user.

        const auto userId = m_allUsers.at(index.row());

        switch (role)
        {
            case IsMemberRole:
                return !m_subjectIsUser
                    && m_subjectContext->subjectHierarchy()->directMembers(m_subjectId)
                        .contains(userId);

            case IsParentRole:
                return false;

            case IsTopLevelRole:
                return true;

            case IsAllowedMember:
                return !m_subjectIsUser && !info(userId).isLdap;

            case IsAllowedParent:
                return false;

            default:
                return getUserData(0, userId, role);
        }
    }

    const auto [topLevelGroupId, rowInGroup] =
        findGroupRow(m_allGroups, index.row() - m_allUsers.size());

    if (rowInGroup == -1)
        return {};

    switch (role)
    {
        case IsMemberRole:
            return m_subjectContext->subjectHierarchy()->directMembers(m_subjectId)
                .contains(topLevelGroupId);

        case IsParentRole:
            return m_subjectContext->subjectHierarchy()->directMembers(topLevelGroupId)
                .contains(m_subjectId);

        case IsTopLevelRole:
            return rowInGroup == 0;

        case IsAllowedMember:
            return !m_subjectIsUser
                && !isPredefined(topLevelGroupId)
                && isAllowedMember(topLevelGroupId)
                && !info(topLevelGroupId).isLdap;

        case IsAllowedParent:
            return isAllowedParent(topLevelGroupId) && !info(topLevelGroupId).isLdap;

        default:
            return getData(0, topLevelGroupId, rowInGroup, role);
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

    if (info(selectedId).isLdap)
        return false;

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
    return m_itemCount;
}

QVariant MembersModel::getParentsTree(const QnUuid& groupId) const
{
    QVariantList result;
    const std::function<void(QnUuid, int)> visit =
        [&result, &visit, this](QnUuid id, int offset)
        {
            const auto& parentIds = m_subjectContext->subjectHierarchy()->directParents(id);

            for (const auto& parentId: parentIds)
            {
                QVariantMap item;
                item["text"] = info(parentId).name;
                item["offset"] = offset;
                result << item;
                visit(parentId, offset + 1);
            }
        };

    visit(groupId, 1);
    return result;
}

QList<MembersModelGroup> MembersModel::parentGroups() const
{
    if (m_subjectContext.isNull())
        return {};

    auto directParents = m_subjectContext->subjectHierarchy()->directParents(m_subjectId).values();
    sortSubjects(directParents, systemContext());

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
        return sourceModel()->data(sourceIndex, MembersModel::IsTopLevelRole).toBool()
            && sourceModel()->data(sourceIndex, MembersModel::IsAllowedMember).toBool()
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

        if (!filterRegularExpression().pattern().isEmpty()
            && !sourceModel()->data(sourceIndex, MembersModel::IsTopLevelRole).toBool())
        {
            return false;
        }

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
        return sourceModel()->data(sourceIndex, MembersModel::IsTopLevelRole).toBool()
            && sourceModel()->data(sourceIndex, MembersModel::IsAllowedParent).toBool()
            && base_type::filterAcceptsRow(sourceRow, sourceParent);
    }
};

void MembersModel::registerQmlType()
{
    qRegisterMetaType<nx::vms::client::desktop::MembersModelGroup>();
    QMetaType::registerComparators<QList<nx::vms::client::desktop::MembersModelGroup>>();

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
