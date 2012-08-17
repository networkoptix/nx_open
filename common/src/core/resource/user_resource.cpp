#include "user_resource.h"
#include <utils/common/warnings.h>

QnUserResource::QnUserResource()
    : base_type(),
      m_rights(0)
{
    setStatus(Online, true);
    addFlags(QnResource::user | QnResource::remote);
}

QString QnUserResource::getUniqueId() const
{
    return getGuid();
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

quint64 QnUserResource::getRights() const
{
    return m_rights;
}

void QnUserResource::setRights(quint64 rights)
{
    m_rights = rights;
}

void QnUserResource::updateInner(QnResourcePtr other) {
    base_type::updateInner(other);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if(localOther) {
        setPassword(localOther->getPassword());

        setRights(localOther->getRights());
    }
}
