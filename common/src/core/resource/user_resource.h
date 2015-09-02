#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <utils/common/uuid.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnUserResource();

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

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

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
    /*!
        \return \a true if password expiration timestamp has been increased
    */
    Qn::AuthResult doPasswordProlongation();
    //!Check \a digest validity with external authentication service (LDAP currently)
    Qn::AuthResult checkDigestValidity( const QByteArray& digest );

signals:
    void hashChanged(const QnResourcePtr &resource);
    void passwordChanged(const QnResourcePtr &resource);
    void digestChanged(const QnResourcePtr &resource);
    void cryptSha512HashChanged(const QnResourcePtr &resource);
    void permissionsChanged(const QnResourcePtr &user);
    void adminChanged(const QnResourcePtr &resource);
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
    quint64 m_permissions;
    bool m_isAdmin;
	bool m_isLdap;
	bool m_isEnabled;
    QString m_email;
    qint64 m_passwordExpirationTimestamp;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
