#include "workbench_layout.h"
#include <limits>
#include <core/resourcemanagment/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <utils/common/warnings.h>
#include <utils/common/range.h>
#include <ui/common/scene_utility.h>
#include "workbench_item.h"
#include "workbench_grid_walker.h"

namespace {
    template<class PointContainer>
    void pointize(const QRect &region, PointContainer *points) {
        if(points == NULL)
            return;

        for (int r = region.top(); r <= region.bottom(); r++)
            for (int c = region.left(); c <= region.right(); c++)
                qnInsert(*points, points->end(), QPoint(c, r));
    }

    class DistanceMagnitudeCalculator: public TypedMagnitudeCalculator<QPoint> {
    public:
        DistanceMagnitudeCalculator(const QPointF &origin):
            m_origin(origin),
            m_calculator(MagnitudeCalculator::forType<QPointF>())
        {}

    protected:
        virtual qreal calculateInternal(const void *value) const override {
            const QPoint &p = *static_cast<const QPoint *>(value);

            return m_calculator->calculate(p - m_origin);
        }

    private:
        QPointF m_origin;
        TypedMagnitudeCalculator<QPointF> *m_calculator;
    };

    struct WorkbenchItemLess {
    public:
        bool operator()(QnWorkbenchItem *l, QnWorkbenchItem *r) const {
            if(l->resourceUid() == r->resourceUid()) {
                return l < r;
            } else {
                return l->resourceUid() < r->resourceUid();
            }
        }
    };

} // anonymous namespace

QnWorkbenchLayout::QnWorkbenchLayout(QObject *parent)
    : QObject(parent)
{
    connect(qnResPool, SIGNAL(resourceRemoved(QnResourcePtr)), this, SLOT(at_resourcePool_resourceRemoved(QnResourcePtr)));
}

QnWorkbenchLayout::~QnWorkbenchLayout()
{
    clear();

    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);
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

bool QnWorkbenchLayout::load(const QnLayoutResource &layoutData) {
    setName(layoutData.getName());

    bool result = true;

    /* Unpin all items so that pinned state does not interfere with 
     * incrementally moving the items. */
    foreach(QnWorkbenchItem *item, m_items)
        item->setPinned(false);

    foreach(const QnLayoutItemData itemData, layoutData.getItems()) {
        QnId id = itemData.resourceId;
        QnResourcePtr resource = qnResPool->getResourceById(id);
        if(resource.isNull()) {
            qnWarning("No resource in resource pool for id '%1'.", id.toString());
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
    if(items().size() > layoutData.getItems().size()) {
        QSet<QUuid> removed;

        foreach(QnWorkbenchItem *item, items())
            removed.insert(item->uuid());

        foreach(const QnLayoutItemData &itemData, layoutData.getItems())
            removed.remove(itemData.uuid);

        foreach(const QUuid &uuid, removed)
            delete item(uuid);
    }

    return result;
}

void QnWorkbenchLayout::save(QnLayoutResource &layoutData) const {
    layoutData.setName(name());

    QnLayoutItemDataList itemDatas;
    itemDatas.reserve(items().size());

    foreach(const QnWorkbenchItem *item, items()) {
        QnLayoutItemData itemData;
        
        QnResourcePtr resource = qnResPool->getResourceByUniqId(item->resourceUid());
        if(resource.isNull()) {
            qnWarning("No resource in resource pool for uid '%1'.", item->resourceUid());
            continue;
        }

        itemData.resourceId = resource->getId();
        itemData.uuid = item->uuid();
        item->save(itemData);

        itemDatas.push_back(itemData);
    }

    layoutData.setItems(itemDatas);
}

void QnWorkbenchLayout::addItem(QnWorkbenchItem *item) {
    if(item == NULL) {
        qnNullWarning(item);
        return;
    }

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

void QnWorkbenchLayout::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    foreach(QnWorkbenchItem *item, items(resource->getUniqueId()))
        delete item;
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
        DistanceMagnitudeCalculator metric(gridPos - SceneUtility::toPoint(QSizeF(size)) / 2.0);
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

