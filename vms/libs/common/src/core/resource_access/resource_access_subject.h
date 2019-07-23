#pragma once

#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_fwd.h>
#include <nx/vms/api/data/user_role_data.h>
#include <core/resource/user_resource.h>

struct QnResourceAccessSubjectPrivate;

/**
* This class represents subject of resource access - user or user role.
*/
class QnResourceAccessSubject final
{
public:
    QnResourceAccessSubject();
    QnResourceAccessSubject(const QnUserResourcePtr& user);
    QnResourceAccessSubject(const nx::vms::api::UserRoleData& role);
    QnResourceAccessSubject(const QnResourceAccessSubject& other);

    const QnUserResourcePtr& user() const { return m_user; }
    nx::vms::api::GlobalPermissions rolePermissions() const { return m_rolePermissions; }

    bool isValid() const {return !m_id.isNull(); }
    bool isUser() const { return !m_user.isNull(); }
    bool isRole() const { return m_user.isNull(); }
    const QnUuid& id() const { return m_id; }

    /** Key value in the shared resources map. */
    QnUuid effectiveId() const
    {
        return m_user && m_user->userRole() == Qn::UserRole::customUserRole
            ? m_user->userRoleId() : m_id;
    }
    QString name() const;

    void operator=(const QnResourceAccessSubject& other);
    bool operator==(const QnResourceAccessSubject& other) const;
    bool operator!=(const QnResourceAccessSubject& other) const;

private:
    QnUserResourcePtr m_user;
    nx::vms::api::GlobalPermissions m_rolePermissions;
    QnUuid m_id;
};

QDebug operator<<(QDebug dbg, const QnResourceAccessSubject& subject);

Q_DECLARE_METATYPE(QnResourceAccessSubject)
