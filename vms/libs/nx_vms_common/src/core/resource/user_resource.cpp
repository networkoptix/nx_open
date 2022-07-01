// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_resource.h"

#include <QtCore/QCryptographicHash>

#include <nx/utils/switch.h>
#include <api/model/password_data.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_properties.h>
#include <utils/common/synctime.h>
#include <utils/common/id.h>
#include <utils/crypt/symmetrical.h>
#include <utils/common/ldap.h>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/network/aio/timer.h>

#include <nx/vms/api/data/user_data.h>

const QnUuid QnUserResource::kAdminGuid("99cbc715-539b-4bfe-856f-799b45b69b1e");

QByteArray QnUserHash::toString(const Type& type)
{
    switch (type)
    {
        case Type::none: return "";
        case Type::cloud: return nx::vms::api::UserData::kCloudPasswordStub;
        case Type::md5: return "md5";
        case Type::ldapPassword: return "LDAP";
        case Type::scrypt: return "scrypt";
    };

    NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(type));
    return NX_FMT("unexpected_value_%1", static_cast<int>(type)).toUtf8();
}

QnUserHash::QnUserHash(const QByteArray& data)
{
    if (data == nx::vms::api::UserData::kCloudPasswordStub)
    {
        type = Type::cloud;
        return;
    }

    const auto parts = data.split('$');
    if (parts.size() == 3 && parts[0] == "md5")
    {
        type = Type::md5;
        salt = parts[1];
        hash = parts[2];
        NX_VERBOSE(this, "Parsed successfully");
        return;
    }

    if (parts.size() == 3 && parts[0] == "LDAP")
    {
        type = Type::ldapPassword;
        salt = parts[1];
        hash = parts[2];
        NX_VERBOSE(this, "Parsed successfully");
        return;
    }

    if (parts.size() == 6 && parts[0] == "scrypt")
    {
        nx::scrypt::Options parsedOptions;
        parsedOptions.r = std::stoul(parts[2].data());
        parsedOptions.N = std::stoul(parts[3].data());
        parsedOptions.p = std::stoul(parts[4].data());
        parsedOptions.keySize = parts[5].size() / 2; //< Due to hex.
        if (parsedOptions.isValid())
        {
            type = Type::scrypt;
            salt = parts[1];
            hash = parts[5];
            options = parsedOptions;
            NX_VERBOSE(this, "Parsed successfully with options %1", options);
            return;
        }
    }

    if (!data.isEmpty())
        NX_WARNING(this, "Unable to parse hash: %1", data);
}

QByteArray QnUserHash::toString() const
{
    switch (type)
    {
        case Type::none:
        case Type::cloud:
            return toString(type);

        case Type::md5:
        case Type::ldapPassword:
            return NX_FMT("%1$%2$%3", toString(type), salt, hash).toUtf8();

        case Type::scrypt:
            if (!NX_ASSERT(options))
                return "";

            return NX_FMT("%1$%2$%3$%4$%5$%6", toString(type), salt,
                options->r, options->N, options->p, hash).toUtf8();
    };

    NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(type));
    return NX_FMT("unexpected_value_%1", static_cast<int>(type)).toUtf8();
}

bool QnUserHash::operator==(const QnUserHash& rhs) const
{
    return type == rhs.type
        && salt == rhs.salt
        && options == rhs.options
        && hash == rhs.hash;
}

QByteArray QnUserHash::hashPassword(const QString& password) const
{
    switch (type)
    {
        case Type::none:
            NX_VERBOSE(this, "None does not store password information");
            return "";

        case Type::cloud:
            NX_VERBOSE(this, "Password is stored in cloud, hash calculation is not possible");
            return "";

        case Type::md5:
        {
            QCryptographicHash md5(QCryptographicHash::Md5);
            md5.addData(salt);
            md5.addData(password.toUtf8());
            return md5.result().toHex();
        }

        case Type::ldapPassword:
            return nx::utils::encodeSimple(password.toUtf8(), QByteArray::fromHex(salt)).toHex();

        case Type::scrypt:
            if (!NX_ASSERT(options))
                return "";
            try
            {
                const auto h = nx::scrypt::encodeOrThrow(password.toStdString(), salt.data(), *options);
                return QByteArray(h.c_str(), h.length());
            }
            catch (nx::scrypt::Exception& e)
            {
                NX_DEBUG(this, "Unable to SCrypt password: %1", e);
                return "";
            }
    };

    NX_ASSERT(false, "Unexpected value: %1", static_cast<int>(type));
    return NX_FMT("unexpected_value_%1", static_cast<int>(type)).toUtf8();
}

bool QnUserHash::checkPassword(const QString& password) const
{
    const auto passwordHash = hashPassword(password);
    if (passwordHash.isEmpty())
        return false; //< Something went wrong...

    const auto isCorrect = (hash == passwordHash);
    NX_VERBOSE(this, "Password hash %1 is %2", passwordHash, isCorrect ? "correct": "incorrect");
    return isCorrect;
}

QString QnUserHash::toLdapPassword() const
{
    return nx::utils::encodeSimple(QByteArray::fromHex(hash), QByteArray::fromHex(salt));
}

QnUserHash QnUserHash::ldapPassword(const QString& password)
{
    QnUserHash result;
    result.type = Type::ldapPassword;
    result.salt = QByteArray::number(nx::utils::random::number(), 16);
    result.hash = result.hashPassword(password);
    return result;
}

QnUserHash QnUserHash::scryptPassword(const QString& password, nx::scrypt::Options options)
{
    QnUserHash result;
    result.type = Type::scrypt;
    result.salt = QByteArray::number(nx::utils::random::number(), 16);
    result.options = std::move(options);
    result.hash = result.hashPassword(password);
    return result;
}

QnUserResource::QnUserResource(nx::vms::api::UserType userType):
    m_userType(userType),
    m_realm(nx::network::AppInfo::realm().c_str())
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
    m_permissions(right.m_permissions.load()),
    m_userRoleId(right.m_userRoleId),
    m_isOwner(right.m_isOwner.load()),
    m_isEnabled(right.m_isEnabled.load()),
    m_email(right.m_email),
    m_fullName(right.m_fullName)
{
}

QnUserResource::~QnUserResource()
{
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

QnUserHash QnUserResource::getHash() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_hash;
}

QString QnUserResource::getPassword() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_password;
}

void QnUserResource::setPasswordAndGenerateHash(const QString& password, DigestSupport digestSupport)
{
    NX_MUTEX_LOCKER locker(&m_mutex);

    const bool isNewPassword = !m_hash.checkPassword(password);
    if (isNewPassword || m_password.isEmpty())
        m_password = password;

    auto hashes = PasswordHashes::calculateHashes(
        m_name, password, m_userType == nx::vms::api::UserType::ldap);
    if (digestSupport == DigestSupport::disable
        || (digestSupport == DigestSupport::keep
            && m_digest == nx::vms::api::UserData::kHttpIsDisabledStub))
    {
        hashes.passwordDigest = nx::vms::api::UserData::kHttpIsDisabledStub;
    }

    const bool digestHasChanged = (m_digest != hashes.passwordDigest);
    setPasswordHashesInternal(hashes, isNewPassword);
    locker.unlock();
    if (isNewPassword)
    {
        NX_VERBOSE(this, "Password is changed by setting a new%1",
            nx::utils::log::showPasswords() ? (":" + password) : QString());
        emit passwordChanged(::toSharedPointer(this));
    }
    if (digestHasChanged)
        emit digestChanged(::toSharedPointer(this));
}

void QnUserResource::resetPassword()
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_password.clear();
}

bool QnUserResource::digestAuthorizationEnabled() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return (m_digest != nx::vms::api::UserData::kHttpIsDisabledStub);
}

bool QnUserResource::shouldDigestAuthBeUsed() const
{
    if (isCloud())
        return false;
    return digestAuthorizationEnabled();
}

QByteArray QnUserResource::getDigest() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_digest;
}

QByteArray QnUserResource::getCryptSha512Hash() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_cryptSha512Hash;
}

QString QnUserResource::getRealm() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_realm;
}

GlobalPermissions QnUserResource::getRawPermissions() const
{
    return m_permissions;
}

void QnUserResource::setRawPermissions(GlobalPermissions permissions)
{
    const auto oldValue = m_permissions.exchange(permissions);
    if (oldValue != permissions)
        emit permissionsChanged(::toSharedPointer(this));
}

void QnUserResource::setPasswordHashes(const PasswordHashes& hashes)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    const bool isNewPassword = m_hash.toString() != hashes.passwordHash;
    const bool isNewDigest = m_digest != hashes.passwordDigest;
    setPasswordHashesInternal(hashes, isNewPassword);
    locker.unlock();
    if (isNewPassword)
    {
        NX_VERBOSE(this, "Password is changed by hashes");
        emit passwordChanged(::toSharedPointer(this));
    }
    if (isNewDigest)
        emit digestChanged(::toSharedPointer(this));
}

void QnUserResource::setPasswordHashesInternal(const PasswordHashes& hashes, bool isNewPassword)
{
    const bool isNewRealm = m_realm != hashes.realm;
    if (isNewRealm)
        m_realm = hashes.realm;

    using UserType = nx::vms::api::UserType;
    const bool isNewHash = isNewPassword || nx::utils::switch_(m_userType,
        UserType::local, [type = m_hash.type] { return type != QnUserHash::Type::scrypt; },
        UserType::ldap, [type = m_hash.type] { return type != QnUserHash::Type::ldapPassword; },
        UserType::cloud, [type = m_hash.type]{ return type != QnUserHash::Type::cloud; },
        nx::utils::default_, [this] { NX_ASSERT(false, this); return false; });
    if (isNewHash)
        m_hash = QnUserHash(hashes.passwordHash);

    const bool isNewDigest = m_digest != hashes.passwordDigest;
    if (isNewDigest)
        m_digest = hashes.passwordDigest;

    const bool isNewCryptSha512Hash = isNewPassword && m_cryptSha512Hash != hashes.cryptSha512Hash;
    if (isNewCryptSha512Hash)
        m_cryptSha512Hash = hashes.cryptSha512Hash;

    m_mutex.unlock();
    if (isNewRealm)
        NX_VERBOSE(this, "Updated realm: %1", hashes.realm);

    if (isNewHash)
        NX_VERBOSE(this, "Updated hash: %1", hashes.passwordHash);

    if (isNewDigest)
        NX_VERBOSE(this, "Updated digest: %1", hashes.passwordDigest);

    if (isNewCryptSha512Hash)
        NX_VERBOSE(this, "Updated crypt SHA512 hash: %1", hashes.cryptSha512Hash);

    m_mutex.lock();
}

bool QnUserResource::isBuiltInAdmin() const
{
    return getId() == kAdminGuid;
}

bool QnUserResource::isOwner() const
{
    return m_isOwner;
}

void QnUserResource::setOwner(bool isOwner)
{
    const auto oldValue = m_isOwner.exchange(isOwner);
    if (oldValue != isOwner)
        emit permissionsChanged(::toSharedPointer(this));
}

QnUuid QnUserResource::userRoleId() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_userRoleId;
}

void QnUserResource::setUserRoleId(const QnUuid& userRoleId)
{
    QnUuid previousRoleId;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_userRoleId == userRoleId)
            return;
        previousRoleId = m_userRoleId;
        m_userRoleId = userRoleId;
    }
    emit userRoleChanged(::toSharedPointer(this), previousRoleId);
}

bool QnUserResource::isEnabled() const
{
    return m_isEnabled;
}

void QnUserResource::setEnabled(bool isEnabled)
{
    const auto oldValue = m_isEnabled.exchange(isEnabled);
    if (oldValue != isEnabled)
        emit enabledChanged(::toSharedPointer(this));
}

nx::vms::api::UserType QnUserResource::userType() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_userType;
}

QString QnUserResource::getEmail() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_email;
}

void QnUserResource::setEmail(const QString& email)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
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
    NX_MUTEX_LOCKER locker(&m_mutex);
    return result.isNull() ? m_fullName : result;
}

void QnUserResource::setFullName(const QString& value)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
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
        NX_ASSERT(m_userType == localOther->m_userType, "%1: User type was designed to be read-only", this);

        bool isEmptyOtherPasswordAcceptable = false;
        if (m_hash != localOther->m_hash)
        {
            if (m_password.isEmpty() && localOther->m_password.isEmpty())
            {
                notifiers <<
                    [other, r = toSharedPointer(this)]
                    {
                        NX_VERBOSE(r.data(), "Password is changed by update of hash from %1", other);
                        emit r->passwordChanged(r);
                    };
            }
            if (m_password != localOther->m_password
                && !localOther->m_hash.checkPassword(m_password)
                && !m_hash.checkPassword(localOther->m_password))
            {
                isEmptyOtherPasswordAcceptable = true;
                notifiers <<
                    [other, r = toSharedPointer(this)]
                    {
                        NX_VERBOSE(r.data(), "Password is changed by update from %1", other);
                        emit r->passwordChanged(r);
                    };
            }
            m_hash = localOther->m_hash;
        }

        if (m_password != localOther->m_password
            && (!localOther->m_password.isEmpty() || isEmptyOtherPasswordAcceptable))
        {
            m_password = localOther->m_password;
        }

        m_realm = localOther->m_realm;
        if (m_digest != localOther->m_digest)
        {
            m_digest = localOther->m_digest;
            notifiers << [r = toSharedPointer(this)] { emit r->digestChanged(r); };
        }
        m_cryptSha512Hash = localOther->m_cryptSha512Hash;

        const auto newPermissions = localOther->m_permissions.load();
        const auto oldPermissions = m_permissions.exchange(newPermissions);
        if (oldPermissions != newPermissions)
            notifiers << [r = toSharedPointer(this)]{ emit r->permissionsChanged(r); };

        if (m_userRoleId != localOther->m_userRoleId)
        {
            const auto previousRoleId = m_userRoleId;
            m_userRoleId = localOther->m_userRoleId;
            notifiers << [r = toSharedPointer(this), previousRoleId]
                { emit r->userRoleChanged(r, previousRoleId); };
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

        const bool isEnabled = localOther->m_isEnabled.load();
        const bool wasEnabled = m_isEnabled.exchange(isEnabled);
        if (isEnabled != wasEnabled)
            notifiers << [r = toSharedPointer(this)]{ emit r->enabledChanged(r); };

        if (m_isOwner != localOther->m_isOwner)
        {
            m_isOwner.store(localOther->m_isOwner.load());
            notifiers << [r = toSharedPointer(this)] { emit r->permissionsChanged(r); };
        }
    }
}

nx::vms::api::ResourceStatus QnUserResource::getStatus() const
{
    return nx::vms::api::ResourceStatus::online;
}

void QnUserResource::fillIdUnsafe()
{
    // ATTENTION: This logic is similar to UserData::fillId().
    NX_ASSERT(!(isCloud() && getEmail().isEmpty()));
    QnUuid id = isCloud() ? guidFromArbitraryData(getEmail()) : QnUuid::createUuid();
    setIdUnsafe(id);
}

QString QnUserResource::idForToStringFromPtr() const
{
    return getName();
}
