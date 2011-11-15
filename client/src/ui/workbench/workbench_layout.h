#ifndef QN_WORKBENCH_LAYOUT_H
#define QN_WORKBENCH_LAYOUT_H

#include <QObject>
#include <QSet>
#include <utils/common/matrix_map.h>
#include <utils/common/rect_set.h>

class QnWorkbenchItem;

class QnWorkbenchLayout: public QObject {
    Q_OBJECT;
public:
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
     * \param item                      Item to move to a new position.
     * \param geometry                  New position.
     * \returns                         Whether the item was moved.
     */
    bool moveItem(QnWorkbenchItem *item, const QRect &geometry);

    /**
     * \param items                     Items to move to new positions.
     * \param geometries                New positions.
     * \returns                         Whether the items were moved.
     */
    bool moveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries);

    /**
     * \param item                      Item to pin.
     * \param geometry                  Position to pin to.
     * \returns                         Whether the item was pinned.
     */
    bool pinItem(QnWorkbenchItem *item, const QRect &geometry);

    /**
     * \param item                      Item to unpin.
     * \returns                         Whether the item was unpinned.
     */
    bool unpinItem(QnWorkbenchItem *item);

    /**
     * \param position                  Position to get item at.
     * \returns                         Item at the given position, or NULL if the given position is empty.
     */
    QnWorkbenchItem *item(const QPoint &position) const;

    /**
     * \param region                    Region to get items at.
     * \returns                         Set of items at the given region.
     */
    QSet<QnWorkbenchItem *> items(const QRect &region) const;

    /**
     * \param regions                   Regions to get items at.
     * \returns                         Set of items at the given regions.
     */
    QSet<QnWorkbenchItem *> items(const QList<QRect> &regions) const;

    /**
     * \returns                         All items of this model.
     */
    const QSet<QnWorkbenchItem *> &items() const {
        return m_items;
    }

    /**
     * Clears this layout by removing all its items.
     */
    void clear();

    /**
     * \returns                         Bounding rectangle of all pinned items in this layout.
     */
    QRect boundingRect() const {
        return m_rectSet.boundingRect();
    }

    /**
     * \param pos                       Desired position.
     * \param size                      Desired slot size.
     * \returns                         Geometry of the free slot of desired size whose upper-left corner 
     *                                  is closest (in L_inf sense) to the given position.
     */
    QRect closestFreeSlot(const QPoint &pos, const QSize &size) const;

signals:
    void aboutToBeDestroyed();
    void itemAdded(QnWorkbenchItem *item);
    void itemAboutToBeRemoved(QnWorkbenchItem *item);

private:
    void moveItemInternal(QnWorkbenchItem *item, const QRect &geometry);

private:
    QSet<QnWorkbenchItem *> m_items;
    QnMatrixMap<QnWorkbenchItem *> m_itemMap;
    QnRectSet m_rectSet;
};

#endif // QN_WORKBENCH_LAYOUT_H
