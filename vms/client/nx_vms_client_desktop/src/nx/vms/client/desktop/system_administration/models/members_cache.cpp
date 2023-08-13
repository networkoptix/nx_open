// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "members_cache.h"

#include <core/resource/user_resource.h>
#include <core/resource_access/subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

using nx::vms::common::PredefinedUserGroups;

MembersCache::MembersCache()
{
}

MembersCache::~MembersCache()
{
}

MembersCache::Info MembersCache::info(const QnUuid& id) const
{
    return m_info.value(id);
}

bool MembersCache::isPredefined(const QnUuid& groupId)
{
    return PredefinedUserGroups::contains(groupId);
}

MembersCache::Info MembersCache::infoFromContext(
    nx::vms::common::SystemContext* systemContext,
    const QnUuid& id)
{
    if (const auto group = systemContext->userGroupManager()->find(id))
    {
        return {
            .name = group->name,
            .description = group->description,
            .isGroup = true,
            .isLdap = (group->type == nx::vms::api::UserType::ldap)};
    }

    if (const auto user = systemContext->resourcePool()->getResourceById<QnUserResource>(id))
    {
        return {
            .name = user->getName(),
            .description = user->fullName(),
            .isGroup = false,
            .isLdap = user->isLdap(),
            .isTemporary = user->isTemporary()};
    }

    return {};
}

void MembersCache::loadInfo(nx::vms::common::SystemContext* systemContext)
{
    m_systemContext = systemContext;

    m_info.clear();
    m_sortedCache.clear();
    m_stats = {};
    emit reset();

    Members members;

    const auto allGroups = systemContext->userGroupManager()->groups();
    for (const auto& group: allGroups)
    {
        const auto id = group.getId();
        m_info.insert(id, infoFromContext(systemContext, id));
        members.groups << id;
    }

    const auto allUsers = systemContext->resourcePool()->getResources<QnUserResource>();
    for (const auto& user: allUsers)
    {
        const auto id = user->getId();
        m_info.insert(id, infoFromContext(systemContext, id));
        members.users << id;
    }

    sortSubjects(members.users);
    sortSubjects(members.groups);

    m_sortedCache.insert(QnUuid{}, members);

    updateStats(nx::utils::toQSet(members.groups), {});
}

std::function<bool(const QnUuid&, const QnUuid&)> MembersCache::lessFunc() const
{
    return [info = m_info](const QnUuid& l, const QnUuid& r)
        {
            const auto leftInfo = info.value(l);
            const auto rightInfo = info.value(r);

            // Users go first.
            if (leftInfo.isGroup != rightInfo.isGroup)
                return rightInfo.isGroup;

            // Predefined groups go first.
            const auto predefinedLeft = isPredefined(l);
            const auto predefinedRight = isPredefined(r);
            if (predefinedLeft != predefinedRight)
            {
                return predefinedLeft;
            }
            else if (predefinedLeft)
            {
                const auto predefList = PredefinedUserGroups::ids();
                return predefList.indexOf(l) < predefList.indexOf(r);
            }

            // TODO sort cloud users.

            // LDAP groups go last.
            if (leftInfo.isLdap != rightInfo.isLdap)
                return rightInfo.isLdap;

            // "LDAP Default" goes in front of all LDAP groups.
            if (l == nx::vms::api::kDefaultLdapGroupId || r == nx::vms::api::kDefaultLdapGroupId)
            {
                // When doing a search, both l and r can be kDefaultLdapGroupId.
                return l == nx::vms::api::kDefaultLdapGroupId
                    && r != nx::vms::api::kDefaultLdapGroupId;
            }

            // Case Insensitive sort.
            const int ret = nx::utils::naturalStringCompare(
                leftInfo.name, rightInfo.name, Qt::CaseInsensitive);

            // Sort identical names by UUID.
            if (ret == 0)
                return l < r;

            return ret < 0;
        };
}

void MembersCache::sortSubjects(QList<QnUuid>& subjects) const
{
    if (!m_subjectContext)
        return;

    std::sort(subjects.begin(), subjects.end(), lessFunc());
}

MembersCache::Members MembersCache::sortedSubjects(const QSet<QnUuid>& subjectSet) const
{
    Members result;

    for (const auto& id: subjectSet)
        (info(id).isGroup ? result.groups : result.users) << id;

    sortSubjects(result.users);
    sortSubjects(result.groups);

    return result;
}

MembersCache::Members MembersCache::sorted(const QnUuid& groupId) const
{
    const auto it = m_sortedCache.constFind(groupId);
    if (it != m_sortedCache.end())
        return *it;

    const auto members = m_subjectContext->subjectHierarchy()->directMembers(groupId);
    const auto result = sortedSubjects(members);
    m_sortedCache[groupId] = result;

    return result;
}

int MembersCache::indexIn(const QList<QnUuid>& list, const QnUuid& id) const
{
    auto it = std::lower_bound(list.begin(), list.end(), id, lessFunc());
    return std::distance(list.begin(), it);
}

void MembersCache::modify(
    const QSet<QnUuid>& added,
    const QSet<QnUuid>& removed,
    const QSet<QnUuid>& groupsWithChangedMembers,
    const QSet<QnUuid>& subjectsWithChangedParents)
{
    const auto removeIn = [this](const QnUuid& groupId, const QnUuid& id, bool isGroup)
        {
            QList<QnUuid>& idList = isGroup
                ? m_sortedCache[groupId].groups
                : m_sortedCache[groupId].users;

            const int i = indexIn(idList, id);
            if (i != idList.size() && idList.at(i) == id)
            {
                emit beginRemove(i, id, groupId);
                idList.removeAt(i);
                emit endRemove(i, id, groupId);
            }
        };

    const auto addTo = [this](const QnUuid& groupId, const QnUuid& id, bool isGroup)
        {
            QList<QnUuid>& idList = isGroup
                ? m_sortedCache[groupId].groups
                : m_sortedCache[groupId].users;

            const int i = indexIn(idList, id);
            if (i == idList.size() || idList.at(i) != id)
            {
                emit beginInsert(i, id, groupId);
                idList.insert(i, id);
                emit endInsert(i, id, groupId);
            }
        };

    for (const auto& id: removed)
    {
        const auto idInfo = info(id);

        for (const auto& groupId: m_sortedCache.keys())
        {
            if (!groupId.isNull()) //< Delay removal from top level.
                removeIn(groupId, id, idInfo.isGroup);
        }

        // Remove from top level.
        removeIn({}, id, idInfo.isGroup);
    }

    for (const auto& id: added)
    {
        const auto idInfo = (m_info[id] = infoFromContext(m_systemContext, id));

        addTo({}, id, idInfo.isGroup);
        const auto parents = m_subjectContext->subjectHierarchy()->directParents(id);
        for (const auto& groupId: parents)
            addTo(groupId, id, idInfo.isGroup);
    }

    for (const auto& id: subjectsWithChangedParents)
    {
        const bool isGroup = info(id).isGroup;
        const auto parents = m_subjectContext->subjectHierarchy()->directParents(id);
        const auto removedFrom = groupsWithChangedMembers - parents;
        auto addedTo = groupsWithChangedMembers;
        addedTo.intersect(parents);

        for (const auto& groupId: removedFrom)
            removeIn(groupId, id, isGroup);

        for (const auto& groupId: addedTo)
            addTo(groupId, id, isGroup);
    }

    for (const auto& id: (removed - added))
    {
        m_sortedCache.remove(id);
        m_info.remove(id);
    }

    updateStats(added, removed);
}

void MembersCache::updateStats(const QSet<QnUuid>& added, const QSet<QnUuid>& removed)
{
    bool statsModified = false;

    const auto countGroups =
        [this, &statsModified](const QnUuid& id, int diff)
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
