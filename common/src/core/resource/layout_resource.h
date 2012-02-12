#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QRectF>
#include <QUuid>
#include "resource.h"

class QnLayoutItemData
{
public:
    QnLayoutItemData(): flags(0), rotation(0) {}

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

    void setItems(const QnLayoutItemDataList &items);

    const QnLayoutItemDataList &getItems() const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    void removeItem(const QUuid &itemUuid);

signals:
    void itemAdded(const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutItemData &item);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QList<QnLayoutItemData> m_items;
};


#endif // QN_LAYOUT_RESOURCE_H
