#pragma once

#include <QtCore/QRectF>
#include <nx/utils/uuid.h>

#include <recording/time_period.h>

#include <core/resource/resource.h>
#include <core/resource/layout_item_data.h>

#include <utils/common/threadsafe_item_storage.h>


class QnLayoutResource: public QnResource,
    private QnThreadsafeItemStorageNotifier<QnLayoutItemData>
{
    Q_OBJECT

    typedef QnResource base_type;

public:
    QnLayoutResource();

    virtual QString getUniqueId() const override;
    virtual Qn::ResourceStatus getStatus() const override;

    QnLayoutResourcePtr clone() const;

    virtual QString toSearchString() const override;

    void setItems(const QnLayoutItemDataList &items);

    void setItems(const QnLayoutItemDataMap &items);

    QnLayoutItemDataMap getItems() const;

    QnLayoutItemData getItem(const QnUuid &itemUuid) const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    void removeItem(const QnUuid &itemUuid);

    void updateItem(const QnLayoutItemData &item);

    float cellAspectRatio() const;

    void setCellAspectRatio(float cellAspectRatio);

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

    /** Check if layout is an exported file. */
    bool isFile() const;

    /** Check if layout is shared. */
    bool isShared() const;

    QSet<QnResourcePtr> layoutResources() const;
    static QSet<QnResourcePtr> layoutResources(const QnLayoutItemDataMap& items);

signals:
    void itemAdded(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemChanged(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);

    void cellAspectRatioChanged(const QnLayoutResourcePtr &resource);
    void cellSpacingChanged(const QnLayoutResourcePtr &resource);
    void storeRequested(const QnLayoutResourcePtr &resource);

    void backgroundSizeChanged(const QnLayoutResourcePtr &resource);
    void backgroundImageChanged(const QnLayoutResourcePtr &resource);
    void backgroundOpacityChanged(const QnLayoutResourcePtr &resource);
    void lockedChanged(const QnLayoutResourcePtr &resource);

protected:
    virtual void storedItemAdded(const QnLayoutItemData& item) override;
    virtual void storedItemRemoved(const QnLayoutItemData& item) override;
    virtual void storedItemChanged(const QnLayoutItemData& item) override;

    virtual void updateInternal(const QnResourcePtr &other, QList<UpdateNotifier>& notifiers) override;

private:
    QScopedPointer<QnThreadsafeItemStorage<QnLayoutItemData> > m_items;
    float m_cellAspectRatio;
    QSizeF m_cellSpacing;
    QHash<int, QVariant> m_dataByRole;
    QnTimePeriod m_localRange;
    QSize m_backgroundSize;
    QString m_backgroundImageFilename;
    qreal m_backgroundOpacity;
    bool m_locked;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);
