#include "layout_item_aggregator.h"

#include <core/resource/layout_resource.h>

QnLayoutItemAggregator::QnLayoutItemAggregator(QObject* parent):
    base_type(parent)
{

}

QnLayoutItemAggregator::~QnLayoutItemAggregator()
{

}

void QnLayoutItemAggregator::addWatchedLayout(const QnLayoutResourcePtr& layout)
{
    if (!m_watchedLayouts.insert(layout))
        return;

    for (auto item: layout->getItems())
        handleItemAdded(item);

    connect(layout, &QnLayoutResource::itemAdded, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            handleItemAdded(item);
        });

    connect(layout, &QnLayoutResource::itemRemoved, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            handleItemRemoved(item);
        });
}

void QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    if (!m_watchedLayouts.remove(layout))
        return;

    layout->disconnect(this);
    for (auto item : layout->getItems())
        handleItemRemoved(item);
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return m_watchedLayouts.keys().toSet();
}

bool QnLayoutItemAggregator::hasItem(const QnUuid& id) const
{
    return m_items.contains(id);
}

void QnLayoutItemAggregator::handleItemAdded(const QnLayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    if (m_items.insert(resourceId))
        emit itemAdded(resourceId);
}

void QnLayoutItemAggregator::handleItemRemoved(const QnLayoutItemData& item)
{
    const auto resourceId = item.resource.id;
    if (resourceId.isNull())
        return;

    if (m_items.remove(resourceId))
        emit itemRemoved(resourceId);
}
