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
    auto addItem = [this](const QnLayoutItemData& item)
        {
            if (!item.resource.id.isNull())
                m_items.insert(item.resource.id);
        };

    m_watchedLayouts.insert(layout);
    for (auto item: layout->getItems())
        addItem(item);

    connect(layout, &QnLayoutResource::itemAdded, this,
        [this, addItem](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            addItem(item);
        });

    connect(layout, &QnLayoutResource::itemRemoved, this,
        [this](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
        {
            m_items.remove(item.resource.id);
        });
}

void QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    layout->disconnect(this);
    m_watchedLayouts.remove(layout);
    for (auto item : layout->getItems())
        m_items.remove(item.resource.id);
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return m_watchedLayouts;
}

bool QnLayoutItemAggregator::hasItem(const QnUuid& id) const
{
    return m_items.contains(id);
}
