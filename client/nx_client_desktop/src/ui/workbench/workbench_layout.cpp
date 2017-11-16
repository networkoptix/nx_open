#include "workbench_layout.h"

#include <limits>

#include <QtWidgets/QGraphicsWidget>

#include <common/common_module.h>
#include <client_core/client_core_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <utils/common/warnings.h>
#include <ui/style/globals.h>
#include <nx/client/core/utils/geometry.h>
#include <ui/graphics/items/resource/resource_widget.h>

#include "workbench_item.h"
#include "workbench_grid_walker.h"
#include "workbench_layout_synchronizer.h"
#include "workbench_utility.h"

#include "extensions/workbench_stream_synchronizer.h"

#include "utils/common/util.h"

#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>

using nx::client::core::Geometry;

namespace {

template<class PointContainer>
void pointize(const QRect& region, PointContainer* points)
{
    if (!points)
        return;

    for (int r = region.top(); r <= region.bottom(); r++)
    {
        for (int c = region.left(); c <= region.right(); c++)
        {
            QnCollection::insert(*points, points->end(), QPoint(c, r));
        }
    }
}

// TODO: these tags are better to be bound to QnWorkbenchLayout.
static const nx::utils::log::Tag kItemMapTag(lit("__itemMap"));
static const nx::utils::log::Tag kFreeSlotTag(lit("__freeSlot"));

} // namespace

QnWorkbenchLayout::QnWorkbenchLayout(const QnLayoutResourcePtr& resource, QObject* parent):
    QObject(parent)
{

    // TODO: #Elric this does not belong here.
    setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(
        QnStreamSynchronizationState(true, DATETIME_NOW, 1.0)));

    initCellParameters();

    if (resource.isNull())
        return;

    if (resource->data().contains(Qn::LayoutFlagsRole))
        setFlags(flags() | resource->data(Qn::LayoutFlagsRole).value<QnLayoutFlags>());

    m_icon = resource->data(Qn::LayoutIconRole).value<QIcon>();
    auto synchronizer = new QnWorkbenchLayoutSynchronizer(this, resource, this);
    synchronizer->setAutoDeleting(true);
    synchronizer->update();

    connect(this, &QnWorkbenchLayout::dataChanged, this,
        [this](int role)
        {
            if (role == Qn::LayoutIconRole)
                emit iconChanged();
        });
}

QnWorkbenchLayout::~QnWorkbenchLayout()
{
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    clear();
}

QnLayoutFlags QnWorkbenchLayout::flags() const
{
    return m_flags;
}

QIcon QnWorkbenchLayout::icon() const
{
    return m_icon;
}

void QnWorkbenchLayout::setFlags(QnLayoutFlags value)
{
    if (m_flags == value)
        return;

    m_flags = value;
    emit flagsChanged();
}

QnUuid QnWorkbenchLayout::resourceId() const
{
    const auto& layout = resource();
    return layout ? layout->getId() : QnUuid();
}

QnLayoutResourcePtr QnWorkbenchLayout::resource() const
{
    if (auto synchronizer = QnWorkbenchLayoutSynchronizer::instance(const_cast<QnWorkbenchLayout*>(this)))
        return synchronizer->resource();
    return QnLayoutResourcePtr();
}

QnLayoutResource*QnWorkbenchLayout::resourcePtr() const
{
    return resource().data();
}

QnWorkbenchLayout* QnWorkbenchLayout::instance(const QnLayoutResourcePtr& layout)
{
    if (auto synchronizer = QnWorkbenchLayoutSynchronizer::instance(layout))
        return synchronizer->layout();
    return nullptr;
}

QnWorkbenchLayout* QnWorkbenchLayout::instance(const QnVideoWallResourcePtr& videoWall)
{
    auto resourcePool = qnClientCoreModule->commonModule()->resourcePool();
    for (const auto& layout: resourcePool->getResources<QnLayoutResource>())
    {
        if (layout->data().value(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>() == videoWall)
            return QnWorkbenchLayout::instance(layout);
    }
    return nullptr;
}

const QString& QnWorkbenchLayout::name() const
{
    return m_name;
}

void QnWorkbenchLayout::setName(const QString& name)
{
    if (m_name == name)
        return;

    m_name = name;

    emit nameChanged();
    emit dataChanged(Qn::ResourceNameRole);
}

bool QnWorkbenchLayout::update(const QnLayoutResourcePtr& resource)
{
    setName(resource->getName());
    setCellAspectRatio(resource->cellAspectRatio());
    setCellSpacing(resource->cellSpacing());
    setLocked(resource->locked());

    // TODO: #Elric note that we keep items that are not present in resource's data.
    // This is not correct, but we currently need it.
    const QHash<int, QVariant> data = resource->data();
    for (auto i = data.begin(); i != data.end(); i++)
    {
        if (i.key() == Qn::VideoWallItemGuidRole)
            continue;
        setData(i.key(), i.value());
    }

    bool result = true;

    /* Unpin all items so that pinned state does not interfere with
     * incrementally moving the items. */
    for (auto item: m_items)
        item->setPinned(false);

    for (const auto& data: resource->getItems())
    {
        auto item = this->item(data.uuid);
        if (!item)
        {
            if (const auto resource = resourcePool()->getResourceByDescriptor(data.resource))
                addItem(new QnWorkbenchItem(resource, data, this));
        }
        else
        {
            result &= item->update(data);
        }
    }

    /* Some items may have been removed. */
    if (items().size() > resource->getItems().size())
    {
        QSet<QnUuid> removed;

        for (auto item: items())
            removed.insert(item->uuid());

        for (const auto& itemData: resource->getItems())
            removed.remove(itemData.uuid);

        for (const auto& uuid: removed)
            delete item(uuid);
    }

    /* Update zoom targets. */
    for (const auto& data: resource->getItems())
    {
        auto item = this->item(data.uuid);
        auto currentZoomTargetItem = zoomTargetItem(item);
        auto expectedZoomTargetItem = this->item(data.zoomTargetUuid);
        if (currentZoomTargetItem != expectedZoomTargetItem)
            addZoomLink(item, expectedZoomTargetItem); //< Will automatically remove the old link if needed.
    }

    return result;
}

void QnWorkbenchLayout::submit(const QnLayoutResourcePtr& resource) const
{
    resource->setName(name());
    resource->setCellAspectRatio(cellAspectRatio());
    resource->setCellSpacing(cellSpacing());

    QnLayoutItemDataList datas;
    datas.reserve(items().size());
    for (auto item: items())
    {
        QnLayoutItemData data = item->data();
        data.zoomTargetUuid = this->zoomTargetUuidInternal(item);
        datas.push_back(data);
    }

    resource->setItems(datas);
}

void QnWorkbenchLayout::notifyTitleChanged()
{
    emit titleChanged();
}

void QnWorkbenchLayout::addItem(QnWorkbenchItem* item)
{
    NX_EXPECT(item && item->resource());
    if (!item)
        return;

    if (m_itemByUuid.contains(item->uuid()))
        qnWarning("Item with UUID '%1' is already on layout '%2'.", item->uuid().toString(), m_name);

    if (item->layout())
    {
        if (item->layout() == this)
            return;
        item->layout()->removeItem(item);
    }

    if (item->isPinned() && m_itemMap.isOccupied(item->geometry()))
        item->setFlag(Qn::Pinned, false);

    item->m_layout = this;
    m_items.insert(item);

    if (item->isPinned())
    {
        const auto options = item->data<QnResourceWidget::Options>(Qn::ItemWidgetOptions);
        if (!options.testFlag(QnResourceWidget::InvisibleWidgetOption))
        {
            m_itemMap.fill(item->geometry(), item);
            NX_DEBUG(kItemMapTag, lm("Add item to cell %1").arg(item->geometry()));
        }
    }
    m_rectSet.insert(item->geometry());
    m_itemsByResource[item->resource()].insert(item);
    m_itemByUuid[item->uuid()] = item;

    emit itemAdded(item);

    updateBoundingRectInternal();
}

void QnWorkbenchLayout::removeItem(QnWorkbenchItem* item)
{
    if (!own(item))
        return;

    /* Remove all zoom links first. */
    if (auto targetItem = zoomTargetItem(item))
        removeZoomLink(item, targetItem);
    for (auto zoomItem: zoomItems(item))
        removeItem(zoomItem);

    /* Update internal data structures. */
    if (item->isPinned())
    {
        const auto options = item->data<QnResourceWidget::Options>(Qn::ItemWidgetOptions);
        if (!options.testFlag(QnResourceWidget::InvisibleWidgetOption))
        {
            m_itemMap.clear(item->geometry());
            NX_DEBUG(kItemMapTag, lm("Item removed from cell %1").arg(item->geometry()));
        }
    }
    m_rectSet.remove(item->geometry());
    m_itemsByResource[item->resource()].remove(item);
    m_itemByUuid.remove(item->uuid());

    item->m_layout = nullptr;
    m_items.remove(item);

    emit itemRemoved(item);

    updateBoundingRectInternal();
}

void QnWorkbenchLayout::addZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem)
{
    if (!own(item) || !own(zoomTargetItem))
        return;

    NX_EXPECT(item != zoomTargetItem, "Cannot create a loop zoom link.");
    if (item == zoomTargetItem)
        return;

    auto currentZoomTargetItem = item->zoomTargetItem();
    if (currentZoomTargetItem)
    {
        if (currentZoomTargetItem == zoomTargetItem)
            return;

        removeZoomLinkInternal(item, currentZoomTargetItem, false);
    }

    addZoomLinkInternal(item, zoomTargetItem, true);
}

void QnWorkbenchLayout::addZoomLinkInternal(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem,
    bool notifyItem)
{
    m_zoomTargetItemByItem.insert(item, zoomTargetItem);
    m_itemsByZoomTargetItem.insert(zoomTargetItem, item);

    emit zoomLinkAdded(item, zoomTargetItem);
    if (notifyItem)
        emit item->zoomTargetItemChanged();
}

void QnWorkbenchLayout::removeZoomLink(QnWorkbenchItem* item, QnWorkbenchItem* zoomTargetItem)
{
    if (!own(item) || !own(zoomTargetItem))
        return;

    NX_EXPECT(m_zoomTargetItemByItem.value(item) == zoomTargetItem,
        "Cannot remove a zoom link that does not exist in this layout.");

    if (m_zoomTargetItemByItem.value(item) != zoomTargetItem)
        return;

    removeZoomLinkInternal(item, zoomTargetItem, true);
}

void QnWorkbenchLayout::removeZoomLinkInternal(QnWorkbenchItem* item,
    QnWorkbenchItem* zoomTargetItem, bool notifyItem)
{
    m_zoomTargetItemByItem.remove(item);
    m_itemsByZoomTargetItem.remove(zoomTargetItem, item);

    emit zoomLinkRemoved(item, zoomTargetItem);
    if (notifyItem)
        emit item->zoomTargetItemChanged();
}

void QnWorkbenchLayout::clear()
{
    foreach(QnWorkbenchItem *item, m_items)
        delete item;
    m_items.clear();
}

bool QnWorkbenchLayout::canMoveItem(QnWorkbenchItem* item, const QRect& geometry,
    Disposition* disposition)
{
    if (!own(item))
        return false;

    if (qnRuntime->isVideoWallMode())
        return true;

    if (item->isPinned())
    {
        return m_itemMap.isOccupiedBy(
            geometry,
            item,
            disposition ? &disposition->free : nullptr,
            disposition ? &disposition->occupied : nullptr
        );
    }

    pointize(geometry, disposition ? &disposition->free : nullptr);
    return true;
}

bool QnWorkbenchLayout::moveItem(QnWorkbenchItem* item, const QRect& geometry)
{
    if (!canMoveItem(item, geometry))
        return false;

    if (item->isPinned())
    {
        m_itemMap.clear(item->geometry());
        m_itemMap.fill(geometry, item);
        NX_DEBUG(kItemMapTag, lm("Item moved from cell %1 to cell %2")
            .arg(item->geometry())
            .arg(geometry));
    }

    moveItemInternal(item, geometry);

    return true;
}

void QnWorkbenchLayout::moveItemInternal(QnWorkbenchItem* item, const QRect& geometry)
{
    m_rectSet.remove(item->geometry());
    m_rectSet.insert(geometry);

    updateBoundingRectInternal();

    item->setGeometryInternal(geometry);
}

bool QnWorkbenchLayout::canMoveItems(const QList<QnWorkbenchItem*>& items,
    const QList<QRect>& geometries, Disposition* disposition) const
{
    const bool returnEarly = (disposition == nullptr);
    NX_EXPECT(items.size() == geometries.size(), "Sizes of the given containers do not match.");

    if (items.size() != geometries.size())
        return false;

    if (items.empty())
        return true;

    /* Check whether it's our items. */
    using boost::algorithm::all_of;
    if (!all_of(items, [this](QnWorkbenchItem* item) { return own(item); }))
        return false;

    /* Good points are those where items can be moved.
     * Bad points are those where they cannot be moved. */
    QSet<QPoint> goodPointSet, badPointSet;

    /* Check whether new positions do not intersect each other. */
    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (!item->isPinned())
            continue;

        const QRect& geometry = geometries[i];
        for (int r = geometry.top(); r <= geometry.bottom(); r++)
        {
            for (int c = geometry.left(); c <= geometry.right(); c++)
            {
                QPoint point(c, r);

                bool conforms = !goodPointSet.contains(point);
                if (conforms)
                {
                    goodPointSet.insert(point);
                }
                else
                {
                    if (returnEarly)
                        return false;
                    badPointSet.insert(point);
                }
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    auto itemSet = items.toSet();
    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (!item->isPinned())
        {
            pointize(geometries[i], &goodPointSet);
            continue;
        }

        bool conforms = m_itemMap.isOccupiedBy(
            geometries[i],
            itemSet,
            returnEarly ? nullptr : &goodPointSet,
            returnEarly ? nullptr : &badPointSet
        );

        if (returnEarly && !conforms)
            return false;
    }

    /* If we got here with early return on, then it means that everything is OK. */
    if (returnEarly)
        return true;

    goodPointSet.subtract(badPointSet);
    disposition->free = goodPointSet;
    disposition->occupied = badPointSet;

    return badPointSet.empty();
}

bool QnWorkbenchLayout::moveItems(const QList<QnWorkbenchItem*>& items,
    const QList<QRect>& geometries)
{
    if (!canMoveItems(items, geometries, nullptr))
        return false;

    /* Move. */
    for (auto item: items)
    {
        if (item->isPinned())
        {
            m_itemMap.clear(item->geometry());
            NX_DEBUG(kItemMapTag, lm("Batch move items: clear cell %1").arg(item->geometry()));
        }
    }

    for (int i = 0; i < items.size(); i++)
    {
        auto item = items[i];
        if (item->isPinned())
        {
            m_itemMap.fill(geometries[i], item);
            NX_DEBUG(kItemMapTag, lm("Batch move items: put on cell %1").arg(geometries[i]));
        }
        moveItemInternal(item, geometries[i]);
    }

    emit itemsMoved(items);
    return true;
}

bool QnWorkbenchLayout::pinItem(QnWorkbenchItem* item, const QRect& geometry)
{
    if (!own(item))
        return false;

    if (item->isPinned())
        return moveItem(item, geometry);

    if (m_itemMap.isOccupied(geometry))
        return false;

    m_itemMap.fill(geometry, item);
    NX_DEBUG(kItemMapTag, lm("Pin item to cell %1").arg(geometry));
    moveItemInternal(item, geometry);
    item->setFlagInternal(Qn::Pinned, true);
    return true;
}

bool QnWorkbenchLayout::unpinItem(QnWorkbenchItem* item)
{
    if (!own(item))
        return false;

    if (!item->isPinned())
        return true;

    m_itemMap.clear(item->geometry());
    NX_DEBUG(kItemMapTag, lm("Unpin item from cell %1").arg(item->geometry()));
    item->setFlagInternal(Qn::Pinned, false);
    return true;
}

QnWorkbenchItem* QnWorkbenchLayout::item(const QPoint& position) const
{
    return m_itemMap.value(position, nullptr);
}

QnWorkbenchItem* QnWorkbenchLayout::item(const QnUuid& uuid) const
{
    return m_itemByUuid.value(uuid, nullptr);
}

QnWorkbenchItem* QnWorkbenchLayout::zoomTargetItem(QnWorkbenchItem* item) const
{
    return m_zoomTargetItemByItem.value(item, nullptr);
}

QnUuid QnWorkbenchLayout::zoomTargetUuidInternal(QnWorkbenchItem* item) const
{
    QnWorkbenchItem* zoomTargetItem = this->zoomTargetItem(item);
    return zoomTargetItem ? zoomTargetItem->uuid() : QnUuid();
}

QList<QnWorkbenchItem*> QnWorkbenchLayout::zoomItems(QnWorkbenchItem* zoomTargetItem) const
{
    return m_itemsByZoomTargetItem.values(zoomTargetItem);
}

bool QnWorkbenchLayout::isEmpty() const
{
    return m_items.isEmpty();
}

float QnWorkbenchLayout::cellAspectRatio() const
{
    return m_cellAspectRatio;
}

bool QnWorkbenchLayout::hasCellAspectRatio() const
{
    return m_cellAspectRatio > 0.0;
}

QSet<QnWorkbenchItem*> QnWorkbenchLayout::items(const QRect& region) const
{
    return m_itemMap.values(region);
}

QSet<QnWorkbenchItem*> QnWorkbenchLayout::items(const QList<QRect>& regions) const
{
    return m_itemMap.values(regions);
}

const QSet<QnWorkbenchItem*>& QnWorkbenchLayout::items(const QnResourcePtr& resource) const
{
    auto pos = m_itemsByResource.find(resource);
    return pos == m_itemsByResource.end() ? m_noItems : pos.value();
}

const QSet<QnWorkbenchItem*>& QnWorkbenchLayout::items() const
{
    return m_items;
}

bool QnWorkbenchLayout::isFreeSlot(const QPointF& gridPos, const QSize& size) const
{
    QPoint gridCell = (gridPos - Geometry::toPoint(QSizeF(size)) / 2.0).toPoint();
    return !m_itemMap.isOccupied(QRect(gridCell, size));
}

QRect QnWorkbenchLayout::closestFreeSlot(const QPointF& gridPos, const QSize& size,
    TypedMagnitudeCalculator<QPoint>* metric) const
{
    if (!metric)
    {
        QnDistanceMagnitudeCalculator metric(gridPos - Geometry::toPoint(QSizeF(size)) / 2.0);
        return closestFreeSlot(gridPos, size, &metric); /* Use default metric if none provided. */
    }

    NX_DEBUG(kFreeSlotTag, lm("Seek for closestFreeSlot to %1 of size %2").args(gridPos, size));

    /* Grid cell where starting search position lies. */
    QPoint gridCell = (gridPos - Geometry::toPoint(QSizeF(size)) / 2.0).toPoint();

    /* Current bests. */
    qreal bestDistance = std::numeric_limits<qreal>::max();
    QPoint bestDelta = QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());

    int noEdge = 0;
    Qt::Edges allEdges = (Qt::TopEdge | Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge);

    /* Border being walked. */
    int processingEdge = noEdge;

    /* Borders that are known not to contain positions closer to the target than current best. */
    int checkedEdges = 0;


    /**
     * Algorithm works the following way:
     * Walker checks all available cells.
     * Expand walker to the best possible edge.
     * Repeat until we have checked all edged at least once AND did not improve current result.
     */
    QnWorkbenchGridWalker walker;
    while (true)
    {
        if (walker.hasNext())
        {
            QPoint delta = walker.next();
            NX_DEBUG(kFreeSlotTag, lm("delta %1").args(delta));

            qreal distance = metric->calculate(gridCell + delta);
            NX_DEBUG(kFreeSlotTag, lm("distance %1").args(distance));

            if (distance > bestDistance || qFuzzyEquals(distance, bestDistance))
                continue;

            // We have improved result a bit, do not exclude this edge next time
            processingEdge = noEdge;

            if (m_itemMap.isOccupied(QRect(gridCell + delta, size)))
            {
                NX_DEBUG(kFreeSlotTag, lm("delta %1 is occupied, skip").args(delta));
                continue;
            }

            checkedEdges = 0;
            bestDistance = distance;
            bestDelta = delta;
            NX_DEBUG(kFreeSlotTag, lm("Remember delta %1 as best (distance %2)")
                .arg(delta)
                .arg(distance));
        }
        else
        {
            NX_DEBUG(kFreeSlotTag, lm("Walker has finished the %1").arg(processingEdge));
            checkedEdges |= processingEdge;
            if (checkedEdges == allEdges && bestDistance < std::numeric_limits<qreal>::max())
            {
                NX_DEBUG(kFreeSlotTag, lm("Best position found: %1 (delta %2, distance %3)")
                    .arg(QRect(gridCell + bestDelta, size))
                    .arg(bestDelta)
                    .arg(bestDistance));
                return QRect(gridCell + bestDelta, size);
            }

            // We have checked all edges but there were no place at all. Let's start over.
            if (checkedEdges == allEdges)
                checkedEdges = noEdge;

            using edge_t = std::pair<Qt::Edge, QPoint>;
            std::array<edge_t, 4> expansion{{
                {Qt::RightEdge,   QPoint(walker.rect().right() + 1,   gridCell.y())},
                {Qt::LeftEdge,    QPoint(walker.rect().left() - 1,    gridCell.y())},
                {Qt::BottomEdge,  QPoint(gridCell.x(),                walker.rect().bottom() + 1)},
                {Qt::TopEdge,     QPoint(gridCell.x(),                walker.rect().top() - 1)},
            }};

            std::list<edge_t> edgesToCheck;
            for (auto v: expansion)
            {
                // Rely on the monotonic magnitude nature, do not expand to the same side again
                if ((checkedEdges & v.first) == v.first)
                    continue;

                NX_DEBUG(kFreeSlotTag, lm("Checking edge %1: distance %2")
                    .arg(v.first)
                    .arg(metric->calculate(gridCell + v.second)));
                edgesToCheck.push_back(v);
            }

            NX_EXPECT(!edgesToCheck.empty());
            if (edgesToCheck.empty())
            {
                // Something goes wrong. Let's start over.
                checkedEdges = noEdge;
                continue;
            }

            const auto bestEdgeIter = std::min_element(edgesToCheck.cbegin(), edgesToCheck.cend(),
                [metric, &gridCell](const edge_t& left, const edge_t& right)
            {
                return metric->calculate(gridCell + left.second)
                    < metric->calculate(gridCell + right.second);
            });

            const Qt::Edge bestEdge = bestEdgeIter->first;
            NX_DEBUG(kFreeSlotTag, lm("Expanding the best border %1").arg(bestEdge));
            walker.expand(bestEdge);
            processingEdge = bestEdge;
        }
    }
}

void QnWorkbenchLayout::updateBoundingRectInternal()
{
    QRect boundingRect = m_rectSet.boundingRect();
    if (m_boundingRect == boundingRect)
        return;

    QRect oldRect = m_boundingRect;
    m_boundingRect = boundingRect;
    emit boundingRectChanged(oldRect, boundingRect);
    emit dataChanged(Qn::LayoutBoundingRectRole);
}

void QnWorkbenchLayout::setCellAspectRatio(float cellAspectRatio)
{
    if (cellAspectRatio < 0.0 || qFuzzyIsNull(cellAspectRatio)) /* Negative means 'use default value'. */
        cellAspectRatio = -1.0;

    if (qFuzzyCompare(m_cellAspectRatio, cellAspectRatio))
        return;

    m_cellAspectRatio = cellAspectRatio;

    emit cellAspectRatioChanged();
    emit dataChanged(Qn::LayoutCellAspectRatioRole);
}

qreal QnWorkbenchLayout::cellSpacingValue(Qn::CellSpacing spacing)
{
    switch (spacing)
    {
        case Qn::CellSpacing::None:
            return 0.0;
        case Qn::CellSpacing::Small:
            return 0.05;
        case Qn::CellSpacing::Medium:
            return 0.1;
        case Qn::CellSpacing::Large:
            return 0.15;
    }
    NX_ASSERT(false, "Unhandled enum value");
    return 0.0;
}

qreal QnWorkbenchLayout::cellSpacing() const
{
    return m_cellSpacing;
}

void QnWorkbenchLayout::setCellSpacing(qreal spacing)
{
    if (spacing < 0.0) //< Negative means 'use default value'
    {
        setCellSpacing(qnGlobals->defaultLayoutCellSpacing());
        return;
    }

    if (qFuzzyEquals(m_cellSpacing, spacing))
        return;

    m_cellSpacing = spacing;

    emit cellSpacingChanged();
    emit dataChanged(Qn::LayoutCellSpacingRole);
}

bool QnWorkbenchLayout::locked() const
{
    return m_locked;
}

void QnWorkbenchLayout::setLocked(bool value)
{
    if (m_locked == value)
        return;
    m_locked = value;

    emit lockedChanged();
}

const QRect& QnWorkbenchLayout::boundingRect() const
{
    return m_boundingRect;
}

void QnWorkbenchLayout::initCellParameters()
{
    m_cellAspectRatio = -1.0;
    m_cellSpacing = qnGlobals->defaultLayoutCellSpacing();
}

bool QnWorkbenchLayout::own(QnWorkbenchItem* item) const
{
    NX_EXPECT(item);
    NX_EXPECT(item->layout() == this, "Item must belong to this layout.");
    return item && item->layout() == this;
}

QVariant QnWorkbenchLayout::data(int role) const
{
    switch (role)
    {
        case Qn::ResourceNameRole:
            return m_name;
        case Qn::LayoutCellSpacingRole:
            return m_cellSpacing;
        case Qn::LayoutCellAspectRatioRole:
            return m_cellAspectRatio;
        case Qn::LayoutBoundingRectRole:
            return m_boundingRect;
        default:
            return m_dataByRole.value(role);
    }
}

QHash<int, QVariant> QnWorkbenchLayout::data() const
{
    return m_dataByRole;
}

bool QnWorkbenchLayout::setData(int role, const QVariant& value)
{
    switch (role)
    {
        case Qn::ResourceNameRole:
            if (value.canConvert<QString>())
            {
                setName(value.toString());
                return true;
            }
            else
            {
                qnWarning("Provided name value '%1' must be convertible to QString.", value);
                return false;
            }
        case Qn::LayoutCellSpacingRole:
            if (value.canConvert<qreal>())
            {
                setCellSpacing(value.toReal());
                return true;
            }
            else
            {
                qnWarning("Provided cell spacing value '%1' must be convertible to qreal.", value);
                return false;
            }
        case Qn::LayoutCellAspectRatioRole:
        {
            bool ok;
            qreal cellAspectRatio = value.toReal(&ok);
            if (ok)
            {
                setCellAspectRatio(cellAspectRatio);
                return true;
            }
            else
            {
                qnWarning("Provided cell aspect ratio value '%1' must be convertible to qreal.", value);
                return false;
            }
        }
        case Qn::LayoutBoundingRectRole:
            if (m_boundingRect == value.toRect())
            {
                return true;
            }
            else
            {
                qnWarning("Changing bounding rect of a workbench layout is not supported.");
                return false;
            }
        default:
            QVariant& localValue = m_dataByRole[role];
            if (localValue != value)
            {
                localValue = value;
                emit dataChanged(role);
            }
            return true;
    }
}

void QnWorkbenchLayout::centralizeItems()
{
    QRect brect = boundingRect();
    int xdiff = -brect.center().x();
    int ydiff = -brect.center().y();

    QList<QnWorkbenchItem*> itemsList = m_items.toList();
    QList<QRect> geometries;
    foreach(QnWorkbenchItem* item, itemsList)
        geometries << item->geometry().adjusted(xdiff, ydiff, xdiff, ydiff);
    moveItems(itemsList, geometries);
}

bool QnWorkbenchLayout::isSearchLayout() const
{
    if (!data().contains(Qn::LayoutSearchStateRole))
        return false;
    return data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>().step > 0;
}

bool QnWorkbenchLayout::isLayoutTourReview() const
{
    return data().contains(Qn::LayoutTourUuidRole);
}
