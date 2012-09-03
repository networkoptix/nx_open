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

    QString getPassword() const;    
    void setPassword(const QString &password);

    quint64 getPermissions() const;
    void setPermissions(quint64 permissions);

    bool isAdmin() const;
    void setAdmin(bool isAdmin);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QString m_password;
    quint64 m_permissions;
    bool m_isAdmin;
};

Q_DECLARE_METATYPE(QnUserResourcePtr)
Q_DECLARE_METATYPE(QnUserResourceList)

#endif // QN_USER_RESOURCE_H
