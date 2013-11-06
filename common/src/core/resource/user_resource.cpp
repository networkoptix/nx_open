#include "user_resource.h"

QnUserResource::QnUserResource():
    m_permissions(0),
    m_isAdmin(false)
{
    setStatus(Online, true);
    addFlags(QnResource::user | QnResource::remote);
}

QString QnUserResource::getUniqueId() const
{
    return getGuid();
}

QString QnUserResource::getHash() const
{
    QMutexLocker locker(&m_mutex);
    return m_hash;
}

void QnUserResource::setHash(const QString& hash)
{
    QMutexLocker locker(&m_mutex);
    m_hash = hash;
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

void QnUserResource::setDigest(const QString& digest)
{
    QMutexLocker locker(&m_mutex);
    m_digest = digest;
}

QString QnUserResource::getDigest() const
{
    QMutexLocker locker(&m_mutex);
    return m_digest;
}
quint64 QnUserResource::getPermissions() const
{
    QMutexLocker locker(&m_mutex);
    return m_permissions;
}

void QnUserResource::setPermissions(quint64 rights)
{
    QMutexLocker locker(&m_mutex);
    m_permissions = rights;
}

bool QnUserResource::isAdmin() const
{
    QMutexLocker locker(&m_mutex);
    return m_isAdmin;
}

void QnUserResource::setAdmin(bool isAdmin)
{
    QMutexLocker locker(&m_mutex);
    m_isAdmin = isAdmin;
}

QString QnUserResource::getEmail() const
{
    QMutexLocker locker(&m_mutex);
    return m_email;
}

void QnUserResource::setEmail(const QString& email)
{
    {
        QMutexLocker locker(&m_mutex);
        if (email.trimmed() == m_email)
            return;
        m_email = email.trimmed();
    }
    emit emailChanged(::toSharedPointer(this));
}

void QnUserResource::updateInner(QnResourcePtr other)
{
    base_type::updateInner(other);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if(localOther) {
        setPassword(localOther->getPassword());
        setHash(localOther->getHash());
        setDigest(localOther->getDigest());
        setPermissions(localOther->getPermissions());
        setAdmin(localOther->isAdmin());
        setEmail(localOther->getEmail());
    }
}
