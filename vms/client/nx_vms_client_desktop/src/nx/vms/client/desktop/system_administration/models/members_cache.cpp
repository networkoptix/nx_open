// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "members_cache.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

#include "members_sort.h"

namespace nx::vms::client::desktop {

namespace {

class ComparableId: public ComparableMember<ComparableId>
{
    const nx::Uuid& infoId;
    const MembersCache::Info& info;

public:
    ComparableId(const nx::Uuid& infoId, const MembersCache::Info& info):
        infoId(infoId), info(info)
    {
    }

    nx::Uuid id() const { return infoId; }
    bool isGroup() const { return info.isGroup; }
    nx::vms::api::UserType userType() const { return info.userType; }
    QString name() const { return info.name; }
};

} // namespace

MembersCache::MembersCache()
{
}

MembersCache::~MembersCache()
{
}

MembersCache::Info MembersCache::info(const nx::Uuid& id) const
{
    return m_info.value(id);
}

MembersCache::Info MembersCache::infoFromContext(
    nx::vms::common::SystemContext* systemContext,
    const nx::Uuid& id)
{
    if (const auto group = systemContext->userGroupManager()->find(id))
    {
        NX_ASSERT(!nx::vms::common::isGroupHidden(*group));

        return {
            .name = group->name,
            .description = group->description,
            .isGroup = true,
            .userType = group->type};
    }

    if (const auto user = systemContext->resourcePool()->getResourceById<QnUserResource>(id))
    {
        NX_ASSERT(!nx::vms::common::isUserHidden(user));

        return {
            .name = user->getName(),
            .description = user->fullName(),
            .isGroup = false,
            .userType = user->userType()};
    }

    return {};
}

void MembersCache::loadInfo(nx::vms::common::SystemContext* systemContext)
{
    m_systemContext = systemContext;

    m_info.clear();
    m_sortedCache.clear();
    m_stats = {};
    m_tmpUsersInGroup.clear();
    m_tmpUsers.clear();
    emit reset();

    Members members;

    const auto allGroups = systemContext->userGroupManager()->groups();
    for (const auto& group: allGroups)
    {
        if (nx::vms::common::isGroupHidden(group))
            continue;

        const auto id = group.getId();
        m_info.insert(id, infoFromContext(systemContext, id));
        members.groups << id;
    }

    const auto allUsers = systemContext->resourcePool()->getResources<QnUserResource>();
    for (const auto& user: allUsers)
    {
        if (nx::vms::common::isUserHidden(user))
            continue;

        const auto id = user->getId();
        m_info.insert(id, infoFromContext(systemContext, id));
        members.users << id;

        if (m_info[id].userType == api::UserType::temporaryLocal)
        {
            for (const auto& groupId: m_subjectContext->subjectHierarchy()->directParents(id))
                addTmpUser(groupId, id);

            addTmpUser({}, id);
        }
    }

    sortSubjects(members.users);
    sortSubjects(members.groups);

    m_sortedCache.insert(nx::Uuid{}, members);

    updateStats(nx::utils::toQSet(members.groups), {});
}

std::function<bool(const nx::Uuid&, const nx::Uuid&)> MembersCache::lessFunc() const
{
    return [info = m_info](const nx::Uuid& l, const nx::Uuid& r)
        {
            return ComparableId(l, info.value(l)) < ComparableId(r, info.value(r));
        };
}

void MembersCache::sortSubjects(QList<nx::Uuid>& subjects) const
{
    if (!m_subjectContext)
        return;

    std::sort(subjects.begin(), subjects.end(), lessFunc());
}

MembersCache::Members MembersCache::sortedSubjects(const QSet<nx::Uuid>& subjectSet) const
{
    Members result;

    for (const auto& id: subjectSet)
        (info(id).isGroup ? result.groups : result.users) << id;

    sortSubjects(result.users);
    sortSubjects(result.groups);

    return result;
}

MembersCache::Members MembersCache::sorted(const nx::Uuid& groupId) const
{
    const auto it = m_sortedCache.constFind(groupId);
    if (it != m_sortedCache.end())
        return *it;

    const auto members = m_subjectContext->subjectHierarchy()->directMembers(groupId);
    const auto result = sortedSubjects(members);
    m_sortedCache[groupId] = result;

    return result;
}

int MembersCache::indexIn(const QList<nx::Uuid>& list, const nx::Uuid& id) const
{
    auto it = std::lower_bound(list.begin(), list.end(), id, lessFunc());
    return std::distance(list.begin(), it);
}

void MembersCache::modify(
    const QSet<nx::Uuid>& added,
    const QSet<nx::Uuid>& removed,
    const QSet<nx::Uuid>& groupsWithChangedMembers,
    const QSet<nx::Uuid>& subjectsWithChangedParents)
{
    const auto removeFrom =
        [this](const nx::Uuid& groupId, const nx::Uuid& id, const Info& idInfo)
        {
            QList<nx::Uuid>& idList = idInfo.isGroup
                ? m_sortedCache[groupId].groups
                : m_sortedCache[groupId].users;

            const int i = indexIn(idList, id);
            if (i != idList.size() && idList.at(i) == id)
            {
                emit beginRemove(i, id, groupId);
                idList.removeAt(i);
                emit endRemove(i, id, groupId);

                if (!idInfo.isGroup && idInfo.userType == api::UserType::temporaryLocal)
                    addTmpUser(groupId, id);
            }
        };

    const auto addTo =
        [this](const nx::Uuid& groupId, const nx::Uuid& id, const Info& idInfo)
        {
            QList<nx::Uuid>& idList = idInfo.isGroup
                ? m_sortedCache[groupId].groups
                : m_sortedCache[groupId].users;

            const int i = indexIn(idList, id);
            if (i == idList.size() || idList.at(i) != id)
            {
                emit beginInsert(i, id, groupId);
                idList.insert(i, id);
                emit endInsert(i, id, groupId);

                if (!idInfo.isGroup && idInfo.userType == api::UserType::temporaryLocal)
                    removeTmpUser(groupId, id);
            }
        };

    for (const auto& id: removed)
    {
        const auto idInfo = info(id);

        for (const auto& groupId: m_sortedCache.keys())
        {
            if (!groupId.isNull()) //< Delay removal from top level.
                removeFrom(groupId, id, idInfo);
        }

        // Remove from top level.
        removeFrom({}, id, idInfo);
    }

    for (const auto& id: added)
    {
        const auto idInfo = (m_info[id] = infoFromContext(m_systemContext, id));

        addTo({}, id, idInfo);
        const auto parents = m_subjectContext->subjectHierarchy()->directParents(id);
        for (const auto& groupId: parents)
            addTo(groupId, id, idInfo);
    }

    for (const auto& id: subjectsWithChangedParents)
    {
        const auto idInfo = info(id);
        const auto parents = m_subjectContext->subjectHierarchy()->directParents(id);
        const auto removedFrom = groupsWithChangedMembers - parents;
        auto addedTo = groupsWithChangedMembers;
        addedTo.intersect(parents);

        for (const auto& groupId: removedFrom)
            removeFrom(groupId, id, idInfo);

        for (const auto& groupId: addedTo)
            addTo(groupId, id, idInfo);
    }

    for (const auto& id: (removed - added))
    {
        m_sortedCache.remove(id);
        m_info.remove(id);
    }

    updateStats(added, removed);
}

void MembersCache::addTmpUser(const nx::Uuid& groupId, const nx::Uuid& userId)
{
    if (groupId.isNull())
    {
        m_tmpUsers.insert(userId);
        return;
    }

    const int count = m_tmpUsersInGroup.value(groupId, 0) + 1;
    m_tmpUsersInGroup[groupId] = count;
}

void MembersCache::removeTmpUser(const nx::Uuid& groupId, const nx::Uuid& userId)
{
    if (groupId.isNull())
    {
        m_tmpUsers.remove(userId);
        return;
    }

    const int count = m_tmpUsersInGroup.value(groupId, 0) - 1;
    if (count <= 0)
        m_tmpUsersInGroup.remove(groupId);
    else
        m_tmpUsersInGroup[groupId] = count;
}

QList<int> MembersCache::temporaryUserIndexes() const
{
    const auto users = sorted().users;

    QList<int> result;
    result.reserve(m_tmpUsers.size());

    for (const auto& id: m_tmpUsers)
    {
        const int i = indexIn(users, id);
        if (i != users.size() && users.at(i) == id)
            result.push_back(i);
    }

    return result;
}

void MembersCache::updateStats(const QSet<nx::Uuid>& added, const QSet<nx::Uuid>& removed)
{
    bool statsModified = false;

    const auto countGroups =
        [this, &statsModified](const nx::Uuid& id, int diff)
        {
            const auto group = m_subjectContext->systemContext()->userGroupManager()->find(id);
            if (!group || group->attributes.testFlag(nx::vms::api::UserAttribute::readonly))
                return;

            statsModified = true;
            switch (group->type)
            {
                case api::UserType::local:
                    m_stats.localGroups += diff;
                    break;

                case api::UserType::cloud:
                    m_stats.cloudGroups += diff;
                    break;

                case api::UserType::ldap:
                    m_stats.ldapGroups += diff;
                    break;
            }
        };

    for (const auto& id: removed)
        countGroups(id, -1);

    for (const auto& id: added)
        countGroups(id, 1);

    if (statsModified)
        emit statsChanged();
}

} // namespace nx::vms::client::desktop
