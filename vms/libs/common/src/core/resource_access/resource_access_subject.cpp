#include "resource_access_subject.h"

#include <core/resource/user_resource.h>

#include <nx/vms/api/data/user_role_data.h>

struct QnResourceAccessSubjectPrivate
{
public:
    QnResourceAccessSubjectPrivate(
        const QnUserResourcePtr& user,
        const nx::vms::api::UserRoleData& role)
        :
        user(user),
        role(role),
        m_id(user ? user->getId() : role.id)
    {
    }

    bool isValid() const
    {
        return !m_id.isNull();
    }

    QnUuid id() const
    {
        return m_id;
    }

    QnUuid effectiveId() const
    {
        QnUuid key = m_id;
        if (user && user->userRole() == Qn::UserRole::customUserRole)
            key = user->userRoleId();

        return key;
    }

    void operator=(const QnResourceAccessSubjectPrivate& other)
    {
        user = other.user;
        role = other.role;
        m_id = other.m_id;
    }

    QnUserResourcePtr user;
    nx::vms::api::UserRoleData role;

private:
    QnUuid m_id;
};

QnResourceAccessSubject::QnResourceAccessSubject(const QnUserResourcePtr& user):
    d_ptr(new QnResourceAccessSubjectPrivate(user, {}))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const nx::vms::api::UserRoleData& role):
    d_ptr(new QnResourceAccessSubjectPrivate(QnUserResourcePtr(), role))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const QnResourceAccessSubject& other):
    d_ptr(new QnResourceAccessSubjectPrivate(other.user(), other.role()))
{
}

QnResourceAccessSubject::QnResourceAccessSubject():
    d_ptr(new QnResourceAccessSubjectPrivate(QnUserResourcePtr(), {}))
{
}

QnResourceAccessSubject::~QnResourceAccessSubject()
{
}

const QnUserResourcePtr& QnResourceAccessSubject::user() const
{
    return d_ptr->user;
}

const nx::vms::api::UserRoleData& QnResourceAccessSubject::role() const
{
    return d_ptr->role;
}

bool QnResourceAccessSubject::isValid() const
{
    return d_ptr->isValid();
}

bool QnResourceAccessSubject::isUser() const
{
    return !d_ptr->user.isNull();
}

bool QnResourceAccessSubject::isRole() const
{
    return !d_ptr->role.isNull();
}

QnUuid QnResourceAccessSubject::id() const
{
    return d_ptr->id();
}

QnUuid QnResourceAccessSubject::effectiveId() const
{
    return d_ptr->effectiveId();
}

QString QnResourceAccessSubject::name() const
{
    return d_ptr->user ? d_ptr->user->getName() : d_ptr->role.name;
}

void QnResourceAccessSubject::operator=(const QnResourceAccessSubject& other)
{
    *d_ptr = *(other.d_ptr);
}

bool QnResourceAccessSubject::operator!=(const QnResourceAccessSubject& other) const
{
    return !(*this == other);
}

bool QnResourceAccessSubject::operator==(const QnResourceAccessSubject& other) const
{
    return d_ptr->id() == other.id();
}

QDebug operator<<(QDebug dbg, const QnResourceAccessSubject& subject)
{
    dbg.nospace() << "QnResourceAccessSubject(" << subject.name() << ")";
    return dbg.space();
}
