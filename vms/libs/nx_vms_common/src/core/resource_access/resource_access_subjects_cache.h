// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_module_aware.h>

#include <core/resource_access/resource_access_subject.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>

class NX_VMS_COMMON_API QnResourceAccessSubjectsCache:
    public Connective<QObject>,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    QnResourceAccessSubjectsCache(QObject* parent);

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
