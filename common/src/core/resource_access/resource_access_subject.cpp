#include "resource_access_subject.h"

#include <core/resource/user_resource.h>
#include <nx_ec/data/api_user_role_data.h>

struct QnResourceAccessSubjectPrivate
{
public:
    QnResourceAccessSubjectPrivate(const QnUserResourcePtr& user, const ec2::ApiUserRoleData& role):
        user(user),
        role(role)
    {
    }

    bool isValid() const
    {
        return user || !role.isNull();
    }

    QnUuid id() const
    {
        return user
            ? user->getId()
            : role.id;
    }

    QnUuid effectiveId() const
    {
        if (!isValid())
            return QnUuid();

        QnUuid key = role.id;
        if (user)
        {
            key = user->getId();
            if (user->userRole() == Qn::UserRole::CustomUserRole)
                key = user->userRoleId();
        }
        return key;
    }

    QnUserResourcePtr user;
    ec2::ApiUserRoleData role;
};



QnResourceAccessSubject::QnResourceAccessSubject(const QnUserResourcePtr& user):
    d_ptr(new QnResourceAccessSubjectPrivate(user, ec2::ApiUserRoleData()))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const ec2::ApiUserRoleData& role):
    d_ptr(new QnResourceAccessSubjectPrivate(QnUserResourcePtr(), role))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const QnResourceAccessSubject& other):
    d_ptr(new QnResourceAccessSubjectPrivate(other.user(), other.role()))
{
}

QnResourceAccessSubject::QnResourceAccessSubject():
    d_ptr(new QnResourceAccessSubjectPrivate(QnUserResourcePtr(), ec2::ApiUserRoleData()))
{
}

QnResourceAccessSubject::~QnResourceAccessSubject()
{
}

const QnUserResourcePtr& QnResourceAccessSubject::user() const
{
    return d_ptr->user;
}

const ec2::ApiUserRoleData& QnResourceAccessSubject::role() const
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

bool QnResourceAccessSubject::operator!=(const QnResourceAccessSubject& other) const
{
    return !(*this == other);
}

void QnResourceAccessSubject::operator=(const QnResourceAccessSubject& other)
{
    d_ptr->user = other.d_ptr->user;
    d_ptr->role = other.d_ptr->role;
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
