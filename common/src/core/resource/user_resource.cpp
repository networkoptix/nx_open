#include "user_resource.h"
#include "network/authenticate_helper.h"

QnUserResource::QnUserResource():
    m_permissions(0),
    m_isAdmin(false)
{
    setStatus(Qn::Online, true);
    addFlags(Qn::user | Qn::remote);
}

QByteArray QnUserResource::getHash() const
{
    QMutexLocker locker(&m_mutex);
    return m_hash;
}

void QnUserResource::setHash(const QByteArray& hash)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_hash == hash)
            return;
        m_hash = hash;
    }
    emit hashChanged(::toSharedPointer(this));
}

QString QnUserResource::getPassword() const
{
    QMutexLocker locker(&m_mutex);
    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_password == password)
            return;
        m_password = password;
    }
    emit passwordChanged(::toSharedPointer(this));
}

void QnUserResource::generateHash() {
    QString password = getPassword();

    QByteArray salt = QByteArray::number(rand(), 16);
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(salt);
    md5.addData(password.toUtf8());
    QByteArray hash = "md5$";
    hash.append(salt);
    hash.append("$");
    hash.append(md5.result().toHex());

    QByteArray digest = QnAuthHelper::createUserPasswordDigest(getName(), password);

    setHash(hash);
    setDigest(digest);
}

bool QnUserResource::checkPassword(const QString &password) {
    QList<QByteArray> values =  getHash().split(L'$');
    if (values.size() != 3)
        return false;

    QByteArray salt = values[1];
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(salt);
    md5.addData(password.toUtf8());
    return md5.result().toHex() == values[2];
}

void QnUserResource::setDigest(const QByteArray& digest)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_digest == digest)
            return;
        m_digest = digest;
    }
    emit digestChanged(::toSharedPointer(this));
}

QByteArray QnUserResource::getDigest() const
{
    QMutexLocker locker(&m_mutex);
    return m_digest;
}
quint64 QnUserResource::getPermissions() const
{
    QMutexLocker locker(&m_mutex);
    return m_permissions;
}

void QnUserResource::setPermissions(quint64 permissions)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_permissions == permissions)
            return;
        m_permissions = permissions;
    }
    emit permissionsChanged(::toSharedPointer(this));
}

bool QnUserResource::isAdmin() const
{
    QMutexLocker locker(&m_mutex);
    return m_isAdmin;
}

void QnUserResource::setAdmin(bool isAdmin)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_isAdmin == isAdmin)
            return;
        m_isAdmin = isAdmin;
    }
    emit adminChanged(::toSharedPointer(this));
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

void QnUserResource::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields)
{
    base_type::updateInner(other, modifiedFields);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if(localOther) {
        if (m_password != localOther->m_password) {
            m_password = localOther->m_password;
            modifiedFields << "passwordChanged";
        }

        if (m_hash != localOther->m_hash) {
            m_hash = localOther->m_hash;
            modifiedFields << "hashChanged";
        }

        if (m_digest != localOther->m_digest) {
            m_digest = localOther->m_digest;
            modifiedFields << "digestChanged";
        }

        if (m_permissions != localOther->m_permissions) {
            m_permissions = localOther->m_permissions;
            modifiedFields << "permissionsChanged";
        }

        if (m_isAdmin != localOther->m_isAdmin) {
            m_isAdmin = localOther->m_isAdmin;
            modifiedFields << "adminChanged";
        }

        if (m_email != localOther->m_email) {
            m_email = localOther->m_email;
            modifiedFields << "emailChanged";
        }
    }
}
