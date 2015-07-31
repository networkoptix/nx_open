
#include "user_resource.h"
#include "network/authenticate_helper.h"
#include <utils/crypt/linux_passwd_crypt.h>


QnUserResource::QnUserResource():
    m_permissions(0),
    m_isAdmin(false),
	m_isLdap(false),
	m_isEnabled(true)
{
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

    QByteArray digest = QnAuthHelper::createUserPasswordDigest(getName().toLower(), password);

    setHash(hash);
    setDigest(digest);
    setCryptSha512Hash( linuxCryptSha512( password.toUtf8(), generateSalt( LINUX_CRYPT_SALT_LENGTH ) ) );
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

void QnUserResource::setCryptSha512Hash( const QByteArray& cryptSha512Hash )
{
    {
        QMutexLocker locker( &m_mutex );
        if( m_cryptSha512Hash == cryptSha512Hash )
            return;
        m_cryptSha512Hash = cryptSha512Hash;
    }
    emit cryptSha512HashChanged( ::toSharedPointer( this ) );
}

QByteArray QnUserResource::getCryptSha512Hash() const
{
    QMutexLocker locker( &m_mutex );
    return m_cryptSha512Hash;
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

bool QnUserResource::isLdap() const
{
    QMutexLocker locker(&m_mutex);
    return m_isLdap;
}

void QnUserResource::setLdap(bool isLdap)
{
    QMutexLocker locker(&m_mutex);
    if (m_isLdap == isLdap)
        return;
    m_isLdap = isLdap;
}

bool QnUserResource::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_isEnabled;
}

void QnUserResource::setEnabled(bool isEnabled)
{
    QMutexLocker locker(&m_mutex);
    if (m_isEnabled == isEnabled)
        return;
    m_isEnabled = isEnabled;
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

        if (m_cryptSha512Hash != localOther->m_cryptSha512Hash ) {
            m_cryptSha512Hash = localOther->m_cryptSha512Hash;
            modifiedFields << "cryptSha512HashChanged";
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

Qn::ResourceStatus QnUserResource::getStatus() const
{
    return Qn::Online;
}
