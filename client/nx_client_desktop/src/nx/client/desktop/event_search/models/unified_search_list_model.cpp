#include "unified_search_list_model.h"
#include "private/unified_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

UnifiedSearchListModel::UnifiedSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

UnifiedSearchListModel::~UnifiedSearchListModel()
{
}

QnVirtualCameraResourcePtr UnifiedSearchListModel::camera() const
{
    return d->camera();
}

void UnifiedSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

UnifiedSearchListModel::Types UnifiedSearchListModel::filter() const
{
    return d->filter();
}

void UnifiedSearchListModel::setFilter(Types filter)
{
    d->setFilter(filter);
}

QnTimePeriod UnifiedSearchListModel::selectedTimePeriod() const
{
    return d->selectedTimePeriod();
}

void UnifiedSearchListModel::setSelectedTimePeriod(const QnTimePeriod& value)
{
    d->setSelectedTimePeriod(value);
}

vms::event::EventType UnifiedSearchListModel::selectedEventType() const
{
    return d->selectedEventType();
}

void UnifiedSearchListModel::setSelectedEventType(vms::event::EventType value)
{
    d->setSelectedEventType(value);
}

bool UnifiedSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

void UnifiedSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    d->fetchMore();
}

void UnifiedSearchListModel::setAnalyticsSearchRect(const QRectF& relativeRect)
{
    d->setAnalyticsSearchRect(relativeRect);
}

} // namespace desktop
} // namespace client
} // namespace nx
