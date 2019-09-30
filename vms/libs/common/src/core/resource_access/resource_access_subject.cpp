#include "resource_access_subject.h"

QnResourceAccessSubject::QnResourceAccessSubject(const QnUserResourcePtr& user):
    m_user(user)
{
    if (user)
        m_id = user->getId();
}

QnResourceAccessSubject::QnResourceAccessSubject(const nx::vms::api::UserRoleData& role):
    m_rolePermissions(role.permissions),
    m_id(role.id)
{
}

QnResourceAccessSubject::QnResourceAccessSubject(const QnResourceAccessSubject& other):
    m_user(other.user()), m_rolePermissions(other.rolePermissions()), m_id(other.id())
{
}

QnResourceAccessSubject::QnResourceAccessSubject()
{
}

QString QnResourceAccessSubject::name() const
{
    return m_user ? m_user->getName() :  QString::number(m_rolePermissions,16);
}

void QnResourceAccessSubject::operator=(const QnResourceAccessSubject& other)
{
    m_user = other.m_user;
    m_rolePermissions = other.m_rolePermissions;
    m_id = other.m_id;
}

bool QnResourceAccessSubject::operator!=(const QnResourceAccessSubject& other) const
{
    return !(*this == other);
}

bool QnResourceAccessSubject::operator==(const QnResourceAccessSubject& other) const
{
    return m_id == other.m_id;
}

QDebug operator<<(QDebug dbg, const QnResourceAccessSubject& subject)
{
    dbg.nospace() << "QnResourceAccessSubject(" << subject.name() << ")";
    return dbg.space();
}
