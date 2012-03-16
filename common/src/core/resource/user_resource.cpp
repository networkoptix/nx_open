#include "user_resource.h"
#include <utils/common/warnings.h>

QnUserResource::QnUserResource()
    : base_type(),
      m_isAdmin(false)
{
    setStatus(Online, true);
    addFlags(QnResource::user | QnResource::remote);
}

QString QnUserResource::getUniqueId() const
{
    return getName();
}

QString QnUserResource::getPassword() const
{
    QMutexLocker locker(&m_mutex);

    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    QMutexLocker locker(&m_mutex);

    m_password = password;
}

bool QnUserResource::isAdmin() const
{
    return m_isAdmin;
}

void QnUserResource::setAdmin(bool admin)
{
    m_isAdmin = admin;
}

void QnUserResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if(localOther) {
        setPassword(localOther->getPassword());

        setAdmin(localOther->isAdmin());
    }
}
