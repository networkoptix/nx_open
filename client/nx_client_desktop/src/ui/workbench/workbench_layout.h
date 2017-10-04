#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QHash>

#include <client_core/connection_context_aware.h>

#include <utils/common/matrix_map.h>
#include <utils/common/rect_set.h>

#include <core/resource/resource_fwd.h>

#include <utils/math/magnitude.h>

#include <client/client_globals.h>

#include <nx/utils/uuid.h>

class QnWorkbenchItem;

enum class QnLayoutFlag
{
    Empty = 0x00,
    NoDrop = 0x01,
    FixedViewport = 0x02, //< Disallow to zoom and hand scroll
    NoMove = 0x04,
    NoResize = 0x08, //< Disallow to resize (including raise) items
    NoTimeline = 0x10,
    SpecialBackground = 0x20,
    FillViewport = 0x40, //< Layout must fill viewport as much as possible
};
Q_DECLARE_FLAGS(QnLayoutFlags, QnLayoutFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(QnLayoutFlags)
Q_DECLARE_METATYPE(QnLayoutFlags)

/**
 * Layout of a workbench.
 * Contains workbench items and information on their positions. Provides the
 * necessary functions for moving items around.
 */
class QnWorkbenchLayout: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(QnUuid resourceId READ resourceId CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(float cellAspectRatio
        READ cellAspectRatio WRITE setCellAspectRatio NOTIFY cellAspectRatioChanged)

public:
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
     * Constructor.
     * @param resource Layout resource that this layout will be in sync with.
     * @param parent Parent object for this layout.
     */
    QnWorkbenchLayout(const QnLayoutResourcePtr& resource, QObject* parent = NULL);

    /**
     * @return Resource associated with this layout, if any.
     */
    QnLayoutResourcePtr resource() const;

    /**
     * @return Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout* instance(const QnLayoutResourcePtr& layout);

    // TODO: #GDM reimplement the same way as layout tour reviews are done
    /**
     * @return Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout* instance(const QnVideoWallResourcePtr& videowall);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchLayout();

    QIcon icon() const;

    QnLayoutFlags flags() const;
    void setFlags(QnLayoutFlags value);

    QnUuid resourceId() const;

    /**
     * @return Name of this layout.
     */
    const QString& name() const;

    /**
     * @param name New name for this layout.
     */
    void setName(const QString& name);

    /**
     * @param resource Resource to update layout from.
     * @return Whether there were no errors during loading.
     */
    bool update(const QnLayoutResourcePtr& resource);

    /**
     * @param[out] resource Resource to submit layout to.
     */
    void submit(const QnLayoutResourcePtr& resource) const;

    /**
     * Notify all subscribers that layout title should be updated.
     */
    void notifyTitleChanged();

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
     * Clears this layout by removing all its items.
     */
    Q_SLOT void clear();

    /**
     * @param position Position to get item at.
     * @return Pinned item at the given position, or NULL if the given position is empty.
     */
    QnWorkbenchItem* item(const QPoint& position) const;

    /**
     * @param uuid Universally unique identifier to get item for.
     * @return Item for the given universally unique identifier, or NULL if no such item exists in
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
     * @param cellAspectRatio New aspect ratio for cells of this layout.
     */
    void setCellAspectRatio(float cellAspectRatio);

    static qreal cellSpacingValue(Qn::CellSpacing spacing);

    /**
     * @return Spacing between cells of this layout,
     * relative to cell size.
     */
    qreal cellSpacing() const;

    /**
     * @param cellSpacing New spacing between cells for this layout,
     * relative to cell size.
     */
    void setCellSpacing(qreal spacing);

    /**
     * @return Lock state of this layout.
     */
    bool locked() const;

    /**
     * @param cellSpacing New lock state for this layout.
     */
    void setLocked(bool value);

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
        TypedMagnitudeCalculator<QPoint>* metric = NULL) const;

    /**
     * @param role Role to get data for.
     * @return Data for the given role.
     */
    QVariant data(int role) const;

    /**
     * @param role Role to set data for.
     * @param value New value for the given data role.
     * @return Whether the data was successfully set.
     */
    bool setData(int role, const QVariant& value);

    QHash<int, QVariant> data() const;

    /**
     * Move all items to the center of the grid coordinates (relative position is not changed).
     */
    void centralizeItems();

    /**
     * Check if this layout is preview search layout
     */
    bool isSearchLayout() const;

    /**
     * Check if this layout is a tour review.
     */
    bool isLayoutTourReview() const;

signals:
    void flagsChanged();

    void iconChanged();

    /**
     * Emitted when this layout is about to be destroyed (i.e. its destructor has started).
     */
    void aboutToBeDestroyed();

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
     * Emitted whenever name of this layout changes.
     */
    void nameChanged();

    /**
     * Emitted whenever title of this layout changes.
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

    void lockedChanged();

    /**
     * Emitted whenever data associated with the provided role is changed.
     *
     * @param role Role of the changed data.
     */
    void dataChanged(int role);

private:
    void moveItemInternal(QnWorkbenchItem* item, const QRect& geometry);
    void updateBoundingRectInternal();

    void addZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
        bool notifyItem);

    void removeZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
        bool notifyItem);

    QnUuid zoomTargetUuidInternal(QnWorkbenchItem* item) const;

    void initCellParameters();

    /** Check that item belongs to the current layout. */
    bool own(QnWorkbenchItem* item) const;

private:
    /** Name of this layout. */
    QString m_name;

    /** Matrix map from coordinate to item. */
    QnMatrixMap<QnWorkbenchItem*> m_itemMap;

    /** Set of all items on this layout. */
    QSet<QnWorkbenchItem*> m_items;

    /** Map from item to its zoom target. */
    QHash<QnWorkbenchItem *, QnWorkbenchItem*> m_zoomTargetItemByItem;

    /** Map from zoom target item to its associated zoom items. */
    QMultiHash<QnWorkbenchItem *, QnWorkbenchItem*> m_itemsByZoomTargetItem;

    /** Set of item borders for fast bounding rect calculation. */
    QnRectSet m_rectSet;

    /** Current bounding rectangle. */
    QRect m_boundingRect;

    /** Map from resource to a set of items. */
    QHash<QnResourcePtr, QSet<QnWorkbenchItem*>> m_itemsByResource;

    /** Aspect ratio of a single cell. */
    float m_cellAspectRatio;

    /** Spacing between cells, relative to cell width. */
    qreal m_cellSpacing;

    /** Lock status of the layout. */
    bool m_locked;

    /** Map from item's universally unique identifier to item. */
    QHash<QnUuid, QnWorkbenchItem*> m_itemByUuid;

    /** Empty item list, to return a reference to. */
    const QSet<QnWorkbenchItem*> m_noItems;

    /** User data by role. */
    QHash<int, QVariant> m_dataByRole;

    QnLayoutFlags m_flags = QnLayoutFlag::Empty;
    QIcon m_icon;
};

using QnWorkbenchLayoutList = QList<QnWorkbenchLayout*>;

Q_DECLARE_METATYPE(QnWorkbenchLayout*);
Q_DECLARE_METATYPE(QnWorkbenchLayoutList);
