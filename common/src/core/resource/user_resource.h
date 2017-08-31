#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

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
    Qn::GlobalPermissions getRawPermissions() const;
    void setRawPermissions(Qn::GlobalPermissions permissions);

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

    ec2::ApiResourceParamWithRefDataList params() const;

    virtual Qn::ResourceStatus getStatus() const override;

    //!Millis since epoch (1970-01-01, UTC)
    qint64 passwordExpirationTimestamp() const;
    bool passwordExpired() const;

    static const qint64 PasswordProlongationAuto = -1;
    void prolongatePassword(qint64 value = PasswordProlongationAuto);

    /*
     * Fill ID field.
     * For regular users it is random value, for cloud users it's md5 hash from email address.
     */
    void fillId();

signals:
    void permissionsChanged(const QnUserResourcePtr& user);
    void userRoleChanged(const QnUserResourcePtr& user);
    void enabledChanged(const QnUserResourcePtr& user);

    void hashChanged(const QnResourcePtr& user);
    void passwordChanged(const QnResourcePtr& user);
    void digestChanged(const QnResourcePtr& user);
    void cryptSha512HashChanged(const QnResourcePtr& user);
    void emailChanged(const QnResourcePtr& user);
    void fullNameChanged(const QnResourcePtr& user);
	void realmChanged(const QnResourcePtr& user);

protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

private:
    using MiddlestepFunction = std::function<void ()>;

    template<typename T>
    bool setMemberChecked(
        T QnUserResource::* member,
        T value,
        MiddlestepFunction middlestep = MiddlestepFunction());

    void updateHash();

private:
    QnUserType m_userType;
    QString m_password;
    QByteArray m_hash;
    QByteArray m_digest;
    QByteArray m_cryptSha512Hash;
    QString m_realm;
    Qn::GlobalPermissions m_permissions;
    QnUuid m_userRoleId;
    bool m_isOwner;
    bool m_isEnabled;
    QString m_email;
    QString m_fullName;
    qint64 m_passwordExpirationTimestamp;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
