#include "abstract_async_search_list_model.h"
#include "private/abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>

namespace nx {
namespace client {
namespace desktop {

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(CreatePrivate dCreator, QObject* parent):
    base_type(parent),
    d(dCreator())
{
}

AbstractAsyncSearchListModel::~AbstractAsyncSearchListModel()
{
}

AbstractAsyncSearchListModel::Private* AbstractAsyncSearchListModel::d_func()
{
    return d.data();
}

QnVirtualCameraResourcePtr AbstractAsyncSearchListModel::camera() const
{
    return d->camera();
}

void AbstractAsyncSearchListModel::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

QnTimePeriod AbstractAsyncSearchListModel::selectedTimePeriod() const
{
    return d->selectedTimePeriod();
}

void AbstractAsyncSearchListModel::setSelectedTimePeriod(const QnTimePeriod& value)
{
    d->setSelectedTimePeriod(value);
}

void AbstractAsyncSearchListModel::clear()
{
    d->clear();
}

int AbstractAsyncSearchListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d->count();
}

QVariant AbstractAsyncSearchListModel::data(const QModelIndex& index, int role) const
{
    if (!isValid(index))
        return QVariant();

    if (role == Qn::AnimatedRole)
        return data(index, Qn::TimestampRole).value<qint64>() >= d->fetchedTimePeriod().startTimeMs;

    bool handled = false;
    const auto result = d->data(index, role, handled);

    return handled ? result : base_type::data(index, role);
}

bool AbstractAsyncSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

bool AbstractAsyncSearchListModel::prefetchAsync(PrefetchCompletionHandler completionHandler)
{
    return d->prefetch(completionHandler);
}

void AbstractAsyncSearchListModel::commitPrefetch(qint64 earliestTimeToCommitMs)
{
    d->commit(earliestTimeToCommitMs);
}

bool AbstractAsyncSearchListModel::fetchInProgress() const
{
    return d->fetchInProgress();
}

bool AbstractAsyncSearchListModel::isConstrained() const
{
    const auto period = selectedTimePeriod();
    return period.startTimeMs > 0 || !period.isInfinite();
}

} // namespace desktop
} // namespace client
} // namespace nx
