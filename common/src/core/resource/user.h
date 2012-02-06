#ifndef _RESOURCE_USER_H
#define _RESOURCE_USER_H

#include <QSharedPointer>
#include <QList>

#include "resource.h"
#include "layout_data.h"

class QnUserResource : public QnResource
{
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

#endif //_RESOURCE_USER_H
