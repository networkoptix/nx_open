// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <core/resource/resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/scrypt.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/integration_request.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/user_model.h>
#include <nx_ec/data/api_conversion_functions.h>

struct PasswordHashes;

namespace nx::network::aio { class Timer; }

namespace nx::vms::api {

struct ResourceParamWithRefData;
using ResourceParamWithRefDataList = std::vector<ResourceParamWithRefData>;

} // namespace nx::vms::api

struct NX_VMS_COMMON_API QnUserHash
{
    enum class Type
    {
        none,

        /**
         * Specifies, that password is in cloud.
         */
        cloud,

        /**
         *  Deprecated local user storage (low security).
         *  "md5$" + salt + "$" + md5(salt + password)
         */
        md5,

        /**
         *  LDAP users storage (low security, should be replaced in future).
         *  "LDAP$" + salt + "$" + encodeSimple(password, fromHex(salt))
         */
        ldapPassword,

        // Recommended local user storage.
        // "scrypt$" + salt + "$" + r + "$" + N + "$" + p + "$" + scrypt(salt, r, N, p, password)
        scrypt,

        /**
         * Temporary user hash - serialized TemporaryToken struct
         */
        temporary,
    };
    static QByteArray toString(const Type& type);

    Type type = Type::none;
    QByteArray salt;
    std::optional<nx::scrypt::Options> options; //< TODO: std::variant when there are more options.
    QByteArray hash;
    std::optional<nx::vms::api::TemporaryToken> temporaryToken;

    explicit QnUserHash(const QByteArray& data = "");
    QByteArray toString() const;
    QString idForToStringFromPtr() const { return toString(); }

    bool operator==(const QnUserHash& rhs) const;
    bool operator!=(const QnUserHash& rhs) const { return !(*this == rhs); }

    QByteArray hashPassword(const QString& password) const;
    bool checkPassword(const QString& password) const;
    bool compareToPasswordHash(const QByteArray& passwordHash) const;

    // These should be refactored out with insecure LDAP password storage.
    QString toLdapPassword() const;

    static QnUserHash ldapPassword(const QString& password);
    static QnUserHash scryptPassword(const QString& password, nx::scrypt::Options options);
};

class NX_VMS_COMMON_API QnUserResource: public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    friend void ec2::fromResourceToApi(const QnUserResource&, nx::vms::api::UserData&);
    friend void ec2::fromResourceToApi(const QnUserResourcePtr&, nx::vms::api::UserData&);
    friend void ec2::fromApiToResource(const nx::vms::api::UserData&, const QnUserResourcePtr&);

    static const QnUuid kAdminGuid;
    static const QString kIntegrationRequestDataProperty;

    QnUserResource(nx::vms::api::UserType userType, nx::vms::api::UserExternalId externalId);
    virtual ~QnUserResource();

    nx::vms::api::UserType userType() const;
    bool isLdap()  const { return userType() == nx::vms::api::UserType::ldap;  }
    bool isCloud() const { return userType() == nx::vms::api::UserType::cloud; }
    bool isLocal() const { return userType() == nx::vms::api::UserType::local; }
    bool isTemporary() const { return userType() == nx::vms::api::UserType::temporaryLocal; }

    // Returns time span by the end of which the temporary user be expired. If the user is
    // not temporary returns null.
    std::optional<std::chrono::seconds> temporarySessionExpiresIn() const;

    QnUserHash getHash() const;

    enum class DigestSupport
    {
        enable,
        disable,
        keep,
    };

    QString getPassword() const;
    bool setPasswordAndGenerateHash(const QString& password, DigestSupport digestSupport = DigestSupport::keep);
    void resetPassword();

    bool digestAuthorizationEnabled() const;
    bool shouldDigestAuthBeUsed() const;

    QByteArray getDigest() const;

    QByteArray getCryptSha512Hash() const;

    QString getRealm() const;

    void setPasswordHashes(const PasswordHashes& hashes);

    // Do not use this method directly.
    // Use resourceAccessManager()::globalPermissions(user) instead
    GlobalPermissions getRawPermissions() const;
    void setRawPermissions(GlobalPermissions permissions);

    /** Administrator has maximum permissions. Can be a local or Cloud user. */
    bool isAdministrator() const;

    /**
     * Predefined local administrator. For the `admin` user, `isAdministrator` is true and can't be
     * reset. Can be disabled for the login. Can't be removed.
     */
    bool isBuiltInAdmin() const;

    std::vector<QnUuid> groupIds() const;
    void setGroupIds(const std::vector<QnUuid>& value);

    void setResourceAccessRights(const std::map<QnUuid, nx::vms::api::AccessRights>& value);

    bool isEnabled() const;
    void setEnabled(bool isEnabled);

    QString getEmail() const;
    void setEmail(const QString& email);

    QString fullName() const;
    void setFullName(const QString& value);

    // External identification data, like dn and valid for LDAP users.
    nx::vms::api::UserExternalId externalId() const;
    void setExternalId(const nx::vms::api::UserExternalId& value);

    nx::vms::api::UserAttributes attributes() const;
    void setAttributes(nx::vms::api::UserAttributes value);

    std::optional<nx::vms::api::analytics::IntegrationRequestData> integrationRequestData() const;
    void setIntegrationRequestData(
        std::optional<nx::vms::api::analytics::IntegrationRequestData> integrationRequestData);

    std::map<QnUuid, nx::vms::api::AccessRights> ownResourceAccessRights() const;

    virtual nx::vms::api::ResourceStatus getStatus() const override;

    /*
     * Fill ID field.
     * For regular users it is random value, for cloud users it's md5 hash from email address.
     */
    void fillIdUnsafe();

    virtual QString idForToStringFromPtr() const override; //< Used by toString(const T*).

signals:
    void permissionsChanged(const QnUserResourcePtr& user);
    void userGroupsChanged(const QnUserResourcePtr& user, const std::vector<QnUuid>& previousRoleIds);

    void enabledChanged(const QnUserResourcePtr& user);

    // Emitted if the password has been changed for cloud or local user.
    void passwordChanged(const QnUserResourcePtr& user);
    void digestChanged(const QnUserResourcePtr& user);

    void emailChanged(const QnResourcePtr& user);
    void fullNameChanged(const QnResourcePtr& user);
    void externalIdChanged(const QnResourcePtr& user);
    void attributesChanged(const QnResourcePtr& user);

    // Emitted if Temporary user has any changes in the token an/or lifetime bounds.
    void temporaryTokenChanged(const QnUserResourcePtr& user);

protected:
    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    bool setPasswordHashesInternal(const PasswordHashes& hashes, bool isNewPassword);

private:
    nx::vms::api::UserType m_userType;
    QString m_password;
    QnUserHash m_hash;
    QByteArray m_digest;
    QByteArray m_cryptSha512Hash;
    QString m_realm;
    std::atomic<GlobalPermissions> m_permissions;
    std::vector<QnUuid> m_groupIds;
    std::atomic<bool> m_isAdministratorCache{false};
    std::atomic<bool> m_isEnabled{true};
    QString m_email;
    QString m_fullName;
    nx::vms::api::UserExternalId m_externalId;
    nx::vms::api::UserAttributes m_attributes;
    std::map<QnUuid, nx::vms::api::AccessRights> m_resourceAccessRights;
};
