// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

#include "private/non_unique_name_tracker.h"

namespace nx::vms::api { struct UserGroupData; }

namespace nx::vms::client::desktop {

class SystemContext;

/**
 * Watches users and groups that become non-editable (disabled checkbox in users/groups tables)
 * This happens in 2 cases: either own/inherited permissions change or (for groups) some members
 * become non-editable.
 */
class NX_VMS_CLIENT_DESKTOP_API NonEditableUsersAndGroups:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

    using base_type = SystemContextAware;

public:
    NonEditableUsersAndGroups(SystemContext* systemContext);
    ~NonEditableUsersAndGroups();

    bool containsUser(const QnUserResourcePtr& user) const;
    bool containsGroup(const nx::Uuid& groupId) const;

    qsizetype userCount() const;
    qsizetype groupCount() const;

    QSet<nx::Uuid> groups() const;

    bool canDelete(const QnUserResourcePtr& user) const;
    bool canEnableDisable(const QnUserResourcePtr& user) const;
    bool canChangeAuthentication(const QnUserResourcePtr& user) const;
    bool canMassEdit(const QnUserResourcePtr& user) const;

    // Checks if user or group parents can be changed.
    bool canEditParents(const nx::Uuid& id) const;

    const NonUniqueNameTracker& nonUniqueUsers() const { return m_nonUniqueUserTracker; }
    const NonUniqueNameTracker& nonUniqueGroups() const { return m_nonUniqueGroupTracker; }

    QString tooltip(const nx::Uuid& id) const;

signals:
    void userModified(const QnUserResourcePtr& user);
    void groupModified(const nx::Uuid& groupId);
    void nonUniqueUsersChanged();
    void nonUniqueGroupsChanged();

private:
    bool addUser(const QnUserResourcePtr& user);
    bool removeUser(const QnUserResourcePtr& user);

    bool addGroup(const nx::Uuid& id);
    bool removeGroup(const nx::Uuid& id);

    void modifyGroups(const QSet<nx::Uuid>& ids, int diff);
    void addOrUpdateUser(const QnUserResourcePtr& user);
    void addOrUpdateGroup(const nx::vms::api::UserGroupData& group);

    bool canDelete(const nx::vms::api::UserGroupData& group);

    void updateMembers(const nx::Uuid& groupId);

    QSet<nx::Uuid> nonUniqueWithEditableParents();

private:
    QSet<QnUserResourcePtr> m_nonMassEditableUsers; //< Disabled checkbox when mass editing.
    QSet<nx::Uuid> m_usersWithUnchangableParents; //< Disabled checkbox in Members tab.

    NonUniqueNameTracker m_nonUniqueUserTracker;
    NonUniqueNameTracker m_nonUniqueGroupTracker;

    QHash<QnUserResourcePtr, QSet<nx::Uuid>> m_nonEditableUsers; //< Maps user to parentIds.
    QHash<nx::Uuid, QSet<nx::Uuid>> m_nonEditableGroups; //< Maps group to parentIds, disabled checkbox in Members tab.
    QSet<nx::Uuid> m_nonRemovableGroups; //< Disabled checkbox when mass editing.

    // Maps group to the number of its members that prevents group removal.
    QHash<nx::Uuid, int> m_groupsWithNonEditableMembers;
};

} // namespace nx::vms::client::desktop
