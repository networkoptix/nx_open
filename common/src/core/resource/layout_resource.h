#ifndef QN_LAYOUT_RESOURCE_H
#define QN_LAYOUT_RESOURCE_H

#include <QtCore/QRectF>
#include <utils/common/uuid.h>

#include <recording/time_period.h>

#include "resource.h"
#include "layout_item_data.h"

class QnLayoutResource: public QnResource {
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnLayoutResource(const QnResourceTypePool* resTypePool);

    QnLayoutResourcePtr clone() const;

    void setItems(const QnLayoutItemDataList &items);

    void setItems(const QnLayoutItemDataMap &items);

    QnLayoutItemDataMap getItems() const;

    QnLayoutItemData getItem(const QnUuid &itemUuid) const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    void removeItem(const QnUuid &itemUuid);

    void updateItem(const QnUuid &itemUuid, const QnLayoutItemData &item);

    qreal cellAspectRatio() const;

    void setCellAspectRatio(qreal cellAspectRatio);

    bool hasCellAspectRatio() const;

    QSizeF cellSpacing() const;

    void setCellSpacing(const QSizeF &cellSpacing);

    void setCellSpacing(qreal horizontalSpacing, qreal verticalSpacing);

    void setData(const QHash<int, QVariant> &dataByRole);

    void setData(int role, const QVariant &value);

    QHash<int, QVariant> data() const;

    void requestStore() { emit storeRequested(::toSharedPointer(this)); } // TODO: #Elric hack

    QnTimePeriod getLocalRange() const;
    void setLocalRange(const QnTimePeriod& value);

    virtual void setUrl(const QString& value) override;

    bool userCanEdit() const;
    void setUserCanEdit(bool value);

    /** Size of background image - in cells */
    QSize backgroundSize() const;
    void setBackgroundSize(QSize size);

    /** Filename of background image on Server */
    QString backgroundImageFilename() const;
    void setBackgroundImageFilename(const QString &filename);

    /** Background image opacity in range [0.0 .. 1.0] */
    qreal backgroundOpacity() const;
    void setBackgroundOpacity(qreal value);

    /** Locked state - locked layout cannot be modified in any way */
    bool locked() const;
    void setLocked(bool value);

    virtual Qn::ResourceStatus getStatus() const override;
signals:
    void itemAdded(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemChanged(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void cellAspectRatioChanged(const QnLayoutResourcePtr &resource);
    void cellSpacingChanged(const QnLayoutResourcePtr &resource);
    void userCanEditChanged(const QnLayoutResourcePtr &resource);
    void storeRequested(const QnLayoutResourcePtr &resource);

    void backgroundSizeChanged(const QnLayoutResourcePtr &resource);
    void backgroundImageChanged(const QnLayoutResourcePtr &resource);
    void backgroundOpacityChanged(const QnLayoutResourcePtr &resource);
    void lockedChanged(const QnLayoutResourcePtr &resource);
protected:
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) override;

private:
    void setItemsUnderLock(const QnLayoutItemDataMap &items);

    void addItemUnderLock(const QnLayoutItemData &item);
    void updateItemUnderLock(const QnUuid &itemUuid, const QnLayoutItemData &item);
    void removeItemUnderLock(const QnUuid &itemUuid);

private:
    QnLayoutItemDataMap m_itemByUuid;
    qreal m_cellAspectRatio;
    QSizeF m_cellSpacing;
    QHash<int, QVariant> m_dataByRole;
    QnTimePeriod m_localRange;
    bool m_userCanEdit;
    QSize m_backgroundSize;
    QString m_backgroundImageFilename;
    qreal m_backgroundOpacity;
    bool m_locked;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);

#endif // QN_LAYOUT_RESOURCE_H

