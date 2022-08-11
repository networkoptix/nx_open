// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <nx/utils/uuid.h>

#include <recording/time_period.h>

#include <core/resource/resource.h>
#include <core/resource/layout_item_data.h>

#include <utils/common/threadsafe_item_storage.h>
#include <common/common_globals.h>

#include <nx/vms/api/data/layout_data.h>

/**
 * QnLayoutResource class describes the set of resources together with their view options.
 *
 * There are several types of layouts:
 * 1) Common layouts. They belong to one of user. Layout's parentId is user's id.
 * 2) Shared layouts. They can be accessible to several users. ParentId is null.
 * 3) Service layouts. These can have server id or videowall id as parentId.
*/
class NX_VMS_COMMON_API QnLayoutResource:
    public QnResource,
    private QnThreadsafeItemStorageNotifier<QnLayoutItemData>
{
    Q_OBJECT
    Q_PROPERTY(float cellAspectRatio
        READ cellAspectRatio WRITE setCellAspectRatio NOTIFY cellAspectRatioChanged)
    Q_PROPERTY(qreal cellSpacing
        READ cellSpacing WRITE setCellSpacing NOTIFY cellSpacingChanged)

    using base_type = QnResource;

public:
    QnLayoutResource();

    virtual nx::vms::api::ResourceStatus getStatus() const override;
    virtual void setStatus(nx::vms::api::ResourceStatus newStatus,
        Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local) override;

    using ItemsRemapHash = QHash<QnUuid, QnUuid>;

    /**
     * Clone items of the current layout to the target layout. Ids of the layout items will be
     *     re-generated, zoom links correspondingly fixed.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneItems(QnLayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the target one.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneTo(QnLayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the new layout.
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     * @return New layout (not added to any system context).
     */
    QnLayoutResourcePtr clone(ItemsRemapHash* remapHash = nullptr) const;

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

    static constexpr float kDefaultCellAspectRatio
        = nx::vms::api::LayoutData::kDefaultCellAspectRatio;

    float cellAspectRatio() const;

    void setCellAspectRatio(float cellAspectRatio);

    bool hasCellAspectRatio() const;

    static constexpr float kDefaultCellSpacing = nx::vms::api::LayoutData::kDefaultCellSpacing;

    qreal cellSpacing() const;

    void setCellSpacing(qreal spacing);

    void setData(const QHash<int, QVariant> &dataByRole);

    void setData(int role, const QVariant &value);

    QVariant data(int role) const; // TODO: #ynikitenkov Possibly move to QnResourceRuntimeDataManager

    QHash<int, QVariant> data() const;

    void requestStore() { emit storeRequested(::toSharedPointer(this)); }

    QnTimePeriod getLocalRange() const;
    void setLocalRange(const QnTimePeriod& value);

    virtual void setUrl(const QString& value) override;

    /** Size of background image - in cells */
    QSize backgroundSize() const;
    void setBackgroundSize(QSize size);

    /** Helper to get the actual scene item coordinates of the layout background. */
    QRect backgroundRect() const;
    static QRect backgroundRect(const QSize& backgroundSize);

    /** Filename of background image on Server */
    bool hasBackground() const;
    QString backgroundImageFilename() const;
    void setBackgroundImageFilename(const QString &filename);

    /** Background image opacity in range [0.0 .. 1.0] */
    qreal backgroundOpacity() const;
    void setBackgroundOpacity(qreal value);

    /** Locked state - locked layout cannot be modified in any way */
    bool locked() const;
    void setLocked(bool value);

    QSize fixedSize() const;
    void setFixedSize(const QSize& value);

    virtual int logicalId() const override;
    virtual void setLogicalId(int value) override;

    /** Check if layout is an exported file. */
    virtual bool isFile() const;

    /** Check if layout is shared. */
    bool isShared() const;

    /**
     * Auto-generated service layout. Such layouts are used for:
     * * videowalls (videowall as a parent)
     * * videowall reviews (videowall as a parent)
     */
    bool isServiceLayout() const;

signals:
    void itemAdded(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemRemoved(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);
    void itemChanged(const QnLayoutResourcePtr &resource, const QnLayoutItemData &item);

    void cellAspectRatioChanged(const QnLayoutResourcePtr &resource);
    void cellSpacingChanged(const QnLayoutResourcePtr &resource);
    void storeRequested(const QnLayoutResourcePtr &resource);

    void fixedSizeChanged(const QnLayoutResourcePtr& resource);
    void backgroundSizeChanged(const QnLayoutResourcePtr &resource);
    void backgroundImageChanged(const QnLayoutResourcePtr &resource);
    void backgroundOpacityChanged(const QnLayoutResourcePtr &resource);
    void lockedChanged(const QnLayoutResourcePtr &resource);

    void dataChanged(int role);

protected:
    virtual Qn::Notifier storedItemAdded(const QnLayoutItemData& item) override;
    virtual Qn::Notifier storedItemRemoved(const QnLayoutItemData& item) override;
    virtual Qn::Notifier storedItemChanged(
        const QnLayoutItemData& item,
        const QnLayoutItemData& oldItem) override;

    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    QScopedPointer<QnThreadsafeItemStorage<QnLayoutItemData> > m_items;
    float m_cellAspectRatio = 0; //< Means 'auto'.
    qreal m_cellSpacing = nx::vms::api::LayoutData::kDefaultCellSpacing;
    QHash<int, QVariant> m_dataByRole; //< TODO: Client-only variable, must be removed.
    QnTimePeriod m_localRange;
    QSize m_fixedSize;
    int m_logicalId = 0;
    QSize m_backgroundSize;
    QString m_backgroundImageFilename;
    qreal m_backgroundOpacity = nx::vms::api::LayoutData::kDefaultBackgroundOpacity;
    bool m_locked = false;
};

Q_DECLARE_METATYPE(QnLayoutResourcePtr);
Q_DECLARE_METATYPE(QnLayoutResourceList);
