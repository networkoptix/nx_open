#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QtCore/QRectF>
#include <QtCore/QUuid>

#include <recording/time_period.h>

#include "resource.h"
#include "layout_item_data.h"

class QnLayoutResource: public QnResource {
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

    qreal cellAspectRatio() const;

    void setCellAspectRatio(qreal cellAspectRatio);

    QSizeF cellSpacing() const;

    void setCellSpacing(const QSizeF &cellSpacing);

    void setCellSpacing(qreal horizontalSpacing, qreal verticalSpacing);

    /**
     * Deserialize layout resource from file
     */
    static QnLayoutResourcePtr fromFile(const QString& xfile);

    QnTimePeriod timeBounds() const;

    void setTimeBounds(const QnTimePeriod &timeBounds);

signals:
    void itemAdded(const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutItemData &item);
    void itemChanged(const QnLayoutItemData &item);
    void cellAspectRatioChanged();
    void cellSpacingChanged();
    void timeBoundsChanged();

protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    void addItemUnderLock(const QnLayoutItemData &item);
    void updateItemUnderLock(const QUuid &itemUuid, const QnLayoutItemData &item);
    void removeItemUnderLock(const QUuid &itemUuid);

private:
    QnLayoutItemDataMap m_itemByUuid;
    QnTimePeriod m_timeBounds;
    qreal m_cellAspectRatio;
    QSizeF m_cellSpacing;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);

#endif // QN_LAYOUT_RESOURCE_H

