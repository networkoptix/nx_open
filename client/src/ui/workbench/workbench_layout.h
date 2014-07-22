#ifndef QN_WORKBENCH_LAYOUT_H
#define QN_WORKBENCH_LAYOUT_H

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QHash>

#include <utils/common/matrix_map.h>
#include <utils/common/rect_set.h>
#include <utils/common/hash.h> /* For qHash(const QUuid &). */

#include <core/resource/resource_fwd.h>

#include <utils/math/magnitude.h>

#include <client/client_globals.h>

class QnWorkbenchItem;

// TODO: #Elric review doxydocs
/**
 * Layout of a workbench.
 *
 * Contains workbench items and information on their positions. Provides the
 * necessary functions for moving items around.
 */
class QnWorkbenchLayout: public QObject {
    Q_OBJECT;
public:
    /**
     * Helper struct for obtaining detailed information on move operations.
     */
    struct Disposition {
        /** Set of free slots that the moved items will occupy. */
        QSet<QPoint> free;

        /** Set of slots that are already occupied, thus blocking the items from
         * being moved. */
        QSet<QPoint> occupied;
    };

    /**
     * Constructor.
     *
     * \param parent                    Parent object for this layout.
     */
    QnWorkbenchLayout(QObject *parent = NULL);

    /**
     * Constructor.
     *
     * \param resource                  Layout resource that this layout will
     *                                  be in sync with.
     * \param parent                    Parent object for this layout.
     */
    QnWorkbenchLayout(const QnLayoutResourcePtr &resource, QObject *parent = NULL);

    /**
     * \returns                         Resource associated with this layout, if any.
     */
    QnLayoutResourcePtr resource() const;

    /**
     * \returns                         Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout *instance(const QnLayoutResourcePtr &layout);


    /**
     * \returns                         Layout associated with the given resource, if any.
     */
    static QnWorkbenchLayout *instance(const QnVideoWallResourcePtr &videowall);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchLayout();

    /**
     * \returns                         Name of this layout.
     */
    const QString &name() const;

    /**
     * \param name                      New name for this layout.
     */
    void setName(const QString &name);

    /**
     * \param resource                  Resource to update layout from.
     * \returns                         Whether there were no errors during loading.
     */
    bool update(const QnLayoutResourcePtr &resource);

    /**
     * \param[out] layoutData           Resource to submit layout to.
     */
    void submit(const QnLayoutResourcePtr &resource) const;

    /** 
     * Notify all subscribers that layout title should be updated.
     */
    void notifyTitleChanged();

    /**
     * \param item                      Item to check.
     * \param geometry                  New position.
     * \param[out] disposition          Disposition of free and occupied cells in the target region.
     * \returns                         Whether the item can be moved.
     */
    bool canMoveItem(QnWorkbenchItem *item, const QRect &geometry, Disposition *disposition = NULL);

    /**
     * \param item                      Item to move to a new position.
     * \param geometry                  New position.
     * \param[out] disposition          Disposition of free and occupied cells in the target region.
     * \returns                         Whether the item was moved.
     */
    bool moveItem(QnWorkbenchItem *item, const QRect &geometry);

    /**
     * \param items                     Items to check.
     * \param geometries                New positions.
     * \param[out] disposition          Disposition of free and occupied cells in the target region.
     * \returns                         Whether the items can be moved.
     */
    bool canMoveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries, Disposition *disposition = NULL);

    /**
     * \param items                     Items to move to new positions.
     * \param geometries                New positions.
     * \param[out] disposition          Disposition of free and occupied cells in the target region.
     * \returns                         Whether the items were moved.
     */
    bool moveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries);

    /**
     * \param item                      Item to pin.
     * \param geometry                  Position to pin to.
     * \param[out] disposition          Disposition of free and occupied cells in the target region.
     * \returns                         Whether the item was pinned.
     */
    bool pinItem(QnWorkbenchItem *item, const QRect &geometry);

    /**
     * \param item                      Item to unpin.
     * \returns                         Whether the item was unpinned.
     */
    bool unpinItem(QnWorkbenchItem *item);

    /**
     * Adds the given item to this layout. This layout takes ownership of the
     * given item. If the given item already belongs to some other layout,
     * it will first be removed from that layout.
     *
     * If the position where the item is to be placed is occupied, the item
     * will be placed unpinned.
     *
     * \param item                      Item to add.
     */
    Q_SLOT void addItem(QnWorkbenchItem *item);

    /**
     * Removes the given item from this layout. Item's ownership is passed
     * to the caller.
     *
     * \param item                      Item to remove
     */
    Q_SLOT void removeItem(QnWorkbenchItem *item);

    /**
     * Clears this layout by removing all its items.
     */
    Q_SLOT void clear();

    /**
     * \param position                  Position to get item at.
     * \returns                         Pinned item at the given position, or NULL if the given position is empty.
     */
    QnWorkbenchItem *item(const QPoint &position) const;

    /**
     * \param uuid                      Universally unique identifier to get item for.
     * \returns                         Item for the given universally unique identifier, or NULL if no such item exists in this layout.
     */
    QnWorkbenchItem *item(const QUuid &uuid) const;

    /**
     * \param region                    Region to get pinned items at.
     * \returns                         Set of pinned items at the given region.
     */
    QSet<QnWorkbenchItem *> items(const QRect &region) const;

    /**
     * \param regions                   Regions to get pinned items at.
     * \returns                         Set of pinned items at the given regions.
     */
    QSet<QnWorkbenchItem *> items(const QList<QRect> &regions) const;

    /**
     * \param resourceUniqueId          Resource unique id.
     * \returns                         Set of items that have the given resource unique id.
     */
    const QSet<QnWorkbenchItem *> &items(const QString &resourceUniqueId) const;

    /**
     * \returns                         All items of this model.
     */
    const QSet<QnWorkbenchItem *> &items() const {
        return m_items;
    }

    void addZoomLink(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);

    void removeZoomLink(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);

    QnWorkbenchItem *zoomTargetItem(QnWorkbenchItem *item) const;

    QList<QnWorkbenchItem *> zoomItems(QnWorkbenchItem *zoomTargetItem) const;

    /**
     * \returns                         Whether there are no items on this layout.
     */
    bool isEmpty() const {
        return m_items.isEmpty();
    }

    /**
     * \returns                         Cell aspect ratio of this layout.
     */
    qreal cellAspectRatio() const {
        return m_cellAspectRatio;
    }

    /**
     * \returns                         True if the correct cell aspect ratio (> 0.0) has been set
     */
    bool hasCellAspectRatio() const {
        return m_cellAspectRatio > 0.0;
    }

    /**
     * \param cellAspectRatio           New aspect ratio for cells of this layout.
     */
    void setCellAspectRatio(qreal cellAspectRatio);

    /**
     * \returns                         Spacing between cells of this layout,
     *                                  relative to cell width.
     */
    const QSizeF &cellSpacing() const {
        return m_cellSpacing;
    }

    /**
     * \param cellSpacing               New spacing between cells for this layout,
     *                                  relative to cell width.
     */
    void setCellSpacing(const QSizeF &cellSpacing);

    /**
     * \returns                         Lock state of this layout.
     */
    bool locked() const {
        return m_locked;
    }

    /**
     * \param cellSpacing               New lock state for this layout.
     */
    void setLocked(bool value);

    /**
     * \returns                         Bounding rectangle of all pinned items in this layout.
     */
    const QRect &boundingRect() const {
        return m_boundingRect;
    }

    /**
     * \param gridPos                   Desired position, in grid coordinates.
     * \param size                      Desired slot size.
     * \param metric                    Metric of the gridspace to use for determining the closest slot.
     *                                  Positions of the top-left corner of the slot at hand will be passed to it.
     * \returns                         Geometry of the free slot of desired size whose upper-left corner
     *                                  is closest (as defined by the metric) to the given position.
     */
    QRect closestFreeSlot(const QPointF &gridPos, const QSize &size, TypedMagnitudeCalculator<QPoint> *metric = NULL) const;

    /**
     * \param role                      Role to get data for.
     * \returns                         Data for the given role.
     */
    QVariant data(int role) const;

    /**
     * \param role                      Role to set data for.
     * \param value                     New value for the given data role.
     * \returns                         Whether the data was successfully set.
     */
    bool setData(int role, const QVariant &value);

    QHash<int, QVariant> data() const {
        return m_dataByRole;
    }

    /**
     * @brief centralizeItems           Move all items to the center of the grid coordinates
     *                                  (relative position is not changed).
     */
    void centralizeItems();

signals:
    /**
     * This signal is emitted when this layout is about to be destroyed
     * (i.e. its destructor has started).
     */
    void aboutToBeDestroyed();

    /**
     * This signal is emitted whenever an item is added to this layout.
     * At the time of emit all internal data structures are already updated
     * to include the added item.
     *
     * \param item                      Item that was added.
     */
    void itemAdded(QnWorkbenchItem *item);

    void zoomLinkAdded(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);
    void zoomLinkRemoved(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem);

    /**
     * This signal is emitted whenever an item is about to be removed from
     * this layout. At the time of emit the item is still valid, but
     * all internal data structures are already updated not to include the
     * removed item.
     *
     * \param item                      Item that was removed.
     */
    void itemRemoved(QnWorkbenchItem *item);

    /**
     * This signal is emitted whenever bounding rectangle of this layout changes.
     */
    void boundingRectChanged(const QRect& oldRect, const QRect &newRect);

    /**
     * This signal is emitted whenever name of this layout changes.
     */
    void nameChanged();

    /**
     * This signal is emitted whenever title of this layout changes.
     */
    void titleChanged();

    /**
     * This signal is emitted whenever cell aspect ratio of this layout changes.
     */
    void cellAspectRatioChanged();

    /**
     * This signal is emitted whenever cell spacing of this layout changes.
     */
    void cellSpacingChanged();

    void lockedChanged();

    /**
     * This signal is emitted whenever data associated with the provided role is changed.
     *
     * \param role                      Role of the changed data.
     */
    void dataChanged(int role);

private:
    template<bool returnEarly>
    bool canMoveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries, Disposition *disposition);

    void moveItemInternal(QnWorkbenchItem *item, const QRect &geometry);
    void updateBoundingRectInternal();

    void addZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem, bool notifyItem);
    void removeZoomLinkInternal(QnWorkbenchItem *item, QnWorkbenchItem *zoomTargetItem, bool notifyItem);
    QUuid zoomTargetUuidInternal(QnWorkbenchItem *item) const;

    void initCellParameters();

private:
    /** Name of this layout. */
    QString m_name;

    /** Matrix map from coordinate to item. */
    QnMatrixMap<QnWorkbenchItem *> m_itemMap;

    /** Set of all items on this layout. */
    QSet<QnWorkbenchItem *> m_items;

    /** Map from item to its zoom target. */
    QHash<QnWorkbenchItem *, QnWorkbenchItem *> m_zoomTargetItemByItem;

    /** Map from zoom target item to its associated zoom items. */
    QMultiHash<QnWorkbenchItem *, QnWorkbenchItem *> m_itemsByZoomTargetItem;

    /** Set of item borders for fast bounding rect calculation. */
    QnRectSet m_rectSet;

    /** Current bounding rectangle. */
    QRect m_boundingRect;

    /** Map from resource unique id to a set of items. */
    QHash<QString, QSet<QnWorkbenchItem *> > m_itemsByUid;

    /** Aspect ratio of a single cell. */
    qreal m_cellAspectRatio;

    /** Spacing between cells, relative to cell width. */
    QSizeF m_cellSpacing;

    /** Lock status of the layout. */
    bool m_locked;

    /** Map from item's universally unique identifier to item. */
    QHash<QUuid, QnWorkbenchItem *> m_itemByUuid;

    /** Empty item list, to return a reference to. */
    const QSet<QnWorkbenchItem *> m_noItems;

    /** User data by role. */
    QHash<int, QVariant> m_dataByRole;
};

typedef QList<QnWorkbenchLayout *> QnWorkbenchLayoutList;

Q_DECLARE_METATYPE(QnWorkbenchLayout *);
Q_DECLARE_METATYPE(QnWorkbenchLayoutList);

#endif // QN_WORKBENCH_LAYOUT_H
