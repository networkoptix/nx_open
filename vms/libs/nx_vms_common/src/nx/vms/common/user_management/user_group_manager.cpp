// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_group_manager.h"

#include <unordered_map>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/user_management/predefined_user_groups.h>

using namespace nx::vms::api;

namespace nx::vms::common {

struct UserGroupManager::Private
{
    mutable nx::Mutex mutex;
    std::unordered_map<nx::Uuid, UserGroupData> customGroups;
};

UserGroupManager::UserGroupManager(QObject* parent):
    QObject(parent),
    d(new Private{})
{
}

UserGroupManager::~UserGroupManager()
{
    // Required here for forward-declared scoped pointer destruction.
}

std::unordered_map<nx::Uuid, UserGroupData> UserGroupManager::customGroups(
    std::function<bool(const nx::vms::api::UserGroupData&)> predicate) const
{
    std::unordered_map<nx::Uuid, UserGroupData> result;
    NX_MUTEX_LOCKER lk(&d->mutex);
    for (const auto& it: d->customGroups)
    {
        if (predicate(it.second))
            result.insert(it);
    }
    return result;
}

std::vector<nx::Uuid> UserGroupManager::customGroupIds(
    std::function<bool(const nx::vms::api::UserGroupData&)> predicate) const
{
    std::vector<nx::Uuid> result;
    NX_MUTEX_LOCKER lk(&d->mutex);
    result.reserve(d->customGroups.size());
    for (const auto& [id, data]: d->customGroups)
    {
        if (predicate(data))
            result.push_back(id);
    }
    return result;
}

UserGroupDataList UserGroupManager::groups(Selection types) const
{
    UserGroupDataList result;
    if (types != Selection::custom)
        result = PredefinedUserGroups::list();

    if (types == Selection::predefined)
        return result;

    NX_MUTEX_LOCKER lk(&d->mutex);
    result.reserve(result.size() + d->customGroups.size());

    for (const auto& [_, group]: d->customGroups)
        result.push_back(group);

    return result;
}

QList<nx::Uuid> UserGroupManager::ids(Selection types) const
{
    QList<nx::Uuid> result;
    if (types != Selection::custom)
        result = PredefinedUserGroups::ids();

    if (types == Selection::predefined)
        return result;

    NX_MUTEX_LOCKER lk(&d->mutex);
    result.reserve(result.size() + d->customGroups.size());

    for (const auto& [id, _]: d->customGroups)
        result.push_back(id);

    return result;
}

bool UserGroupManager::contains(const nx::Uuid& groupId) const
{
    if (PredefinedUserGroups::contains(groupId))
        return true;

    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->customGroups.contains(groupId);
}

std::optional<UserGroupData> UserGroupManager::find(const nx::Uuid& groupId) const
{
    if (auto predefinedGroup = PredefinedUserGroups::find(groupId))
        return predefinedGroup;

    NX_MUTEX_LOCKER lk(&d->mutex);
    if (const auto it = d->customGroups.find(groupId); it != d->customGroups.end())
        return it->second;

    return std::nullopt;
}

void UserGroupManager::resetAll(const UserGroupDataList& groups)
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    d->customGroups = {};

    for (const auto& group: groups)
    {
        if (group.id.isNull() || PredefinedUserGroups::contains(group.id))
            continue;

        if (const auto it = d->customGroups.find(group.id);
            it != d->customGroups.end() && it->second != group)
        {
            NX_WARNING(this, "User groups with the same id: %1, %2", it->second, group);
        }

        d->customGroups.insert_or_assign(group.id, group);
    }

    const auto size = d->customGroups.size();
    lk.unlock();

    NX_DEBUG(this, "User groups reset: there are %1 custom groups", size);
    emit reset();
}

bool UserGroupManager::addOrUpdate(const UserGroupData& group)
{
    if (group.id.isNull() || PredefinedUserGroups::contains(group.id))
        return false;

    NX_MUTEX_LOCKER lk(&d->mutex);

    const auto it = d->customGroups.find(group.id);
    const bool existing = it != d->customGroups.end();

    if (existing && it->second == group)
        return false; //< No changes.

    d->customGroups.insert_or_assign(it, group.id, group);
    lk.unlock();

    if (existing)
        NX_DEBUG(this, "Custom user group updated: %1", group);
    else
        NX_DEBUG(this, "Custom user group added: %1", group);

    emit addedOrUpdated(group);

    return true;
}

bool UserGroupManager::remove(const nx::Uuid& groupId)
{
    if (groupId.isNull() || PredefinedUserGroups::contains(groupId))
        return false;

    NX_MUTEX_LOCKER lk(&d->mutex);

    const auto it = d->customGroups.find(groupId);
    if (it == d->customGroups.end())
        return false;

    const UserGroupData group = std::move(it->second);
    d->customGroups.erase(it);
    lk.unlock();

    NX_DEBUG(this, "Custom user group removed: %1", group);
    emit removed(group);

    return true;
}

} // namespace nx::vms::common
