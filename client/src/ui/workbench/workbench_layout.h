#ifndef QN_WORKBENCH_LAYOUT_H
#define QN_WORKBENCH_LAYOUT_H

#include <QObject>
#include <QSet>
#include <QHash>
#include <core/resource/resource.h>
#include <utils/common/matrix_map.h>
#include <utils/common/rect_set.h>
#include <ui/common/magnitude.h>
#include "core/resource/layout_data.h"

class QnWorkbenchItem;

/**
 * Layout of a workbench.
 *
 * Contains workbench items and information on their positions. Provides the
 * necessary functions for moving items around.
 */
class QnWorkbenchLayout: public QObject {
    Q_OBJECT
public:
    struct Disposition {
        QSet<QPoint> free;
        QSet<QPoint> occupied;
    };

    /**
     * Constructor.
     *
     * \param parent                    Parent object for this ui layout.
     */
    QnWorkbenchLayout(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchLayout();

    /**
      * Load from QnLayoutData
      */
    void load(const QnLayoutData&);

public Q_SLOTS:
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
    void addItem(QnWorkbenchItem *item);

    /**
     * Removes the given item from this layout. Item's ownership is passed
     * to the caller.
     *
     * \param item                      Item to remove
     */
    void removeItem(QnWorkbenchItem *item);

    /**
     * Clears this layout by removing all its items.
     */
    void clear();

public:
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
     * \param resource                  Resource to get item for.
     * \returns                         Item associated with the given resource, or NULL if there is no such item.
     */
    QnWorkbenchItem *item(const QnResourcePtr &resource) const;

    /**
     * \param position                  Position to get item at.
     * \returns                         Pinned item at the given position, or NULL if the given position is empty.
     */
    QnWorkbenchItem *item(const QPoint &position) const;

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

    /**
     * \returns                         Whether there are no items on this layout.
     */
    bool isEmpty() const {
        return m_items.isEmpty();
    }

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

Q_SIGNALS:
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
    void boundingRectChanged();

private Q_SLOTS:
    void resourceRemoved(const QnResourcePtr &resource);

private:
    template<bool returnEarly>
    bool canMoveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries, Disposition *disposition);

    void moveItemInternal(QnWorkbenchItem *item, const QRect &geometry);
    void updateBoundingRectInternal();

private:
    /** Matrix map from coordinate to item. */
    QnMatrixMap<QnWorkbenchItem *> m_itemMap;

    /** Set of all items on this layout. */
    QSet<QnWorkbenchItem *> m_items;

    /** Set of item borders for fast bounding rect calculation. */
    QnRectSet m_rectSet;

    /** Current bounding rectangle. */
    QRect m_boundingRect;

    /** Map from resource unique id to a set of items. */
    QHash<QString, QSet<QnWorkbenchItem *> > m_itemsByUid;

    /** Empty item list, to return a reference to. */
    const QSet<QnWorkbenchItem *> m_noItems;
};

#endif // QN_WORKBENCH_LAYOUT_H
