// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtGui/QIcon>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/client/desktop/workbench/layouts/workbench_layout_state.h>
#include <utils/math/magnitude.h>
#include <utils/rect_set.h>

#include "matrix_map.h"

Q_MOC_INCLUDE("core/resource/layout_resource.h")
Q_MOC_INCLUDE("ui/workbench/workbench_item.h")
Q_MOC_INCLUDE("ui/workbench/workbench_layout_synchronizer.h")

class QnWorkbenchItem;
class QnWorkbenchLayoutSynchronizer;

enum class QnLayoutFlag
{
    Empty = 0,
    FixedViewport = 1 << 0, //< Disallow to zoom and hand scroll
    NoMove = 1 << 1,
    NoResize = 1 << 2, //< Disallow to resize (including raise) items
    NoTimeline = 1 << 3,
    SpecialBackground = 1 << 4,
    FillViewport = 1 << 5, //< Layout must fill viewport as much as possible
};
Q_DECLARE_FLAGS(QnLayoutFlags, QnLayoutFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnLayoutFlags)

/**
 * Layout of a workbench.
 * Contains workbench items and information on their positions. Provides the
 * necessary functions for moving items around.
 */
class QnWorkbenchLayout: public QObject,
    public nx::vms::client::desktop::WindowContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnLayoutResource* resource READ resourcePtr CONSTANT)

public:
    using LayoutResourcePtr = nx::vms::client::desktop::LayoutResourcePtr;
    using StreamSynchronizationState = nx::vms::client::desktop::StreamSynchronizationState;
    static constexpr auto kDefaultCellSpacing = Qn::CellSpacing::Small;

    /**
     * @return Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout* instance(const LayoutResourcePtr& resource);

    // TODO: #sivanov Reimplement the same way as Showreel reviews are implemented.
    /**
     * @return Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout* instance(const QnVideoWallResourcePtr& videowall);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchLayout();

    QnWorkbenchLayoutSynchronizer* layoutSynchronizer() const;

    /**
     * @return Resource associated with this layout. Always exists.
     */
    LayoutResourcePtr resource() const;

    /**
     * @return Plain pointer to the associated resource. Needed by QML.
     */
    QnLayoutResource* resourcePtr() const;

    QIcon icon() const;

    QnLayoutFlags flags() const;
    void setFlags(QnLayoutFlags value);

    QnUuid resourceId() const;

    /**
     * @return Name of this layout.
     */
    QString name() const;

    /**
     * State of cameras synchronization on this layout.
     */
    StreamSynchronizationState streamSynchronizationState() const;

    /**
     * Set cameras syncronization state. It will be used when this layout become current. If it
     * already is, no changes would be applied.
     */
    void setStreamSynchronizationState(StreamSynchronizationState value);

    /**
     * @param resource Resource to update layout from.
     * @return Whether there were no errors during loading.
     */
    bool update(const LayoutResourcePtr& resource);

    /**
     * @param[out] resource Resource to submit layout to.
     */
    void submit(const LayoutResourcePtr& resource) const;

    /**
     * Notify all subscribers that layout title should be updated.
     */
    void notifyTitleChanged();

    /**
     * @returns true if the layout has no specific Cell Aspect Ratio and it can be adjusted.
     */
    bool canAutoAdjustAspectRatio();

    /**
     * Helper struct for obtaining detailed information on move operations.
     */
    struct Disposition
    {
        /** Set of free slots that the moved items will occupy. */
        QSet<QPoint> free;

        /** Set of slots that are already occupied, thus blocking the items from being moved. */
        QSet<QPoint> occupied;
    };

    /**
     * @param item Item to check.
     * @param geometry New position.
     * @param[out] disposition Disposition of free and occupied cells in the target region.
     * @return Whether the item can be moved.
     */
    bool canMoveItem(QnWorkbenchItem* item, const QRect& geometry,
        Disposition* disposition = nullptr);

    /**
     * @param item Item to move to a new position.
     * @param geometry New position.
     * @param[out] disposition Disposition of free and occupied cells in the target region.
     * @return Whether the item was moved.
     */
    bool moveItem(QnWorkbenchItem* item, const QRect& geometry);

    /**
     * @param items Items to check.
     * @param geometries New positions.
     * @param[out] disposition Disposition of free and occupied cells in the target region.
     * @return Whether the items can be moved.
     */
    bool canMoveItems(const QList<QnWorkbenchItem*>& items, const QList<QRect>& geometries,
        Disposition* disposition = nullptr) const;

    /**
     * @param items Items to move to new positions.
     * @param geometries New positions.
     * @return Whether the items were moved.
     */
    bool moveItems(const QList<QnWorkbenchItem*>& items, const QList<QRect>& geometries);

    /**
     * @param item Item to pin.
     * @param geometry Position to pin to.
     * @return Whether the item was pinned.
     */
    bool pinItem(QnWorkbenchItem* item, const QRect& geometry);

    /**
     * @param item Item to unpin.
     * @return Whether the item was unpinned.
     */
    bool unpinItem(QnWorkbenchItem* item);

    /**
     * Adds the given item to this layout. This layout takes ownership of the given item. If the
     * given item already belongs to some other layout, it will first be removed from that layout.
     * If the position where the item is to be placed is occupied, the item will be placed unpinned.
     * @param item Item to add.
     */
    Q_SLOT void addItem(QnWorkbenchItem* item);

    /**
     * Removes the given item from this layout. Item's ownership is passed to the caller.
     * @param item Item to remove
     */
    Q_SLOT void removeItem(QnWorkbenchItem* item);

    /**
     * Remove all items by the given resource.
     */
    void removeItems(const QnResourcePtr& resource);

    /**
     * Clears this layout by removing all its items.
     */
    Q_SLOT void clear();

    /**
     * @param position Position to get item at.
     * @return Pinned item at the given position, or nullptr if the given position is empty.
     */
    QnWorkbenchItem* item(const QPoint& position) const;

    /**
     * @param uuid Universally unique identifier to get item for.
     * @return Item for the given universally unique identifier, or nullptr if no such item exists in
     *     this layout.
     */
    QnWorkbenchItem* item(const QnUuid& uuid) const;

    /**
     * @param region Region to get pinned items at.
     * @return Set of pinned items at the given region.
     */
    QSet<QnWorkbenchItem*> items(const QRect& region) const;

    /**
     * @param regions Regions to get pinned items at.
     * @return Set of pinned items at the given regions.
     */
    QSet<QnWorkbenchItem*> items(const QList<QRect>& regions) const;

    /**
     * @param resource Resource.
     * @return Set of items that have the given resource.
     */
    const QSet<QnWorkbenchItem*>& items(const QnResourcePtr& resource) const;

    /**
     * @return All items of this model.
     */
    const QSet<QnWorkbenchItem*>& items() const;

    /**
     * @return All resources of layout items, without duplicates.
     */
    QnResourceList itemResources() const;

    void addZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);

    void removeZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);

    QnWorkbenchItem* zoomTargetItem(QnWorkbenchItem* item) const;

    QList<QnWorkbenchItem*> zoomItems(QnWorkbenchItem* zoomTargetItem) const;

    /**
     * @return Whether there are no items on this layout.
     */
    bool isEmpty() const;

    /**
     * @return Cell aspect ratio of this layout.
     */
    float cellAspectRatio() const;

    /**
     * @return True if the correct cell aspect ratio (> 0.0) has been set
     */
    bool hasCellAspectRatio() const;

    /**
     * @return Spacing between cells of this layout,
     * relative to cell size.
     */
    qreal cellSpacing() const;

    /**
     * @return Lock state of this layout.
     */
    bool locked() const;

    /**
     * @return Bounding rectangle of all pinned items in this layout.
     */
    const QRect& boundingRect() const;

    /**
     * @param gridPos Desired position, in grid coordinates.
     * @param size Desired slot size.
     * @return True if requested rect is not covered by pinned items.
     */
    bool isFreeSlot(const QPointF& gridPos, const QSize& size) const;

    /**
     * @param gridPos Desired position, in grid coordinates.
     * @param size Desired slot size.
     * @param metric Metric of the gridspace to use for determining the closest slot. Positions of
     *     the top-left corner of the slot at hand will be passed to it.
     * @return Geometry of the free slot of desired size whose upper-left corner is closest (as
     *     defined by the metric) to the given position.
     */
    QRect closestFreeSlot(const QPointF& gridPos, const QSize& size,
        TypedMagnitudeCalculator<QPoint>* metric = nullptr) const;

    /**
     * @param role Role to get data for.
     * @return Data for the given role.
     */
    QVariant data(Qn::ItemDataRole role) const;

    /**
     * @param role Role to set data for.
     * @param value New value for the given data role.
     */
    void setData(Qn::ItemDataRole role, const QVariant& value);

    /**
     * Move all items to the center of the grid coordinates (relative position is not changed).
     */
    void centralizeItems();

    /**
     * Whether this layout is a preview search layout.
     */
    bool isPreviewSearchLayout() const;

    /**
     * Whether this layout is a showreel review layout.
     */
    bool isShowreelReviewLayout() const;

    /** Whether this layout is a Video Wall review layout. */
    bool isVideoWallReviewLayout() const;

    /** Debug string representation. */
    QString toString() const;

signals:
    void flagsChanged();

    void iconChanged();

    /**
     * Emitted whenever an item is added to this layout. At the time of emit all internal data
     * structures are already updated to include the added item.
     * @param item Item that was added.
     */
    void itemAdded(QnWorkbenchItem* item);

    void itemsMoved(QList<QnWorkbenchItem*> items);

    void zoomLinkAdded(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);
    void zoomLinkRemoved(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem);

    /**
     * Emitted whenever an item is about to be removed from this layout. At the time of emit the
     * item is still valid, but all internal data structures are already updated not to include the
     * removed item.
     * @param item Item that was removed.
     */
    void itemRemoved(QnWorkbenchItem* item);

    /**
     * Emitted whenever bounding rectangle of this layout changes.
     */
    void boundingRectChanged(const QRect& oldRect, const QRect& newRect);

    /**
     * Emitted whenever or icon of this layout changes.
     */
    void titleChanged();

    /**
     * Emitted whenever cell aspect ratio of this layout changes.
     */
    void cellAspectRatioChanged();

    /**
     * Emitted whenever cell spacing of this layout changes.
     */
    void cellSpacingChanged();

    /**
     * Emitted whenever data associated with the provided role is changed.
     *
     * @param role Role of the changed data.
     */
    void dataChanged(Qn::ItemDataRole role);

protected:
    /**
     * Constructor.
     * @param resource Layout resource that this layout will be in sync with.
     */
    QnWorkbenchLayout(
        nx::vms::client::desktop::WindowContext* windowContext,
        const LayoutResourcePtr& resource);

private:
    void moveItemInternal(QnWorkbenchItem* item, const QRect& geometry);
    void updateBoundingRectInternal();

    void addZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
        bool notifyItem);

    void removeZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
        bool notifyItem);

    QnUuid zoomTargetUuidInternal(QnWorkbenchItem* item) const;

    /** Check that item belongs to the current layout. */
    bool own(QnWorkbenchItem* item) const;

    /** Calculate what icon should be used for the layout. */
    QIcon calculateIcon() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

using QnWorkbenchLayoutList = QList<QnWorkbenchLayout*>;
