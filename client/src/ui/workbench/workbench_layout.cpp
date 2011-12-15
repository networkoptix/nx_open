#include "workbench_layout.h"
#include <utils/common/warnings.h>
#include "workbench_item.h"
#include "grid_walker.h"

QnWorkbenchLayout::QnWorkbenchLayout(QObject *parent):
    QObject(parent)
{}

QnWorkbenchLayout::~QnWorkbenchLayout() {
    clear();

    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

void QnWorkbenchLayout::clear() {
    while(!m_items.empty()) {
        QnWorkbenchItem *item = *m_items.begin();

        removeItem(item);
        delete item;
    }
}

bool QnWorkbenchLayout::empty() const {
    return m_items.empty();
}

void QnWorkbenchLayout::addItem(QnWorkbenchItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    if(item->layout() != NULL)
        item->layout()->removeItem(item);

    if(item->isPinned() && m_itemMap.isOccupied(item->geometry())) 
        item->setFlag(QnWorkbenchItem::Pinned, false);

    item->m_layout = this;
    m_items.insert(item);
    
    if(item->isPinned())
        m_itemMap.fill(item->geometry(), item);
    m_rectSet.insert(item->geometry());
    m_itemsByUid[item->resourceUniqueId()].insert(item);

    updateBoundingRectInternal();

    emit itemAdded(item);
}

void QnWorkbenchLayout::removeItem(QnWorkbenchItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    if(item->layout() != this) {
        qnWarning("Cannot remove an item that belongs to a different layout.");
        return;
    }

    if(item->isPinned())
        m_itemMap.clear(item->geometry());
    m_rectSet.remove(item->geometry());
    m_itemsByUid[item->resourceUniqueId()].remove(item);

    item->m_layout = NULL;
    m_items.remove(item);

    updateBoundingRectInternal();

    emit itemRemoved(item);
}

bool QnWorkbenchLayout::moveItem(QnWorkbenchItem *item, const QRect &geometry) {
    if(item->layout() != this) {
        qnWarning("Cannot move an item that does not belong to this layout.");
        return false;
    }

    if (item->isPinned() && !m_itemMap.isOccupiedBy(geometry, item, true))
        return false;

    if(item->isPinned()) {
        m_itemMap.clear(item->geometry());
        m_itemMap.fill(geometry, item);
    }

    moveItemInternal(item, geometry);

    return true;
}

void QnWorkbenchLayout::moveItemInternal(QnWorkbenchItem *item, const QRect &geometry) {
    m_rectSet.remove(item->geometry());
    m_rectSet.insert(geometry);

    updateBoundingRectInternal();

    item->setGeometryInternal(geometry);
}

bool QnWorkbenchLayout::moveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries) {
    if (items.size() != geometries.size()) {
        qnWarning("Sizes of the given containers do not match.");
        return false;
    }

    if (items.empty())
        return true;

    /* Check whether it's our items. */
    foreach (QnWorkbenchItem *item, items) {
        if (item->layout() != this) {
            qnWarning("One of the given items does not belong to this layout.");
            return false;
        }
    }

    /* Check whether new positions do not intersect each other. */
    QSet<QPoint> pointSet;
    for(int i = 0; i < items.size(); i++) {
        if(!items[i]->isPinned())
            continue;

        const QRect &geometry = geometries[i];
        for (int r = geometry.top(); r <= geometry.bottom(); r++) {
            for (int c = geometry.left(); c <= geometry.right(); c++) {
                QPoint point(c, r);

                if (pointSet.contains(point))
                    return false;

                pointSet.insert(point);
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    QSet<QnWorkbenchItem *> replacedItems;
    for(int i = 0; i < items.size(); i++) {
        if(!items[i]->isPinned())
            continue;

        m_itemMap.values(geometries[i], &replacedItems);
    }
    foreach (QnWorkbenchItem *item, items)
        replacedItems.remove(item);
    if (!replacedItems.empty())
        return false;

    /* Move. */
    foreach (QnWorkbenchItem *item, items)
        if(item->isPinned())
            m_itemMap.clear(item->geometry());
    for (int i = 0; i < items.size(); i++) {
        QnWorkbenchItem *item = items[i];

        if(item->isPinned())
            m_itemMap.fill(geometries[i], item);
        moveItemInternal(item, geometries[i]);
    }

    return true;
}

bool QnWorkbenchLayout::pinItem(QnWorkbenchItem *item, const QRect &geometry) {
    if(item->layout() != this) {
        qnWarning("Cannot pin an item that does not belong to this layout");
        return false;
    }

    if(item->isPinned())
        return moveItem(item, geometry);

    if(m_itemMap.isOccupied(geometry))
        return false;

    m_itemMap.fill(geometry, item);
    moveItemInternal(item, geometry);
    item->setFlagInternal(QnWorkbenchItem::Pinned, true);
    return true;
}

bool QnWorkbenchLayout::unpinItem(QnWorkbenchItem *item) {
    if(item->layout() != this) {
        qnWarning("Cannot unpin an item that does not belong to this layout");
        return false;
    }

    if(!item->isPinned())
        return true;

    m_itemMap.clear(item->geometry());
    item->setFlagInternal(QnWorkbenchItem::Pinned, false);
    return true;
}

QnWorkbenchItem *QnWorkbenchLayout::item(const QPoint &position) const {
    return m_itemMap.value(position, NULL);
}

QSet<QnWorkbenchItem *> QnWorkbenchLayout::items(const QRect &region) const {
    return m_itemMap.values(region);
}

QSet<QnWorkbenchItem *> QnWorkbenchLayout::items(const QList<QRect> &regions) const {
    return m_itemMap.values(regions);
}

const QSet<QnWorkbenchItem *> &QnWorkbenchLayout::items(const QString &resourceUniqueId) const {
    QHash<QString, QSet<QnWorkbenchItem *> >::const_iterator pos = m_itemsByUid.find(resourceUniqueId);

    return pos == m_itemsByUid.end() ? m_noItems : pos.value();
}

QRect QnWorkbenchLayout::closestFreeSlot(const QPoint &pos, const QSize &size, QnGridWalker *walker) const {
    if(walker == NULL) {
        QnAspectRatioGridWalker aspectRatioWalker(1.0);
        return closestFreeSlot(pos, size, &aspectRatioWalker);
    }

    while(true) {
        QRect rect = QRect(pos + walker->next(), size);
        if(!m_itemMap.isOccupied(rect))
            return rect;
    }
}

void QnWorkbenchLayout::updateBoundingRectInternal() {
    QRect boundingRect = m_rectSet.boundingRect();
    if(m_boundingRect == boundingRect)
        return;

    m_boundingRect = boundingRect;
    emit boundingRectChanged();
}

