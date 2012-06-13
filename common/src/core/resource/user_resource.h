#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include "resource.h"
#include "layout_resource.h"

class QnUserResource : public QnResource
{
    Q_OBJECT;

    typedef QnResource base_type;

public:
    QnUserResource();

    virtual QString getUniqueId() const override;

    QString getPassword() const;    
    void setPassword(const QString &password);

    bool isAdmin() const;    
    void setAdmin(bool admin = true);

protected:
    void initInternal() override {}
    virtual void updateInner(QnResourcePtr other) override;

private:
    QString m_password;
    bool m_isAdmin;
};

Q_DECLARE_METATYPE(QnUserResourcePtr);
Q_DECLARE_METATYPE(QnUserResourceList);

#endif // QN_USER_RESOURCE_H
