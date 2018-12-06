#pragma once

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

namespace nx { namespace network { namespace aio { class Timer; } } }

enum class QnUserType
{
    Local,
    Ldap,
    Cloud
};

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    static const QnUuid kAdminGuid;

    QnUserResource(QnUserType userType);
    QnUserResource(const QnUserResource& right);
    virtual ~QnUserResource();

    QnUserType userType() const;
    bool isLdap()  const { return userType() == QnUserType::Ldap;  }
    bool isCloud() const { return userType() == QnUserType::Cloud; }
    bool isLocal() const { return userType() == QnUserType::Local; }

    Qn::UserRole userRole() const;

    QByteArray getHash() const;
    void setHash(const QByteArray& hash);

    QString getPassword() const;
    void setPasswordAndGenerateHash(const QString& password);
    void resetPassword();

    QString decodeLDAPPassword() const;

    bool checkLocalUserPassword(const QString &password);

    QByteArray getDigest() const;
    void setDigest(const QByteArray& digest);

    QByteArray getCryptSha512Hash() const;
    void setCryptSha512Hash(const QByteArray& hash);

    QString getRealm() const;
    void setRealm(const QString& realm);

    // Do not use this method directly.
    // Use resourceAccessManager()::globalPermissions(user) instead
    GlobalPermissions getRawPermissions() const;
    void setRawPermissions(GlobalPermissions permissions);

    /**
     * Owner user has maxumum permissions. Could be local or cloud user
     */
    bool isOwner() const;
    void setOwner(bool isOwner);

    /**
     * Predefined local owner.
     * 'isOwner=true' for 'admin' user and can't be reset. Could be disabled for login. Can't be removed.
     */
    bool isBuiltInAdmin() const;

    QnUuid userRoleId() const;
    void setUserRoleId(const QnUuid& userRoleId);

    bool isEnabled() const;
    void setEnabled(bool isEnabled);

    QString getEmail() const;
    void setEmail(const QString& email);

    QString fullName() const;
    void setFullName(const QString& value);

    nx::vms::api::ResourceParamWithRefDataList params() const;

    virtual Qn::ResourceStatus getStatus() const override;

    //!Millis since epoch (1970-01-01, UTC)
    qint64 passwordExpirationTimestamp() const;
    bool passwordExpired() const;

    void prolongatePassword();

    /*
     * Fill ID field.
     * For regular users it is random value, for cloud users it's md5 hash from email address.
     */
    void fillId();

    virtual QString idForToStringFromPtr() const override; //< Used by toString(const T*).

    void setLdapPasswordExperationPeriod(std::chrono::milliseconds value);
signals:
    void permissionsChanged(const QnUserResourcePtr& user);
    void userRoleChanged(const QnUserResourcePtr& user);
    void enabledChanged(const QnUserResourcePtr& user);

    void hashesChanged(const QnResourcePtr& user);

    void emailChanged(const QnResourcePtr& user);
    void fullNameChanged(const QnResourcePtr& user);
    void sessionExpired(const QnResourcePtr& user);
protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

private:
    using MiddlestepFunction = std::function<void ()>;

    template<typename T>
    bool setMemberChecked(
        T QnUserResource::* member,
        T value,
        MiddlestepFunction middlestep = MiddlestepFunction());

    bool updateHash();
private:
    QnUserType m_userType;
    QString m_password;
    QByteArray m_hash;
    QByteArray m_digest;
    QByteArray m_cryptSha512Hash;
    QString m_realm;
    GlobalPermissions m_permissions;
    QnUuid m_userRoleId;
    bool m_isOwner;
    bool m_isEnabled;
    QString m_email;
    QString m_fullName;
    std::shared_ptr<nx::network::aio::Timer> m_ldapPasswordTimer;
    std::atomic<bool> m_ldapPasswordValid{false};
    std::chrono::milliseconds m_ldapPasswordExperationPeriod{0};
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)
