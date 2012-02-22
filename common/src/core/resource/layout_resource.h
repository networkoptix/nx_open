#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QRectF>
#include <QUuid>
#include <QMetaType>
#include <utils/common/fuzzy.h>
#include "resource.h"

class QnLayoutItemData
{
public:
    QnLayoutItemData(): flags(0), rotation(0) {}

    struct {
        QnId id;
        QString path;
    } resource;

    QUuid uuid;
    int flags;
    QRectF combinedGeometry;
    qreal rotation;

    friend bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r) {
        return 
            l.resource.id == r.resource.id &&
            l.resource.path == r.resource.path &&
            l.uuid == r.uuid &&
            l.flags == r.flags &&
            qFuzzyCompare(l.combinedGeometry, r.combinedGeometry) &&
            qFuzzyCompare(l.rotation, r.rotation);
    }
};

Q_DECLARE_METATYPE(QnLayoutItemData);
Q_DECLARE_TYPEINFO(QnLayoutItemData, Q_MOVABLE_TYPE);

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

    void setItems(const QnLayoutItemDataMap &items);

    QnLayoutItemDataMap getItems() const;

    QnLayoutItemData getItem(const QUuid &itemUuid) const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    void removeItem(const QUuid &itemUuid);

    void updateItem(const QUuid &itemUuid, const QnLayoutItemData &item);

signals:
    void itemAdded(const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutItemData &item);
    void itemChanged(const QnLayoutItemData &item);

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    QnLayoutItemDataMap m_itemByUuid;
};




#endif // QN_LAYOUT_RESOURCE_H
