#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QSharedPointer>
#include <QRectF>
#include <QUuid>
#include "resource.h"

class QnLayoutItemData
{
public:
    QnId resourceId;
    QUuid uuid;
    int flags;
    QRectF combinedGeometry;
    qreal rotation;
};

typedef QList<QnLayoutItemData> QnLayoutItemDataList;

class QnLayoutResource : public QnResource
{
    Q_OBJECT;

    typedef QnResource base_type;

public:
    QnLayoutResource();

    virtual QString getUniqueId() const override;

    void setItems(const QnLayoutItemDataList& items);

    const QnLayoutItemDataList &getItems() const
    {
        return m_items;
    }

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QList<QnLayoutItemData> m_items;
};

#endif // QN_LAYOUT_RESOURCE_H
