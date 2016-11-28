#include "layout_item_aggregator.h"

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
}

void QnLayoutItemAggregator::removeWatchedLayout(const QnLayoutResourcePtr& layout)
{
    m_watchedLayouts.remove(layout);
}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return m_watchedLayouts;
}
