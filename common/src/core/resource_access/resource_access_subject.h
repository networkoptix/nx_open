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
    QnResourceAccessSubject();
    QnResourceAccessSubject(const QnUserResourcePtr& user);
    QnResourceAccessSubject(const nx::vms::api::UserRoleData& role);
    QnResourceAccessSubject(const QnResourceAccessSubject& other);
    virtual ~QnResourceAccessSubject();

    const QnUserResourcePtr& user() const;
    const nx::vms::api::UserRoleData& role() const;

    bool isValid() const;
    bool isUser() const;
    bool isRole() const;

    QnUuid id() const;

    /** Key value in the shared resources map. */
    QnUuid effectiveId() const;

    QString name() const;

    void operator=(const QnResourceAccessSubject& other);
    bool operator==(const QnResourceAccessSubject& other) const;
    bool operator!=(const QnResourceAccessSubject& other) const;
private:
    QScopedPointer<QnResourceAccessSubjectPrivate> d_ptr;
};

QDebug operator<<(QDebug dbg, const QnResourceAccessSubject& subject);

Q_DECLARE_METATYPE(QnResourceAccessSubject)
