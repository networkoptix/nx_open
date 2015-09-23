
#include "user_resource.h"

#include <utils/crypt/linux_passwd_crypt.h>
#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/network/http/auth_tools.h>


static const int LDAP_PASSWORD_PROLONGATION_PERIOD_SEC = 5 * 60;
static const int MSEC_PER_SEC = 1000;

QnUserResource::QnUserResource():
    m_permissions(0),
    m_isAdmin(false),
	m_isLdap(false),
	m_isEnabled(true),
    m_passwordExpirationTimestamp(0)
{
    addFlags(Qn::user | Qn::remote);
}

QByteArray QnUserResource::getHash() const
{
    QnMutexLocker locker( &m_mutex );
    return m_hash;
}

void QnUserResource::setHash(const QByteArray& hash)
{
    {
        QnMutexLocker locker( &m_mutex );
        if (m_hash == hash)
            return;
        m_hash = hash;
    }
    emit hashChanged(::toSharedPointer(this));
}

QString QnUserResource::getPassword() const
{
    QnMutexLocker locker( &m_mutex );
    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    {
        QnMutexLocker locker( &m_mutex );
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

    QByteArray digest = nx_http::calcHa1(
        getName().toLower(),
        QnAppInfo::realm(),
        password );

    setRealm(QnAppInfo::realm());
    setHash(hash);
    setDigest(digest);
    setCryptSha512Hash( linuxCryptSha512( password.toUtf8(), generateSalt( LINUX_CRYPT_SALT_LENGTH ) ) );
}

bool QnUserResource::checkPassword(const QString &password) {
    QnMutexLocker locker( &m_mutex );
    
    if( !m_digest.isEmpty() )
    {
        return nx_http::calcHa1( m_name.toLower(), m_realm, password ) == m_digest;
    }
    else
    {
        //hash is obsolete. Cannot remove it to maintain update from version < 2.3
        //  hash becomes empty after changing user's realm
        QList<QByteArray> values = m_hash.split(L'$');
        if (values.size() != 3)
            return false;

        QByteArray salt = values[1];
        QCryptographicHash md5(QCryptographicHash::Md5);
        md5.addData(salt);
        md5.addData(password.toUtf8());
        return md5.result().toHex() == values[2];
    }
}

void QnUserResource::setDigest(const QByteArray& digest, bool isValidated)
{
    {
        QnMutexLocker locker( &m_mutex );
        if (m_digest == digest)
            return;
        if( m_isLdap )
        {
            m_passwordExpirationTimestamp = isValidated
                ? qnSyncTime->currentMSecsSinceEpoch() + LDAP_PASSWORD_PROLONGATION_PERIOD_SEC * MSEC_PER_SEC
                : 0;
        }
        m_digest = digest;
    }
    emit digestChanged(::toSharedPointer(this));
}

QByteArray QnUserResource::getDigest() const
{
    QnMutexLocker locker( &m_mutex );
    return m_digest;
}

void QnUserResource::setCryptSha512Hash( const QByteArray& cryptSha512Hash )
{
    {
        QnMutexLocker locker( &m_mutex );
        if( m_cryptSha512Hash == cryptSha512Hash )
            return;
        m_cryptSha512Hash = cryptSha512Hash;
    }
    emit cryptSha512HashChanged( ::toSharedPointer( this ) );
}

QByteArray QnUserResource::getCryptSha512Hash() const
{
    QnMutexLocker locker( &m_mutex );
    return m_cryptSha512Hash;
}

QString QnUserResource::getRealm() const
{
    QnMutexLocker locker( &m_mutex );
    return m_realm;
}

void QnUserResource::setRealm( const QString& realm )
{
    QnMutexLocker locker( &m_mutex );
    m_realm = realm;
}

quint64 QnUserResource::getPermissions() const
{
    QnMutexLocker locker( &m_mutex );
    return m_permissions;
}

void QnUserResource::setPermissions(quint64 permissions)
{
    {
        QnMutexLocker locker( &m_mutex );
        if (m_permissions == permissions)
            return;
        m_permissions = permissions;
    }
    emit permissionsChanged(::toSharedPointer(this));
}

bool QnUserResource::isAdmin() const
{
    QnMutexLocker locker( &m_mutex );
    return m_isAdmin;
}

void QnUserResource::setAdmin(bool isAdmin)
{
    {
        QnMutexLocker locker( &m_mutex );
        if (m_isAdmin == isAdmin)
            return;
        m_isAdmin = isAdmin;
    }
    emit adminChanged(::toSharedPointer(this));
}

bool QnUserResource::isLdap() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isLdap;
}

void QnUserResource::setLdap(bool isLdap)
{
    QnMutexLocker locker(&m_mutex);
    if (m_isLdap == isLdap)
        return;
    m_isLdap = isLdap;

    emit ldapChanged(::toSharedPointer(this));
}

bool QnUserResource::isEnabled() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isEnabled;
}

void QnUserResource::setEnabled(bool isEnabled)
{
    QnMutexLocker locker(&m_mutex);
    if (m_isEnabled == isEnabled)
        return;
    m_isEnabled = isEnabled;
    emit enabledChanged(::toSharedPointer(this));
}

QString QnUserResource::getEmail() const
{
    QnMutexLocker locker( &m_mutex );
    return m_email;
}

void QnUserResource::setEmail(const QString& email)
{
    {
        QnMutexLocker locker( &m_mutex );
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

		if (m_realm != localOther->m_realm) {
            m_realm = localOther->m_realm;
            modifiedFields << "realmChanged";
        }

        if (m_isEnabled != localOther->m_isEnabled) {
            m_isEnabled = localOther->m_isEnabled;
            modifiedFields << "enabledChanged";
        }

        if (m_isLdap != localOther->m_isLdap) {
            m_isLdap = localOther->m_isLdap;
            modifiedFields << "ldapChanged";
        }
    }
}

Qn::ResourceStatus QnUserResource::getStatus() const
{
    return Qn::Online;
}

qint64 QnUserResource::passwordExpirationTimestamp() const
{
    QnMutexLocker lk( &m_mutex );
    return m_passwordExpirationTimestamp;
}

bool QnUserResource::passwordExpired() const
{
    if( !isLdap() )
        return false;

    return passwordExpirationTimestamp() < qnSyncTime->currentMSecsSinceEpoch();
}

Qn::AuthResult QnUserResource::doPasswordProlongation()
{
    if( !isLdap() )
        return Qn::Auth_OK;

    QString name;
    QString digest;
    {
        QnMutexLocker lk( &m_mutex );
        name = m_name;
        digest = QLatin1String(m_digest);
    }

    auto errorCode = QnLdapManager::instance()->authenticateWithDigest( name, digest );
    if( errorCode != Qn::Auth_OK ) {
        if (!passwordExpired())
            return Qn::Auth_OK;
        else
            return errorCode;
    }

    QnMutexLocker lk( &m_mutex );
    if( m_name != name || QLatin1String(m_digest) != digest )  //user data has been updated somehow while performing ldap request
        return m_passwordExpirationTimestamp > qnSyncTime->currentMSecsSinceEpoch() ? Qn::Auth_OK : Qn::Auth_PasswordExpired;
    m_passwordExpirationTimestamp =
        qnSyncTime->currentMSecsSinceEpoch() +
        LDAP_PASSWORD_PROLONGATION_PERIOD_SEC * MSEC_PER_SEC;
    return Qn::Auth_OK;
}

Qn::AuthResult QnUserResource::checkDigestValidity( const QByteArray& digest )
{
    if( !isLdap() )
        return Qn::Auth_OK;

    return QnLdapManager::instance()->authenticateWithDigest( getName(), QLatin1String(digest) );
}
