#include "user_resource.h"

#include <api/model/password_data.h>

#include <nx/network/http/auth_tools.h>

#include <utils/common/synctime.h>
#include <core/resource_management/resource_properties.h>

namespace {
    static const int LDAP_PASSWORD_PROLONGATION_PERIOD_SEC = 5 * 60;
    static const int MSEC_PER_SEC = 1000;
}


const QnUuid QnUserResource::kAdminGuid("99cbc715-539b-4bfe-856f-799b45b69b1e");

QnUserResource::QnUserResource(QnUserType userType):
    m_userType(userType),
    m_permissions(0),
    m_userGroup(),
    m_isOwner(false),
	m_isEnabled(true),
    m_fullName(),
    m_passwordExpirationTimestamp(0)
{
    addFlags(Qn::user | Qn::remote);
    setTypeId(qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kUserTypeId));
}

QnUserResource::QnUserResource(const QnUserResource& right):
    QnResource(right),
    m_userType(right.m_userType),
    m_password(right.m_password),
    m_hash(right.m_hash),
    m_digest(right.m_digest),
    m_cryptSha512Hash(right.m_cryptSha512Hash),
    m_realm(right.m_realm),
    m_permissions(right.m_permissions),
    m_userGroup(right.m_userGroup),
    m_isOwner(right.m_isOwner),
    m_isEnabled(right.m_isEnabled),
    m_email(right.m_email),
    m_fullName(right.m_fullName),
    m_passwordExpirationTimestamp(right.m_passwordExpirationTimestamp)
{
}

Qn::UserRole QnUserResource::role() const
{
    if (!resourcePool())
        return Qn::UserRole::CustomPermissions;

    if (isOwner())
        return Qn::UserRole::Owner;

    QnUuid groupId = userGroup();
    if (!groupId.isNull())
        return Qn::UserRole::CustomUserGroup;

    auto permissions = getRawPermissions();

    if (permissions.testFlag(Qn::GlobalAdminPermission))
        return Qn::UserRole::Administrator;

    switch (permissions)
    {
        case Qn::GlobalAdvancedViewerPermissionSet:
            return Qn::UserRole::AdvancedViewer;

        case Qn::GlobalViewerPermissionSet:
            return Qn::UserRole::Viewer;

        case Qn::GlobalLiveViewerPermissionSet:
            return Qn::UserRole::LiveViewer;

        default:
            break;
    };

    return Qn::UserRole::CustomPermissions;
}

QByteArray QnUserResource::getHash() const
{
    QnMutexLocker locker(&m_mutex);
    return m_hash;
}

void QnUserResource::setHash(const QByteArray& hash)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_hash == hash)
            return;
        m_hash = hash;
    }
    emit hashChanged(::toSharedPointer(this));
}

QString QnUserResource::getPassword() const
{
    QnMutexLocker locker(&m_mutex);
    return m_password;
}

void QnUserResource::setPassword(const QString& password)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_password == password)
            return;
        m_password = password;
    }
    emit passwordChanged(::toSharedPointer(this));
}

void QnUserResource::generateHash()
{
    QString password = getPassword();
    NX_ASSERT(!password.isEmpty(), Q_FUNC_INFO, "Invalid password");
    if (password.isEmpty())
        return;

    auto hashes = PasswordData::calculateHashes(getName(), password);

    setRealm(hashes.realm);
    setHash(hashes.passwordHash);
    setDigest(hashes.passwordDigest);
    setCryptSha512Hash(hashes.cryptSha512Hash);
}

bool QnUserResource::checkLocalUserPassword(const QString &password)
{
    QnMutexLocker locker(&m_mutex);

    if (!m_digest.isEmpty())
        return nx_http::calcHa1(m_name.toLower(), m_realm, password) == m_digest;

    //hash is obsolete. Cannot remove it to maintain update from version < 2.3
    //hash becomes empty after changing user's realm
    QList<QByteArray> values = m_hash.split(L'$');
    if (values.size() != 3)
        return false;

    QByteArray salt = values[1];
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(salt);
    md5.addData(password.toUtf8());
    return md5.result().toHex() == values[2];
}

void QnUserResource::setDigest(const QByteArray& digest, bool isValidated)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_digest == digest)
            return;
        if (m_userType == QnUserType::Ldap)
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
    QnMutexLocker locker(&m_mutex);
    return m_digest;
}

void QnUserResource::setCryptSha512Hash(const QByteArray& cryptSha512Hash)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_cryptSha512Hash == cryptSha512Hash)
            return;
        m_cryptSha512Hash = cryptSha512Hash;
    }
    emit cryptSha512HashChanged(::toSharedPointer(this));
}

QByteArray QnUserResource::getCryptSha512Hash() const
{
    QnMutexLocker locker(&m_mutex);
    return m_cryptSha512Hash;
}

QString QnUserResource::getRealm() const
{
    QnMutexLocker locker(&m_mutex);
    return m_realm;
}

void QnUserResource::setRealm(const QString& realm)
{
    QnMutexLocker locker(&m_mutex);
    m_realm = realm;
}

Qn::GlobalPermissions QnUserResource::getRawPermissions() const
{
    QnMutexLocker locker(&m_mutex);
    return m_permissions;
}

void QnUserResource::setRawPermissions(Qn::GlobalPermissions permissions)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_permissions == permissions)
            return;
        m_permissions = permissions;
    }
    emit permissionsChanged(::toSharedPointer(this));
}

bool QnUserResource::isOwner() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isOwner;
}

void QnUserResource::setOwner(bool isOwner)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_isOwner == isOwner)
            return;
        m_isOwner = isOwner;
    }
    emit permissionsChanged(::toSharedPointer(this));
}

QnUuid QnUserResource::userGroup() const
{
    QnMutexLocker locker(&m_mutex);
    return m_userGroup;
}

void QnUserResource::setUserGroup(const QnUuid& group)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_userGroup == group)
            return;
        m_userGroup = group;
    }
    emit userGroupChanged(::toSharedPointer(this));
}

bool QnUserResource::isEnabled() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isEnabled;
}

void QnUserResource::setEnabled(bool isEnabled)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_isEnabled == isEnabled)
            return;
        m_isEnabled = isEnabled;
    }
    emit enabledChanged(::toSharedPointer(this));
}

QnUserType QnUserResource::userType() const
{
    QnMutexLocker locker(&m_mutex);
    return m_userType;
}

QString QnUserResource::getEmail() const
{
    QnMutexLocker locker(&m_mutex);
    return m_email;
}

void QnUserResource::setEmail(const QString& email)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (email.trimmed() == m_email)
            return;
        m_email = email.trimmed();
    }
    emit emailChanged(::toSharedPointer(this));
}

QString QnUserResource::fullName() const
{
    QString result = propertyDictionary->value(getId(), Qn::USER_FULL_NAME);
    QnMutexLocker locker(&m_mutex);
    return result.isNull() ? m_fullName : result;
}

void QnUserResource::setFullName(const QString& value)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (value.trimmed() == m_fullName)
            return;
        m_fullName = value.trimmed();
    }
    emit fullNameChanged(::toSharedPointer(this));
}

void QnUserResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    QnUserResourcePtr localOther = other.dynamicCast<QnUserResource>();
    if (localOther)
    {
        NX_ASSERT(m_userType == localOther->m_userType, Q_FUNC_INFO, "User type was designed to be read-only");

        if (m_password != localOther->m_password)
        {
            m_password = localOther->m_password;
            notifiers << [r = toSharedPointer(this)]{emit r->passwordChanged(r);};
        }

        if (m_hash != localOther->m_hash)
        {
            m_hash = localOther->m_hash;
            notifiers << [r = toSharedPointer(this)]{ emit r->hashChanged(r); };
        }

        if (m_digest != localOther->m_digest)
        {
            m_digest = localOther->m_digest;
            notifiers << [r = toSharedPointer(this)]{ emit r->digestChanged(r); };
        }

        if (m_cryptSha512Hash != localOther->m_cryptSha512Hash)
        {
            m_cryptSha512Hash = localOther->m_cryptSha512Hash;
            notifiers << [r = toSharedPointer(this)]{ emit r->cryptSha512HashChanged(r); };
        }

        if (m_permissions != localOther->m_permissions)
        {
            m_permissions = localOther->m_permissions;
            notifiers << [r = toSharedPointer(this)]{ emit r->permissionsChanged(r); };
        }

        if (m_userGroup != localOther->m_userGroup)
        {
            m_userGroup = localOther->m_userGroup;
            notifiers << [r = toSharedPointer(this)]{ emit r->userGroupChanged(r); };
        }

        if (m_isOwner != localOther->m_isOwner)
        {
            NX_ASSERT(false, "'Owner' field should not be changed.");
            m_isOwner = localOther->m_isOwner;
            notifiers << [r = toSharedPointer(this)]{ emit r->permissionsChanged(r); };
        }

        if (m_email != localOther->m_email)
        {
            m_email = localOther->m_email;
            notifiers << [r = toSharedPointer(this)]{ emit r->emailChanged(r); };
        }

        if (m_fullName != localOther->m_fullName)
        {
            m_fullName = localOther->m_fullName;
            notifiers << [r = toSharedPointer(this)]{ emit r->fullNameChanged(r); };
        }

		if (m_realm != localOther->m_realm)
        {
            m_realm = localOther->m_realm;
            notifiers << [r = toSharedPointer(this)]{ emit r->realmChanged(r); };
        }

        if (m_isEnabled != localOther->m_isEnabled)
        {
            m_isEnabled = localOther->m_isEnabled;
            notifiers << [r = toSharedPointer(this)]{ emit r->enabledChanged(r); };
        }
    }
}

Qn::ResourceStatus QnUserResource::getStatus() const
{
    return Qn::Online;
}

qint64 QnUserResource::passwordExpirationTimestamp() const
{
    QnMutexLocker lk(&m_mutex);
    return m_passwordExpirationTimestamp;
}

void QnUserResource::prolongatePassword(qint64 value)
{
    QnMutexLocker lk(&m_mutex);
    if (value == PasswordProlongationAuto)
        m_passwordExpirationTimestamp = qnSyncTime->currentMSecsSinceEpoch() + LDAP_PASSWORD_PROLONGATION_PERIOD_SEC * MSEC_PER_SEC;
    else
        m_passwordExpirationTimestamp = value;
}

bool QnUserResource::passwordExpired() const
{
    if (!isLdap())
        return false;

    return passwordExpirationTimestamp() < qnSyncTime->currentMSecsSinceEpoch();
}

void QnUserResource::fillId()
{
    NX_ASSERT(!(isCloud() && getEmail().isEmpty()));
    QnUuid id = isCloud() ? guidFromArbitraryData(getEmail()) : QnUuid::createUuid();
    setId(id);
}
