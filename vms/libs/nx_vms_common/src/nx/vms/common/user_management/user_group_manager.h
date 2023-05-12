// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <unordered_map>

#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/user_group_data.h>

namespace nx::vms::common {

class NX_VMS_COMMON_API UserGroupManager: public QObject
{
    Q_OBJECT

public:
    explicit UserGroupManager(QObject* parent = nullptr);
    virtual ~UserGroupManager() override;

    enum class Selection
    {
        predefined,
        custom,
        all
    };

    std::unordered_map<QnUuid, nx::vms::api::UserGroupData> customGroups(
        std::function<bool(const nx::vms::api::UserGroupData&)> predicate) const;

    /** Returns list of all user groups of specified types. */
    nx::vms::api::UserGroupDataList groups(Selection types = Selection::all) const;

    /** Returns list of all user group ids of specified types. */
    QList<QnUuid> ids(Selection types = Selection::all) const;

    /** Returns whether a predefined or custom user group specified by id exists. */
    bool contains(const QnUuid& groupId) const;

    /** Returns predefined or custom user group data by specified id, if exists. */
    std::optional<nx::vms::api::UserGroupData> find(const QnUuid& groupId) const;

    /** Returns predefined or custom user group data list by specified id list. */
    template<class IDList>
    nx::vms::api::UserGroupDataList getGroupsByIds(const IDList& idList) const;

    /** Sets all custom user groups. */
    void resetAll(const nx::vms::api::UserGroupDataList& groups);

    /** Adds or updates a custom user group. */
    bool addOrUpdate(const nx::vms::api::UserGroupData& group);

    /** Removes specified custom user group. */
    bool remove(const QnUuid& groupId);

signals:
    void reset();
    void addedOrUpdated(const nx::vms::api::UserGroupData& group);
    void removed(const nx::vms::api::UserGroupData& group);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

// ------------------------------------------------------------------------------------------------
// Template function implementation

template<class IDList>
nx::vms::api::UserGroupDataList UserGroupManager::getGroupsByIds(const IDList& idList) const
{
    nx::vms::api::UserGroupDataList result;
    for (const auto& id: idList)
    {
        if (auto group = find(id))
            result.push_back(std::move(*group));
    }

    return result;
}

} // namespace nx::vms::common
