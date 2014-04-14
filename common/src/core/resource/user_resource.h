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

    QString getHash() const;
    void setHash(const QString &hash);

    QString getPassword() const;
    void setPassword(const QString &password);

    QString getDigest() const;
    void setDigest(const QString& digest);

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

    QString getEmail() const;
    void setEmail(const QString &email);

signals:
    void emailChanged(const QnUserResourcePtr &user);
    void permissionsChanged(const QnUserResourcePtr &user);

protected:
    virtual void updateInner(QnResourcePtr other, QSet<QByteArray>& modifiedFields) override;

private:
    QString m_password;
    QString m_hash;
    QString m_digest;
    quint64 m_permissions;
    bool m_isAdmin;
    QString m_email;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
