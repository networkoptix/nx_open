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
    QnUserResource(QnUserType userType);
    QnUserResource(const QnUserResource& right);

    QnUserType userType() const;
    bool isLdap()  const { return userType() == QnUserType::Ldap;  }
    bool isCloud() const { return userType() == QnUserType::Cloud; }
    bool isLocal() const { return userType() == QnUserType::Local; }

    QByteArray getHash() const;
    void setHash(const QByteArray& hash);

    QString getPassword() const;
    void setPassword(const QString& password);

    void generateHash();
    bool checkPassword(const QString& password);

    QByteArray getDigest() const;
    void setDigest(const QByteArray& digest, bool isValidated = false);

    QByteArray getCryptSha512Hash() const;
    void setCryptSha512Hash(const QByteArray& hash);

    QString getRealm() const;
    void setRealm( const QString& realm );

    // Do not use this method directly.
    // Use qnResourceAccessManager::globalPermissions(user) instead
    Qn::GlobalPermissions getRawPermissions() const;
    void setRawPermissions(Qn::GlobalPermissions permissions);

    bool isOwner() const;
    void setOwner(bool isOwner);

    QnUuid userGroup() const;
    void setUserGroup(const QnUuid& group);

    bool isEnabled() const;
    void setEnabled(bool isEnabled);

    QString getEmail() const;
    void setEmail(const QString& email);

    virtual Qn::ResourceStatus getStatus() const override;

    //!Millis since epoch (1970-01-01, UTC)
    qint64 passwordExpirationTimestamp() const;
    bool passwordExpired() const;

    static const qint64 PasswordProlongationAuto = -1;
    void prolongatePassword(qint64 value = PasswordProlongationAuto);

signals:
    void hashChanged(const QnResourcePtr& user);
    void passwordChanged(const QnResourcePtr& user);
    void digestChanged(const QnResourcePtr& user);
    void cryptSha512HashChanged(const QnResourcePtr& user);
    void permissionsChanged(const QnResourcePtr& user);
    void userGroupChanged(const QnResourcePtr& user);
    void emailChanged(const QnResourcePtr& user);
	void realmChanged(const QnResourcePtr& user);
    void enabledChanged(const QnResourcePtr& user);

protected:
    virtual void updateInner(const QnResourcePtr& other, QSet<QByteArray>& modifiedFields) override;

private:
    QnUserType m_userType;
    QString m_password;
    QByteArray m_hash;
    QByteArray m_digest;
    QByteArray m_cryptSha512Hash;
    QString m_realm;
    Qn::GlobalPermissions m_permissions;
    QnUuid m_userGroup;
    bool m_isOwner;
    bool m_isEnabled;
    QString m_email;
    qint64 m_passwordExpirationTimestamp;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
