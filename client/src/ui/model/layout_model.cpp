#include "layout_model.h"
#include <utils/common/warnings.h>
#include "resource_item_model.h"

QnLayoutModel::QnLayoutModel(QObject *parent):
    QObject(parent)
{}


QnLayoutModel::~QnLayoutModel() {
    clear();

    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
}

void QnLayoutModel::clear() {
    while(!m_items.empty()) {
        QnLayoutItemModel *item = *m_items.begin();

        removeItem(item);
        delete item;
    }
}

void QnLayoutModel::addItem(QnLayoutItemModel *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    if(item->layout() != NULL)
        item->layout()->removeItem(item);

    if(item->isPinned() && m_itemMap.isOccupied(item->geometry())) 
        item->setFlag(QnLayoutItemModel::Pinned, false);

    item->m_layout = this;
    m_items.insert(item);
    if(item->isPinned())
        m_itemMap.fill(item->geometry(), item);
    m_rectSet.insert(item->geometry());

    emit itemAdded(item);
}

void QnLayoutModel::removeItem(QnLayoutItemModel *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    if(item->layout() != this) {
        qnWarning("Cannot remove an item that belongs to a different layout.");
        return;
    }

    emit itemAboutToBeRemoved(item);

    if(item->isPinned())
        m_itemMap.clear(item->geometry());
    m_rectSet.remove(item->geometry());
    item->m_layout = NULL;
    m_items.remove(item);

    delete item;
}

bool QnLayoutModel::moveItem(QnLayoutItemModel *item, const QRect &geometry) {
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

void QnLayoutModel::moveItemInternal(QnLayoutItemModel *item, const QRect &geometry) {
    m_rectSet.remove(item->geometry());
    m_rectSet.insert(geometry);
    item->setGeometryInternal(geometry);
}

bool QnLayoutModel::moveItems(const QList<QnLayoutItemModel *> &items, const QList<QRect> &geometries) {
    if (items.size() != geometries.size()) {
        qnWarning("Sizes of the given containers do not match.");
        return false;
    }

    if (items.empty())
        return true;

    /* Check whether it's our items. */
    foreach (QnLayoutItemModel *item, items) {
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
    QSet<QnLayoutItemModel *> replacedItems;
    for(int i = 0; i < items.size(); i++) {
        if(!items[i]->isPinned())
            continue;

        m_itemMap.values(geometries[i], &replacedItems);
    }
    foreach (QnLayoutItemModel *item, items)
        replacedItems.remove(item);
    if (!replacedItems.empty())
        return false;

    /* Move. */
    foreach (QnLayoutItemModel *item, items)
        if(item->isPinned())
            m_itemMap.clear(item->geometry());
    for (int i = 0; i < items.size(); i++) {
        QnLayoutItemModel *item = items[i];

        if(item->isPinned())
            m_itemMap.fill(geometries[i], item);
        moveItemInternal(item, geometries[i]);
    }

    return true;
}

bool QnLayoutModel::pinItem(QnLayoutItemModel *item, const QRect &geometry) {
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
    item->setFlagInternal(QnLayoutItemModel::Pinned, true);
    return true;
}

bool QnLayoutModel::unpinItem(QnLayoutItemModel *item) {
    if(item->layout() != this) {
        qnWarning("Cannot unpin an item that does not belong to this layout");
        return false;
    }

    if(!item->isPinned())
        return true;

    m_itemMap.clear(item->geometry());
    item->setFlagInternal(QnLayoutItemModel::Pinned, false);
    return true;
}

QnLayoutItemModel *QnLayoutModel::item(const QPoint &position) const {
    return m_itemMap.value(position, NULL);
}

QSet<QnLayoutItemModel *> QnLayoutModel::items(const QRect &region) const {
    return m_itemMap.values(region);
}

QSet<QnLayoutItemModel *> QnLayoutModel::items(const QList<QRect> &regions) const {
    return m_itemMap.values(regions);
}

namespace {
    bool isFree(const QnMatrixMap<QnLayoutItemModel *> &map, const QPoint &pos, const QSize &size, int dr, int dc, QRect *result) {
        QRect rect = QRect(pos + QPoint(dc, dr), size);

        if(!map.isOccupied(rect)) {
            *result = rect;
            return true;
        } else {
            return false;
        }
    }
}

QRect QnLayoutModel::closestFreeSlot(const QPoint &pos, const QSize &size) const {
    QRect result;

    for(int l = 0; ; l++) {
        int dr, dc;

        /* Iterate in the following order:
         *  213
         *  2.3
         *  444
         */

        /* 1. */
        dr = -l;
        for(dc = -l + 1; dc <= l - 1; dc++)
            if(isFree(m_itemMap, pos, size, dr, dc, &result))
                return result;

        /* 2. */
        dc = -l;
        for(dr = -l; dr <= l - 1; dr++)
            if(isFree(m_itemMap, pos, size, dr, dc, &result))
                return result;

        /* 3. */
        dc = l;
        for(dr = -l; dr <= l - 1; dr++)
            if(isFree(m_itemMap, pos, size, dr, dc, &result))
                return result;

        /* 4. */
        dr = l;
        for(dc = -l; dc <= l; dc++)
            if(isFree(m_itemMap, pos, size, dr, dc, &result))
                return result;
    }
}