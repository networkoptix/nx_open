#include "workbench_layout.h"
#include <limits>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <utils/common/warnings.h>
#include <utils/common/range.h>
#include <ui/common/scene_utility.h>
#include <ui/style/globals.h>
#include "workbench_item.h"
#include "workbench_grid_walker.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_utility.h"

namespace {
    template<class PointContainer>
    void pointize(const QRect &region, PointContainer *points) {
        if(points == NULL)
            return;

        for (int r = region.top(); r <= region.bottom(); r++)
            for (int c = region.left(); c <= region.right(); c++)
                qnInsert(*points, points->end(), QPoint(c, r));
    }

} // anonymous namespace

QnWorkbenchLayout::QnWorkbenchLayout(QObject *parent): 
    QObject(parent)
{
    initCellParameters();
}

QnWorkbenchLayout::QnWorkbenchLayout(const QnLayoutResourcePtr &resource, QObject *parent):
    QObject(parent)
{
    if(resource.isNull()) {
        qnNullWarning(resource);
        return;
    }

    initCellParameters();

    QnWorkbenchLayoutSynchronizer *synchronizer = new QnWorkbenchLayoutSynchronizer(this, resource, this);
    synchronizer->setAutoDeleting(true);
    synchronizer->update();
}
    
QnWorkbenchLayout::~QnWorkbenchLayout() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    clear();
}

QnLayoutResourcePtr QnWorkbenchLayout::resource() const {
    QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(const_cast<QnWorkbenchLayout *>(this));
    if(synchronizer == NULL)
        return QnLayoutResourcePtr();

    return synchronizer->resource();
}

QnWorkbenchLayout *QnWorkbenchLayout::layout(const QnLayoutResourcePtr &resource) {
    QnWorkbenchLayoutSynchronizer *synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource);
    if(synchronizer == NULL)
        return NULL;

    return synchronizer->layout();
}

const QString &QnWorkbenchLayout::name() const {
    return m_name;
}

void QnWorkbenchLayout::setName(const QString &name) {
    if(m_name == name)
        return;

    m_name = name;

    emit nameChanged();
}

bool QnWorkbenchLayout::load(const QnLayoutResourcePtr &resource) {
    setName(resource->getName());
    setCellAspectRatio(resource->cellAspectRatio());
    setCellSpacing(resource->cellSpacing());

    bool result = true;

    /* Unpin all items so that pinned state does not interfere with 
     * incrementally moving the items. */
    foreach(QnWorkbenchItem *item, m_items)
        item->setPinned(false);

    foreach(const QnLayoutItemData itemData, resource->getItems()) {
        QnId id = itemData.resource.id;
        QString path = itemData.resource.path;

        QnResourcePtr resource;
        if (id.isValid())
            resource = qnResPool->getResourceById(id);
        else
            resource = qnResPool->getResourceByUniqId(path);

        if(resource.isNull()) {
            qnWarning("No resource in resource pool for id '%1' or path '%s'.", id.toString(), path);
            result = false;
            continue;
        }

        QnWorkbenchItem *item = this->item(itemData.uuid);
        if(item == NULL) {
            item = new QnWorkbenchItem(resource->getUniqueId(), itemData.uuid);
            addItem(item);
        } else if(item->resourceUid() != resource->getUniqueId()) {
            qnWarning("Resource unique id of an item and corresponding item data do not match (%1 != %2)", item->resourceUid(), resource->getUniqueId());
            result = false;
        }

        result &= item->load(itemData);
    }

    /* Some items may have been removed. */
    if(items().size() > resource->getItems().size()) {
        QSet<QUuid> removed;

        foreach(QnWorkbenchItem *item, items())
            removed.insert(item->uuid());

        foreach(const QnLayoutItemData &itemData, resource->getItems())
            removed.remove(itemData.uuid);

        foreach(const QUuid &uuid, removed)
            delete item(uuid);
    }

    return result;
}

void QnWorkbenchLayout::save(const QnLayoutResourcePtr &resource) const {
    resource->setName(name());
    resource->setCellAspectRatio(cellAspectRatio());
    resource->setCellSpacing(cellSpacing());

    QnLayoutItemDataList resources;
    resources.reserve(items().size());

    foreach(const QnWorkbenchItem *item, items()) {
        QnLayoutItemData itemData;
        
        QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
        if(resource.isNull()) {
            qnWarning("No resource in resource pool for uid '%1'.", item->resourceUid());
            continue;
        }

        itemData.resource.id = resource->getId();
        itemData.resource.path = resource->getUrl();
        itemData.uuid = item->uuid();
        item->save(itemData);

        resources.push_back(itemData);
    }

    resource->setItems(resources);
}

void QnWorkbenchLayout::addItem(QnWorkbenchItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

    if(m_itemByUuid.contains(item->uuid()))
        qnWarning("Item with UUID '%1' is already on layout '%2'.", item->uuid().toString(), m_name);

    if (item->layout() != NULL) {
        if (item->layout() == this)
            return;
        item->layout()->removeItem(item);
    }

    if(item->isPinned() && m_itemMap.isOccupied(item->geometry()))
        item->setFlag(QnWorkbenchItem::Pinned, false);

    item->m_layout = this;
    m_items.insert(item);

    if(item->isPinned())
        m_itemMap.fill(item->geometry(), item);
    m_rectSet.insert(item->geometry());
    m_itemsByUid[item->resourceUid()].insert(item);
    m_itemByUuid[item->uuid()] = item;

    emit itemAdded(item);

    updateBoundingRectInternal();
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
    m_itemsByUid[item->resourceUid()].remove(item);
    m_itemByUuid.remove(item->uuid());

    item->m_layout = NULL;
    m_items.remove(item);

    emit itemRemoved(item);

    updateBoundingRectInternal();
}

void QnWorkbenchLayout::clear()
{
    foreach (QnWorkbenchItem *item, m_items)
		delete item;
    m_items.clear();
}

bool QnWorkbenchLayout::canMoveItem(QnWorkbenchItem *item, const QRect &geometry, Disposition *disposition) {
    if(item->layout() != this) {
        qnWarning("Cannot move an item that does not belong to this layout.");
        return false;
    }

    if(item->isPinned()) {
        return m_itemMap.isOccupiedBy(
            geometry,
            item,
            disposition == NULL ? NULL : &disposition->free,
            disposition == NULL ? NULL : &disposition->occupied
        );
    } else {
        pointize(geometry, disposition == NULL ? NULL : &disposition->free);
        return true;
    }
}

bool QnWorkbenchLayout::moveItem(QnWorkbenchItem *item, const QRect &geometry) {
    if(!canMoveItem(item, geometry))
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

bool QnWorkbenchLayout::canMoveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries, Disposition *disposition) {
    if(disposition == NULL) {
        return canMoveItems<true>(items, geometries, NULL);
    } else {
        return canMoveItems<false>(items, geometries, disposition);
    }
}

template<bool returnEarly>
bool QnWorkbenchLayout::canMoveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries, Disposition *disposition) {
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

    /* Good points are those where items can be moved.
     * Bad points are those where they cannot be moved. */
    QSet<QPoint> goodPointSet, badPointSet;

    /* Check whether new positions do not intersect each other. */
    for(int i = 0; i < items.size(); i++) {
        QnWorkbenchItem *item = items[i];
        if(!item->isPinned())
            continue;

        const QRect &geometry = geometries[i];
        for (int r = geometry.top(); r <= geometry.bottom(); r++) {
            for (int c = geometry.left(); c <= geometry.right(); c++) {
                QPoint point(c, r);

                bool conforms = !goodPointSet.contains(point);
                if(conforms) {
                    goodPointSet.insert(point);
                } else {
                    if(returnEarly)
                        return false;
                    badPointSet.insert(point);
                }
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    QSet<QnWorkbenchItem *> itemSet = items.toSet();
    for(int i = 0; i < items.size(); i++) {
        QnWorkbenchItem *item = items[i];
        if(!item->isPinned()) {
            pointize(geometries[i], &goodPointSet);
            continue;
        }

        bool conforms = m_itemMap.isOccupiedBy(
            geometries[i],
            itemSet,
            returnEarly ? NULL : &goodPointSet,
            returnEarly ? NULL : &badPointSet
        );

        Q_UNUSED(conforms); /* It is not used if we're not returning early. */
        if(returnEarly && !conforms)
            return false;
    }

    if(returnEarly) {
        return true; /* If we got here with early return on, then it means that everything is OK. */
    } else {
        goodPointSet.subtract(badPointSet);

        disposition->free = goodPointSet;
        disposition->occupied = badPointSet;

        return badPointSet.empty();
    }
}

bool QnWorkbenchLayout::moveItems(const QList<QnWorkbenchItem *> &items, const QList<QRect> &geometries) {
    if(!canMoveItems(items, geometries, NULL))
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

QnWorkbenchItem *QnWorkbenchLayout::item(const QUuid &uuid) const {
    return m_itemByUuid.value(uuid, NULL);
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

QRect QnWorkbenchLayout::closestFreeSlot(const QPointF &gridPos, const QSize &size, TypedMagnitudeCalculator<QPoint> *metric) const {
    if(metric == NULL) {
        QnDistanceMagnitudeCalculator metric(gridPos - SceneUtility::toPoint(QSizeF(size)) / 2.0);
        return closestFreeSlot(gridPos, size, &metric); /* Use default metric if none provided. */
    }

    /* Grid cell where starting search position lies. */
    QPoint gridCell = (gridPos - SceneUtility::toPoint(QSizeF(size)) / 2.0).toPoint();

    /* Current bests. */
    qreal bestDistance = std::numeric_limits<qreal>::max();
    QPoint bestDelta = QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());

    /* Border being walked. */
    QnWorkbenchGridWalker::Border checkedBorder = QnWorkbenchGridWalker::NoBorders;

    /* Borders that are known not to contain positions closer to the target than current best. */
    QnWorkbenchGridWalker::Borders checkedBorders = 0;

    QnWorkbenchGridWalker walker;
    while(true) {
        if(walker.hasNext()) {
            QPoint delta = walker.next();

            qreal distance = metric->calculate(gridCell + delta);
            if(distance > bestDistance || qFuzzyCompare(distance, bestDistance))
                continue;
            checkedBorder = QnWorkbenchGridWalker::NoBorders;

            if(m_itemMap.isOccupied(QRect(gridCell + delta, size)))
                continue;

            checkedBorders = 0;
            bestDistance = distance;
            bestDelta = delta;
        } else {
            checkedBorders |= checkedBorder;
            if(checkedBorders == QnWorkbenchGridWalker::AllBorders && bestDistance < std::numeric_limits<qreal>::max())
                return QRect(gridCell + bestDelta, size);

            struct {
                QnWorkbenchGridWalker::Border border;
                QPoint delta;
            } expansion[4] = {
                {QnWorkbenchGridWalker::RightBorder,   QPoint(walker.rect().right() + 1,   gridCell.y())},
                {QnWorkbenchGridWalker::LeftBorder,    QPoint(walker.rect().left() - 1,    gridCell.y())},
                {QnWorkbenchGridWalker::BottomBorder,  QPoint(gridCell.x(),                walker.rect().bottom() + 1)},
                {QnWorkbenchGridWalker::TopBorder,     QPoint(gridCell.x(),                walker.rect().top() - 1)},
            };

            QnWorkbenchGridWalker::Border bestBorder = QnWorkbenchGridWalker::NoBorders;
            qreal bestBorderDistance = std::numeric_limits<qreal>::max();
            for(int i = 0; i < 4; i++) {
                qreal distance = metric->calculate(gridCell + expansion[i].delta);
                if(distance < bestBorderDistance) {
                    bestBorderDistance = distance;
                    bestBorder = expansion[i].border;
                }
            }

            walker.expand(bestBorder);
            checkedBorder = bestBorder;
        }
    }
}

void QnWorkbenchLayout::updateBoundingRectInternal() {
    QRect boundingRect = m_rectSet.boundingRect();
    if(m_boundingRect == boundingRect)
        return;

    m_boundingRect = boundingRect;
    emit boundingRectChanged();
}

void QnWorkbenchLayout::setCellAspectRatio(qreal cellAspectRatio) {
    if(cellAspectRatio < 0.0) /* Negative means 'use default value'. */
        cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio(); 

    if(qFuzzyCompare(m_cellAspectRatio, cellAspectRatio))
        return;

    m_cellAspectRatio = cellAspectRatio;
    
    emit cellAspectRatioChanged();
}

void QnWorkbenchLayout::setCellSpacing(const QSizeF &cellSpacing) {
    if(cellSpacing.width() < 0.0 || cellSpacing.height() < 0.0) { /* Negative means 'use default value'. */
        setCellSpacing(qnGlobals->defaultLayoutCellSpacing());
        return;
    }

    if(qFuzzyCompare(m_cellSpacing, cellSpacing))
        return;

    m_cellSpacing = cellSpacing;
    
    emit cellSpacingChanged();
}

void QnWorkbenchLayout::initCellParameters() {
    m_cellAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();
    m_cellSpacing = qnGlobals->defaultLayoutCellSpacing();
}

