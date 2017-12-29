#include "user_resource.h"

#include <api/model/password_data.h>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/raii_guard.h>

#include <utils/common/synctime.h>
#include <core/resource_management/resource_properties.h>
#include <utils/crypt/symmetrical.h>
#include <common/common_module.h>
#include "resource.h"

namespace
{
static const int LDAP_PASSWORD_PROLONGATION_PERIOD_SEC = 5 * 60;
static const int MSEC_PER_SEC = 1000;

QnRaiiGuardPtr createSignalGuard(
    QnUserResource* resource,
    void (QnUserResource::*targetSignal)(const QnResourcePtr&))
{
    return QnRaiiGuard::createDestructible(
        [resource, targetSignal]()
        {
            (resource->*targetSignal)(::toSharedPointer(resource));
        });
}

}

const QnUuid QnUserResource::kAdminGuid("99cbc715-539b-4bfe-856f-799b45b69b1e");

QnUserResource::QnUserResource(QnUserType userType):
    m_userType(userType),
    m_realm(nx::network::AppInfo::realm()),
    m_permissions(0),
    m_userRoleId(),
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
    m_userRoleId(right.m_userRoleId),
    m_isOwner(right.m_isOwner),
    m_isEnabled(right.m_isEnabled),
    m_email(right.m_email),
    m_fullName(right.m_fullName),
    m_passwordExpirationTimestamp(right.m_passwordExpirationTimestamp)
{
}

template<typename T>
bool QnUserResource::setMemberChecked(
    T QnUserResource::* member,
    T value,
    MiddlestepFunction middlestep)
{
    QnMutexLocker locker(&m_mutex);
    if (this->*member == value)
        return false;

    if (middlestep)
        middlestep();

    this->*member = value;
    return true;
}

Qn::UserRole QnUserResource::userRole() const
{
    if (isOwner())
        return Qn::UserRole::Owner;

    QnUuid id = userRoleId();
    if (!id.isNull())
        return Qn::UserRole::CustomUserRole;

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
    if (setMemberChecked(&QnUserResource::m_hash, hash))
        emit hashChanged(::toSharedPointer(this));
}

QString QnUserResource::getPassword() const
{
    QnMutexLocker locker(&m_mutex);
    return m_password;
}

void QnUserResource::setPasswordAndGenerateHash(const QString& password)
{
    const bool emitChangedSignal = setMemberChecked(&QnUserResource::m_password, password);
    updateHash();
    if (emitChangedSignal)
        emit passwordChanged(::toSharedPointer(this));
}

void QnUserResource::resetPassword()
{
    setMemberChecked(&QnUserResource::m_password, QString());
}

QString QnUserResource::decodeLDAPPassword() const
{
    QList<QByteArray> parts = getHash().split('$');
    if (parts.size() != 3 || parts[0] != "LDAP")
        return QString();

    QByteArray salt = QByteArray::fromHex(parts[1]);
    QByteArray encodedPassword = QByteArray::fromHex(parts[2]);
    return QString::fromUtf8(nx::utils::encodeSimple(encodedPassword, salt));
}

void QnUserResource::updateHash()
{
    QString password = getPassword();
    if (password.isEmpty())
        return;

    const auto hashes = PasswordData::calculateHashes(getName(), password, isLdap());

    using SignalGuardList = QList<QnRaiiGuardPtr>;

    SignalGuardList guards;

    const auto resource = ::toSharedPointer(this);
    if (setMemberChecked(&QnUserResource::m_realm, hashes.realm))
        guards.append(createSignalGuard(this, &QnUserResource::realmChanged));

    if (setMemberChecked(&QnUserResource::m_hash, hashes.passwordHash))
        guards.append(createSignalGuard(this, &QnUserResource::hashChanged));

    if (setMemberChecked(&QnUserResource::m_digest, hashes.passwordDigest))
        guards.append(createSignalGuard(this, &QnUserResource::digestChanged));

    if (setMemberChecked(&QnUserResource::m_cryptSha512Hash, hashes.cryptSha512Hash))
        guards.append(createSignalGuard(this, &QnUserResource::cryptSha512HashChanged));
}

bool QnUserResource::checkLocalUserPassword(const QString &password)
{
    QnMutexLocker locker(&m_mutex);

    if (!m_digest.isEmpty())
        return nx::network::http::calcHa1(m_name.toLower(), m_realm, password) == m_digest;

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

void QnUserResource::setDigest(const QByteArray& digest)
{
    const auto middlestep =
        [this]()
        {
            if (m_userType == QnUserType::Ldap)
                m_passwordExpirationTimestamp = 0;
        };

    if (setMemberChecked(&QnUserResource::m_digest, digest, middlestep))
        emit digestChanged(::toSharedPointer(this));
}

QByteArray QnUserResource::getDigest() const
{
    QnMutexLocker locker(&m_mutex);
    return m_digest;
}

void QnUserResource::setCryptSha512Hash(const QByteArray& cryptSha512Hash)
{
    if (setMemberChecked(&QnUserResource::m_cryptSha512Hash, cryptSha512Hash))
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
    if (setMemberChecked(&QnUserResource::m_realm, realm))
        emit realmChanged(::toSharedPointer(this));
}

Qn::GlobalPermissions QnUserResource::getRawPermissions() const
{
    QnMutexLocker locker(&m_mutex);
    return m_permissions;
}

void QnUserResource::setRawPermissions(Qn::GlobalPermissions permissions)
{
    if (setMemberChecked(&QnUserResource::m_permissions, permissions))
        emit permissionsChanged(::toSharedPointer(this));
}

bool QnUserResource::isBuiltInAdmin() const
{
    return getId() == kAdminGuid;
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

QnUuid QnUserResource::userRoleId() const
{
    QnMutexLocker locker(&m_mutex);
    return m_userRoleId;
}

void QnUserResource::setUserRoleId(const QnUuid& userRoleId)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_userRoleId == userRoleId)
            return;
        m_userRoleId = userRoleId;
    }
    emit userRoleChanged(::toSharedPointer(this));
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
    QString result;
    if (commonModule())
        result = commonModule()->propertyDictionary()->value(getId(), Qn::USER_FULL_NAME);
    QnMutexLocker locker(&m_mutex);
    return result.isNull() ? m_fullName : result;
}

ec2::ApiResourceParamWithRefDataList QnUserResource::params() const
{
    ec2::ApiResourceParamWithRefDataList result;
    QString value;
    if (commonModule())
        value = commonModule()->propertyDictionary()->value(getId(), Qn::USER_FULL_NAME);
    if (value.isEmpty() && !fullName().isEmpty() && isCloud())
        value = fullName(); //< move fullName to property dictionary to sync data with cloud correctly
    if (!value.isEmpty())
    {
        ec2::ApiResourceParamWithRefData param(getId(), Qn::USER_FULL_NAME, value);
        result.push_back(param);
    }
    return result;
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

        if (m_userRoleId != localOther->m_userRoleId)
        {
            m_userRoleId = localOther->m_userRoleId;
            notifiers << [r = toSharedPointer(this)]{ emit r->userRoleChanged(r); };
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
    // ATTENTION: This logic is similar to ApiUserData::fillId().
    NX_ASSERT(!(isCloud() && getEmail().isEmpty()));
    QnUuid id = isCloud() ? guidFromArbitraryData(getEmail()) : QnUuid::createUuid();
    setId(id);
}
