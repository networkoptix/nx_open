#pragma once

#include <QtCore/QRectF>
#include <nx/utils/uuid.h>

#include <recording/time_period.h>

#include <core/resource/resource.h>
#include <core/resource/layout_item_data.h>

#include <utils/common/threadsafe_item_storage.h>
#include <common/common_globals.h>

/**
 * QnLayoutResource class describes the set of resources together with their view options.
 *
 * There are several types of layouts:
 * 1) Common layouts. They belong to one of user. Layout's parentId is user's id.
 * 2) Shared layouts. They can be accessible to several users. ParentId is null.
 * 3) Service layouts. These can have server id or videowall id as parentId.
*/
class QnLayoutResource: public QnResource,
    private QnThreadsafeItemStorageNotifier<QnLayoutItemData>
{
    Q_OBJECT
    Q_PROPERTY(float cellAspectRatio
        READ cellAspectRatio WRITE setCellAspectRatio NOTIFY cellAspectRatioChanged)
    Q_PROPERTY(qreal cellSpacing
        READ cellSpacing WRITE setCellSpacing NOTIFY cellSpacingChanged)

    using base_type = QnResource;

public:
    QnLayoutResource(QnCommonModule* commonModule = nullptr);

    virtual QString getUniqueId() const override;
    virtual Qn::ResourceStatus getStatus() const override;

    QnLayoutResourcePtr clone() const;

    /** Create a new layout with a given resource on it. */
    static QnLayoutResourcePtr createFromResource(const QnResourcePtr& resource);

    virtual QString toSearchString() const override;

    void setItems(const QnLayoutItemDataList &items);

    void setItems(const QnLayoutItemDataMap &items);

    QnLayoutItemDataMap getItems() const;

    QnLayoutItemData getItem(const QnUuid &itemUuid) const;

    void addItem(const QnLayoutItemData &item);

    void removeItem(const QnLayoutItemData &item);

    Q_INVOKABLE void removeItem(const QnUuid &itemUuid);

    /**
     * @note Resource replacement is not supported for item.
     */
    void updateItem(const QnLayoutItemData &item);

    float cellAspectRatio() const;

    void setCellAspectRatio(float cellAspectRatio);

    bool hasCellAspectRatio() const;

    qreal cellSpacing() const;

    void setCellSpacing(qreal spacing);

    void setData(const QHash<int, QVariant> &dataByRole);

    void setData(int role, const QVariant &value);

    QVariant data(int role) const; // TODO: #ynikitenkov Possibly move to QnResourceRuntimeDataManager

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

    /**
     * Auto-generated service layout. Such layouts are used for:
     * * videowalls (videowall as a parent)
     * * videowall reviews (videowall as a parent)
     * * lite client control (server as a parent)
     */
    bool isServiceLayout() const;

    /** Get all resources ids placed on the layout. */
    QSet<QnUuid> layoutResourceIds() const;

    /** Get all resources placed on the layout. WARNING: method is SLOW! */
    QSet<QnResourcePtr> layoutResources() const;

    /** Get all resources placed on the layout. WARNING: method is SLOW! */
    static QSet<QnResourcePtr> layoutResources(QnResourcePool* resourcePool, const QnLayoutItemDataMap& items);

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

    void dataChanged(int role);

protected:
    virtual Qn::Notifier storedItemAdded(const QnLayoutItemData& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnLayoutItemData& item) override;
    virtual Qn::Notifier storedItemChanged(const QnLayoutItemData& item) override;

    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers) override;

private:
    QScopedPointer<QnThreadsafeItemStorage<QnLayoutItemData> > m_items;
    float m_cellAspectRatio;
    qreal m_cellSpacing;
    QHash<int, QVariant> m_dataByRole;
    QnTimePeriod m_localRange;
    QSize m_backgroundSize;
    QString m_backgroundImageFilename;
    qreal m_backgroundOpacity;
    bool m_locked;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);
