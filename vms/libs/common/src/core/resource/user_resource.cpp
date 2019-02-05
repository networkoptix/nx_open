#include "user_resource.h"

#include <api/model/password_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/synctime.h>
#include <utils/crypt/symmetrical.h>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>
#include <nx/network/aio/timer.h>

namespace
{
std::chrono::minutes kDefaultLdapPasswordExperationPeriod(5);
static const int MSEC_PER_SEC = 1000;

nx::utils::SharedGuardPtr createSignalGuard(
    QnUserResource* resource,
    void (QnUserResource::*targetSignal)(const QnResourcePtr&))
{
    return nx::utils::makeSharedGuard(
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
    m_ldapPasswordExperationPeriod(kDefaultLdapPasswordExperationPeriod)
{
    addFlags(Qn::user | Qn::remote);
    setTypeId(nx::vms::api::UserData::kResourceTypeId);
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
    m_ldapPasswordExperationPeriod(right.m_ldapPasswordExperationPeriod)
{
}

QnUserResource::~QnUserResource()
{
    if (m_ldapPasswordTimer)
        m_ldapPasswordTimer->pleaseStopSync();
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
        return Qn::UserRole::owner;

    QnUuid id = userRoleId();
    if (!id.isNull())
        return Qn::UserRole::customUserRole;

    auto permissions = getRawPermissions();

    if (permissions.testFlag(GlobalPermission::admin))
        return Qn::UserRole::administrator;

    switch (permissions)
    {
        case (int) GlobalPermission::advancedViewerPermissions:
            return Qn::UserRole::advancedViewer;

        case (int) GlobalPermission::viewerPermissions:
            return Qn::UserRole::viewer;

        case (int) GlobalPermission::liveViewerPermissions:
            return Qn::UserRole::liveViewer;

        default:
            break;
    };

    return Qn::UserRole::customPermissions;
}

QByteArray QnUserResource::getHash() const
{
    QnMutexLocker locker(&m_mutex);
    return m_hash;
}

void QnUserResource::setHash(const QByteArray& hash)
{
    if (setMemberChecked(&QnUserResource::m_hash, hash))
        emit hashesChanged(::toSharedPointer(this));
}

QString QnUserResource::getPassword() const
{
    QnMutexLocker locker(&m_mutex);
    return m_password;
}

void QnUserResource::setPasswordAndGenerateHash(const QString& password)
{
    const bool emitChangedSignal = setMemberChecked(&QnUserResource::m_password, password);
    if (updateHash() && emitChangedSignal)
        emit hashesChanged(::toSharedPointer(this));
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

bool QnUserResource::updateHash()
{
    QString password = getPassword();
    if (password.isEmpty())
        return false;

    const auto hashes = PasswordData::calculateHashes(getName(), password, isLdap());

    using SignalGuardList = QList<nx::utils::SharedGuardPtr>;

    const auto resource = ::toSharedPointer(this);
    bool hashesChanged = false;
    if (setMemberChecked(&QnUserResource::m_realm, hashes.realm))
        hashesChanged = true;

    if (setMemberChecked(&QnUserResource::m_hash, hashes.passwordHash))
        hashesChanged = true;

    if (setMemberChecked(&QnUserResource::m_digest, hashes.passwordDigest))
        hashesChanged = true;

    if (setMemberChecked(&QnUserResource::m_cryptSha512Hash, hashes.cryptSha512Hash))
        hashesChanged = true;

    return hashesChanged;
}

bool QnUserResource::checkLocalUserPassword(const QString &password)
{
    QnMutexLocker locker(&m_mutex);

    if (!m_digest.isEmpty())
    {
        const auto isDigestCorrect = nx::network::http::calcHa1(m_name.toLower(), m_realm, password)
            == m_digest;

        NX_VERBOSE(this, lm("Digest is %1").args(isDigestCorrect ? "correct" : "incorrect"));
        return isDigestCorrect;
    }

    //hash is obsolete. Cannot remove it to maintain update from version < 2.3
    //hash becomes empty after changing user's realm
    QList<QByteArray> values = m_hash.split(L'$');
    if (values.size() != 3)
    {
        NX_VERBOSE(this, lm("Unable to parse hash: %1").arg(m_hash));
        return false;
    }

    QByteArray salt = values[1];
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(salt);
    md5.addData(password.toUtf8());

    const auto isHashCorrect = md5.result().toHex() == values[2];
    NX_VERBOSE(this, lm("Hash is %1").arg(isHashCorrect ? "correct" : "incorrect"));
    return isHashCorrect;
}

void QnUserResource::setDigest(const QByteArray& digest)
{
    const auto middlestep =
        [this]()
        {
            if (m_userType == QnUserType::Ldap)
            {
                m_ldapPasswordValid = false;
                QnMutexLocker lk(&m_mutex);
                if (m_ldapPasswordTimer)
                    m_ldapPasswordTimer->pleaseStopSync();
            }
        };

    if (setMemberChecked(&QnUserResource::m_digest, digest, middlestep))
        emit hashesChanged(::toSharedPointer(this));
}

QByteArray QnUserResource::getDigest() const
{
    QnMutexLocker locker(&m_mutex);
    return m_digest;
}

void QnUserResource::setCryptSha512Hash(const QByteArray& cryptSha512Hash)
{
    if (setMemberChecked(&QnUserResource::m_cryptSha512Hash, cryptSha512Hash))
        emit hashesChanged(::toSharedPointer(this));
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
    // Uncoment to debug authorization related problems.
    // NX_ASSERT(m_digest.isEmpty() || !realm.isEmpty());
    if (setMemberChecked(&QnUserResource::m_realm, realm))
        emit hashesChanged(::toSharedPointer(this));
}

GlobalPermissions QnUserResource::getRawPermissions() const
{
    QnMutexLocker locker(&m_mutex);
    return m_permissions;
}

void QnUserResource::setRawPermissions(GlobalPermissions permissions)
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
        result = commonModule()->resourcePropertyDictionary()->value(getId(), Qn::USER_FULL_NAME);
    QnMutexLocker locker(&m_mutex);
    return result.isNull() ? m_fullName : result;
}

nx::vms::api::ResourceParamWithRefDataList QnUserResource::params() const
{
    nx::vms::api::ResourceParamWithRefDataList result;
    QString value;
    if (commonModule())
        value = commonModule()->resourcePropertyDictionary()->value(getId(), Qn::USER_FULL_NAME);
    if (value.isEmpty() && !fullName().isEmpty() && isCloud())
        value = fullName(); //< move fullName to property dictionary to sync data with cloud correctly
    if (!value.isEmpty())
        result.emplace_back(getId(), Qn::USER_FULL_NAME, value);

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
        NX_ASSERT(m_userType == localOther->m_userType, "User type was designed to be read-only");
        bool hashesChanged = false;
        if (m_password != localOther->m_password)
        {
            m_password = localOther->m_password;
            hashesChanged = true;
        }

        if (m_hash != localOther->m_hash)
        {
            m_hash = localOther->m_hash;
            hashesChanged = true;
        }

        if (m_digest != localOther->m_digest)
        {
            m_digest = localOther->m_digest;
            hashesChanged = true;
        }

        if (m_cryptSha512Hash != localOther->m_cryptSha512Hash)
        {
            m_cryptSha512Hash = localOther->m_cryptSha512Hash;
            hashesChanged = true;
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
            // Uncoment to debug authorization related problems.
            // NX_ASSERT(m_digest.isEmpty() || !localOther->m_realm.isEmpty());
            m_realm = localOther->m_realm;
            hashesChanged = true;
        }

        if (m_isEnabled != localOther->m_isEnabled)
        {
            m_isEnabled = localOther->m_isEnabled;
            notifiers << [r = toSharedPointer(this)]{ emit r->enabledChanged(r); };
        }

        if (hashesChanged)
            notifiers << [r = toSharedPointer(this)]{ emit r->hashesChanged(r); };
    }
}

Qn::ResourceStatus QnUserResource::getStatus() const
{
    return Qn::Online;
}

void QnUserResource::prolongatePassword()
{
    {
        QnMutexLocker lk(&m_mutex);
        if (!m_ldapPasswordTimer)
            m_ldapPasswordTimer = std::make_shared<nx::network::aio::Timer>();
    }
    m_ldapPasswordTimer->pleaseStopSync();
    m_ldapPasswordValid = true;
    m_ldapPasswordTimer->start(
        m_ldapPasswordExperationPeriod,
        [this]()
        {
            m_ldapPasswordValid = false;
            emit sessionExpired(toSharedPointer(this));
        });

}

bool QnUserResource::passwordExpired() const
{
    if (!isLdap())
        return false;
    return !m_ldapPasswordValid;
}

void QnUserResource::fillId()
{
    // ATTENTION: This logic is similar to UserData::fillId().
    NX_ASSERT(!(isCloud() && getEmail().isEmpty()));
    QnUuid id = isCloud() ? guidFromArbitraryData(getEmail()) : QnUuid::createUuid();
    setId(id);
}

QString QnUserResource::idForToStringFromPtr() const
{
    return getName();
}

void QnUserResource::setLdapPasswordExperationPeriod(std::chrono::milliseconds value)
{
    m_ldapPasswordExperationPeriod = value;
}
