#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include "resource.h"
#include "layout_resource.h"

class QnUserResource : public QnResource
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnUserResource();

    virtual QString getUniqueId() const override;

    QByteArray getHash() const;
    void setHash(const QByteArray&hash);

    QString getPassword() const;
    void setPassword(const QString &password);

    QByteArray getDigest() const;
    void setDigest(const QByteArray& digest);

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

    QString getEmail() const;
    void setEmail(const QString &email);

signals:
    void emailChanged(const QnUserResourcePtr &user);

protected:
    virtual void updateInner(QnResourcePtr other) override;

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
