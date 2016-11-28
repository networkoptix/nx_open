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
    m_watchedLayouts.insert(layout);
    for (auto item: layout->getItems())
        m_items.insert(item.resource.id);

    connect(layout, &QnLayoutResource::itemRemoved, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            m_items.remove(item.resource.id);
        });
}

void QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    m_watchedLayouts.remove(layout);
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return m_watchedLayouts;
}

bool QnLayoutItemAggregator::hasItem(const QnUuid& id) const
{
    return m_items.contains(id);
}
