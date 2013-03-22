#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QtCore/QRectF>
#include <QtCore/QUuid>

#include <recording/time_period.h>

#include "resource.h"
#include "layout_item_data.h"
#include "recording/time_period.h"

class QnLayoutResource: public QnResource {
    Q_OBJECT

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

    void setData(const QHash<int, QVariant> &dataByRole);

    void setData(int role, const QVariant &value);

    QHash<int, QVariant> data() const;

    void requestStore() { emit storeRequested(::toSharedPointer(this)); } // TODO: hack

    QnTimePeriod getLocalRange() const;
    void setLocalRange(const QnTimePeriod& value);

    static QString updateNovParent(const QString& novName, const QString& itemName);

    virtual void setUrl(const QString& value) override;

    /** Size of background image - in cells */
    QSize backgroundSize() const;
    void setBackgroundSize(QSize size);

    /** Id of background image on EC */
    int backgroundImageId() const;
    void setBackgroundImageId(int id);

    /** Locked state - locked layout cannot be modified in any way */
    bool locked() const;
    void setLocked(bool value);
signals:
    void itemAdded(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemChanged(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void cellAspectRatioChanged(const QnLayoutResourcePtr &resource);
    void cellSpacingChanged(const QnLayoutResourcePtr &resource);
    void storeRequested(const QnLayoutResourcePtr &resource);

    void backgroundSizeChanged(const QnLayoutResourcePtr &resource);
    void backgroundImageChanged(const QnLayoutResourcePtr &resource);
    void lockedChanged(const QnLayoutResourcePtr &resource);
protected:
    virtual void updateInner(QnResourcePtr other) override;

private:
    void addItemUnderLock(const QnLayoutItemData &item);
    void updateItemUnderLock(const QUuid &itemUuid, const QnLayoutItemData &item);
    void removeItemUnderLock(const QUuid &itemUuid);

private:
    QnLayoutItemDataMap m_itemByUuid;
    qreal m_cellAspectRatio;
    QSizeF m_cellSpacing;
    QHash<int, QVariant> m_dataByRole;
    QnTimePeriod m_localRange;
    QSize m_backgroundSize;
    int m_backgroundImageId;
    bool m_locked;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);

#endif // QN_LAYOUT_RESOURCE_H

