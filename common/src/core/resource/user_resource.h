#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <QtCore/QUuid>

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
    void setDigest(const QByteArray& digest);

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

    QString getEmail() const;
    void setEmail(const QString &email);
signals:
    void hashChanged(const QnResourcePtr &resource);
    void passwordChanged(const QnResourcePtr &resource);
    void digestChanged(const QnResourcePtr &resource);
    void permissionsChanged(const QnResourcePtr &user);
    void adminChanged(const QnResourcePtr &resource);
    void emailChanged(const QnResourcePtr &user);
protected:
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

private:
    QString m_password;
    QByteArray m_hash;
    QByteArray m_digest;
    quint64 m_permissions;
    bool m_isAdmin;
    QString m_email;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
