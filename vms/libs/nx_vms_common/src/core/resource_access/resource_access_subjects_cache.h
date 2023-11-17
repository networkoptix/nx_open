// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>
#include <unordered_set>

#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context_aware.h>

/**
 * The cache to access users and child roles by specific role id.
 */
class NX_VMS_COMMON_API QnResourceAccessSubjectsCache:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnResourceAccessSubjectsCache(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);

    /** List of all subjects of the resources access: users and roles (excl. predefined). */
    std::unordered_set<QnResourceAccessSubject> allSubjects() const;

    /** List of all subjects included in the role (without recursion, predefined roles are also supported). */
    std::unordered_set<QnResourceAccessSubject> subjectsInRole(const QnUuid& roleId) const;

    /** List of users, belonging to given role (including recursive roles, predefined roles are also supported). */
    std::unordered_set<QnResourceAccessSubject> allUsersInRole(const QnUuid& roleId) const;

    /** List of all subjects included in the role (with recursion). */
    std::unordered_set<QnResourceAccessSubject> allSubjectsInRole(const QnUuid& roleId) const;

    /** All parent roles including inheritance. */
    // This function returns std::vector<QnUuid> instead of
    // std::unordered_set<QnResourceAccessSubject> due to significant performance difference
    // in Resource Access Providers in cache mode.
    std::vector<QnUuid> subjectWithParents(const QnResourceAccessSubject& subject) const;

private:
    void handleUserAdded(const QnUserResourcePtr& user);
    void handleUserRemoved(const QnUserResourcePtr& user);

    void handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);

    void removeUserFromRole(const QnUserResourcePtr& user, const QnUuid& roleId);

    void updateSubjectRoles(
        const QnResourceAccessSubject& subject,
        const std::vector<QnUuid>& newRoleIds,
        GlobalPermissions rawPermissions);
    void updateSubjectRoles(
        const QnResourceAccessSubject& subject,
        const std::vector<QnUuid>& newRoleIds,
        GlobalPermissions rawPermissions,
        const nx::MutexLocker& lock);

    template<typename Action>
    void forEachSubjectInRole(
        const QnUuid& roleId, const Action& action,
        const nx::MutexLocker& lock) const;

private:
    mutable nx::Mutex m_mutex;

    std::unordered_set<QnResourceAccessSubject> m_subjects;
    std::unordered_map<QnUuid, std::unordered_set<QnResourceAccessSubject>> m_subjectsInRole;
    std::unordered_map<QnUuid, std::unordered_set<QnResourceAccessSubject>> m_rolesOfSubject;
};
