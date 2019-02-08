#include "layout_item_aggregator.h"

#include <core/resource/layout_resource.h>

QnLayoutItemAggregator::QnLayoutItemAggregator(QObject* parent):
    base_type(parent)
{

}

QnLayoutItemAggregator::~QnLayoutItemAggregator()
{

}

bool QnLayoutItemAggregator::addWatchedLayout(const QnLayoutResourcePtr& layout)
{
    if (m_watchedLayouts.contains(layout))
        return false;

    m_watchedLayouts.insert(layout);

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

    return true;
}

bool QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    if (!m_watchedLayouts.contains(layout))
        return false;

    m_watchedLayouts.remove(layout);

    layout->disconnect(this);
    for (auto item : layout->getItems())
        handleItemRemoved(item);

    return true;
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return m_watchedLayouts;
}

bool QnLayoutItemAggregator::hasLayout(const QnLayoutResourcePtr& layout) const
{
    return m_watchedLayouts.contains(layout);
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
