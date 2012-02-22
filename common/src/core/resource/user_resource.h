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

    void setLayouts(const QnLayoutResourceList &layouts);    
    const QnLayoutResourceList &getLayouts() const;
    void addLayout(const QnLayoutResourcePtr &layout);
    void removeLayout(const QnLayoutResourcePtr &layout);

    QString getPassword() const;    
    void setPassword(const QString& password);

    bool isAdmin() const;    
    void setAdmin(bool admin = true);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QString m_password;
    bool m_isAdmin;
    QnLayoutResourceList m_layouts;
};

#endif // QN_USER_RESOURCE_H
