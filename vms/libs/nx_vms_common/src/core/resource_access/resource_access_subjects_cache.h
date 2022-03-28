// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/resource_access_subject.h>
#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context_aware.h>

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
    QList<QnResourceAccessSubject> allSubjects() const;

    /** List of users, belonging to given role (predefined roles are also supported). */
    QList<QnResourceAccessSubject> usersInRole(const QnUuid& roleId) const;

private:
    void handleUserAdded(const QnUserResourcePtr& user);
    void handleUserRemoved(const QnUserResourcePtr& user);
    void updateUserRole(const QnUserResourcePtr& user);

    void handleRoleAdded(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);

    void removeUserFromRole(const QnUserResourcePtr& user, const QnUuid& roleId);

private:
    mutable nx::Mutex m_mutex;

    QList<QnResourceAccessSubject> m_subjects;
    QHash<QnUuid, QnUuid> m_roleIdByUserId;
    QHash<QnUuid, QList<QnResourceAccessSubject>> m_usersByRoleId;
};
