#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <QSharedPointer>
#include <QList>

#include "resource.h"
#include "layout_data.h"

class QnUserResource : public QnResource
{
    Q_OBJECT;

    typedef QnResource base_type;

public:
    QnUserResource();

    virtual QString getUniqueId() const override
    {
        return getName();
    }

    void setLayouts(QnLayoutDataList layouts);
    QnLayoutDataList getLayouts() const;

    QString getPassword() const;
    void setPassword(const QString& password);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QString m_password;
    QnLayoutDataList m_layouts;
};

typedef QSharedPointer<QnUserResource> QnUserResourcePtr;
typedef QList<QnUserResourcePtr> QnUserResourceList;

#endif // QN_USER_RESOURCE_H
