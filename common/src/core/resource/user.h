#ifndef QN_USER_RESOURCE_H
#define QN_USER_RESOURCE_H

#include <QSharedPointer>
#include <QList>

#include "resource.h"
#include "layout_data.h"

class QnUserResource : public QnResource
{
    Q_OBJECT;

public:
    QnUserResource();

    QString getUniqueId() const
    {
        return getName();
    }

    void setLayouts(QnLayoutDataList layouts);
    QnLayoutDataList getLayouts() const;

    QString getPassword() const;
    void setPassword(const QString& password);

private:
    QString m_password;
    QnLayoutDataList m_layouts;
};

typedef QSharedPointer<QnUserResource> QnUserResourcePtr;
typedef QList<QnUserResourcePtr> QnUserResourceList;

#endif // QN_USER_RESOURCE_H
