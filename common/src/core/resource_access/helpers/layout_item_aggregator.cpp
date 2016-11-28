#include "layout_item_aggregator.h"

QnLayoutItemAggregator::QnLayoutItemAggregator(QObject* parent):
    base_type(parent)
{

}

QnLayoutItemAggregator::~QnLayoutItemAggregator()
{

}

QSet<QnLayoutResourcePtr> QnLayoutItemAggregator::watchedLayouts() const
{
    return QSet<QnLayoutResourcePtr>();
}
