#include "resource_access_subject.h"

#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_user_group_data.h>

struct QnResourceAccessSubjectPrivate
{
public:
    QnResourceAccessSubjectPrivate(const QnUserResourcePtr& user, const ec2::ApiUserGroupData& role):
        user(user),
        role(role)
    {
    }

    bool isValid() const
    {
        return user || !role.isNull();
    }

    QnUserResourcePtr user;
    ec2::ApiUserGroupData role;
};



QnResourceAccessSubject::QnResourceAccessSubject(const QnUserResourcePtr& user):
    d_ptr(new QnResourceAccessSubjectPrivate(user, ec2::ApiUserGroupData()))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const ec2::ApiUserGroupData& role):
    d_ptr(new QnResourceAccessSubjectPrivate(QnUserResourcePtr(), role))
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const QnResourceAccessSubject& other):
    d_ptr(new QnResourceAccessSubjectPrivate(other.user(), other.role()))
{
}

QnResourceAccessSubject::~QnResourceAccessSubject()
{
}

const QnUserResourcePtr& QnResourceAccessSubject::user() const
{
    return d_ptr->user;
}

const ec2::ApiUserGroupData& QnResourceAccessSubject::role() const
{
    return d_ptr->role;
}

bool QnResourceAccessSubject::isValid() const
{
    return d_ptr->isValid();
}
