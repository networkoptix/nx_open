#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QRectF>
#include <QUuid>
#include <utils/common/fuzzy.h>
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

    friend bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) {
        return 
            l.resourceId == r.resourceId &&
            l.uuid == r.uuid &&
            l.flags == r.flags &&
            qFuzzyCompare(l.combinedGeometry, r.combinedGeometry) &&
            qFuzzyCompare(l.rotation, r.rotation);
    }
};



typedef QList<QnLayoutItemData> QnLayoutItemDataList;
typedef QHash<QUuid, QnLayoutItemData> QnLayoutItemDataMap;

class QnLayoutResource : public QnResource
{
    Q_OBJECT;

    typedef QnResource base_type;

public:
    QnLayoutResource();

    virtual QString getUniqueId() const override;

    void setItems(const QnLayoutItemDataList &items);

    QnLayoutItemDataList getItems() const;

    const QnLayoutItemDataMap &getItemMap() const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    void removeItem(const QUuid &itemUuid);

signals:
    void itemAdded(const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutItemData &item);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QnLayoutItemDataMap m_itemByUuid;
};


#endif // QN_LAYOUT_RESOURCE_H
