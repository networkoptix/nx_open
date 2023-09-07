// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

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
    bool containsGroup(const QnUuid& groupId) const;

    qsizetype userCount() const;
    qsizetype groupCount() const;

    QSet<QnUuid> groups() const;

    bool canDelete(const QnUserResourcePtr& user) const;
    bool canEnableDisable(const QnUserResourcePtr& user) const;
    bool canChangeAuthentication(const QnUserResourcePtr& user) const;
    bool canEdit(const QnUserResourcePtr& user);

signals:
    void userModified(const QnUserResourcePtr& user);
    void groupModified(const QnUuid& groupId);

private:
    void addUser(const QnUserResourcePtr& user);
    void removeUser(const QnUserResourcePtr& user);

    void addGroup(const QnUuid& id);
    void removeGroup(const QnUuid& id);

    void modifyGroups(const QSet<QnUuid> ids, int diff);
    void addOrUpdateUser(const QnUserResourcePtr& user);
    void addOrUpdateGroup(const nx::vms::api::UserGroupData& group);

    bool canDelete(const nx::vms::api::UserGroupData& group);

    void updateMembers(const QnUuid& groupId);

private:
    QHash<QnUserResourcePtr, QSet<QnUuid>> m_nonEditableUsers; //< Maps user to parentIds.
    QHash<QnUuid, QSet<QnUuid>> m_nonEditableGroups; //< Maps group to parentIds.

    //< Maps group to the number of its non-editable members.
    QHash<QnUuid, int> m_groupsWithNonEditableMembers;
};

} // namespace nx::vms::client::desktop
