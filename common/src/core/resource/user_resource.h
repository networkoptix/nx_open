#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnUserResource();
    QnUserResource(const QnUserResource&);

    QByteArray getHash() const;
    void setHash(const QByteArray&hash);

    QString getPassword() const;
    void setPassword(const QString &password);

    void generateHash();
    bool checkPassword(const QString &password);

    QByteArray getDigest() const;
    void setDigest(const QByteArray& digest, bool isValidated = false);

    QByteArray getCryptSha512Hash() const;
    void setCryptSha512Hash(const QByteArray& hash);

    QString getRealm() const;
    void setRealm( const QString& realm );

    // Do not use this method directly.
    // Use qnResourceAccessManager::globalPermissions(user) instead
    Qn::GlobalPermissions getPermissions() const;
    void setPermissions(Qn::GlobalPermissions permissions);

    bool isOwner() const;
    void setOwner(bool isOwner);

    bool isLdap() const;
    void setLdap(bool isLdap);

    bool isEnabled() const;
    void setEnabled(bool isEnabled);

    QString getEmail() const;
    void setEmail(const QString &email);
    virtual Qn::ResourceStatus getStatus() const override;

    //!Millis since epoch (1970-01-01, UTC)
    qint64 passwordExpirationTimestamp() const;
    bool passwordExpired() const;

    static const qint64 PasswordProlongationAuto = -1;
    void prolongatePassword(qint64 value = PasswordProlongationAuto);

signals:
    void hashChanged(const QnResourcePtr &resource);
    void passwordChanged(const QnResourcePtr &resource);
    void digestChanged(const QnResourcePtr &resource);
    void cryptSha512HashChanged(const QnResourcePtr &resource);
    void permissionsChanged(const QnResourcePtr &user);
    void emailChanged(const QnResourcePtr &user);
	void realmChanged(const QnResourcePtr &user);
    void enabledChanged(const QnResourcePtr &user);
    void ldapChanged(const QnResourcePtr &user);

protected:
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

private:
    QString m_password;
    QByteArray m_hash;
    QByteArray m_digest;
    QByteArray m_cryptSha512Hash;
    QString m_realm;
    Qn::GlobalPermissions m_permissions;
    bool m_isOwner;
    bool m_isLdap;
    bool m_isEnabled;
    QString m_email;
    qint64 m_passwordExpirationTimestamp;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
