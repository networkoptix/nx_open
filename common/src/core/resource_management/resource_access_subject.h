#pragma once

#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_fwd.h>

struct QnResourceAccessSubjectPrivate;

/**
* This class represents subject of resource access - user or user role.
*/
class QnResourceAccessSubject
{
public:
    QnResourceAccessSubject(const QnUserResourcePtr& user);
    QnResourceAccessSubject(const ec2::ApiUserGroupData& role);
    QnResourceAccessSubject(const QnResourceAccessSubject& other);
    virtual ~QnResourceAccessSubject();

    const QnUserResourcePtr& user() const;
    const ec2::ApiUserGroupData& role() const;

    bool isValid() const;

    //TODO: #GDM logical dependency hack. Change ResourceAccessManager interface to work with subj.
    /** Key value in the shared resources map. */
    QnUuid sharedResourcesKey() const;

private:
    QScopedPointer<QnResourceAccessSubjectPrivate> d_ptr;
};
