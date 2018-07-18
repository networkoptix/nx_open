#include "abstract_async_search_list_model.h"
#include "private/abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>

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

bool AbstractAsyncSearchListModel::cancelFetch()
{
    if (!fetchInProgress())
        return false;

    d->cancelPrefetch();
    return true;
}

void AbstractAsyncSearchListModel::clearData()
{
    d->clearData();
}

bool AbstractAsyncSearchListModel::canFetch() const
{
    return d->canFetch();
}

void AbstractAsyncSearchListModel::requestFetch()
{
    d->requestFetch();
}

bool AbstractAsyncSearchListModel::fetchInProgress() const
{
    return d->fetchInProgress();
}

void AbstractAsyncSearchListModel::truncateToMaximumCount()
{
    d->truncateToMaximumCount();
}

void AbstractAsyncSearchListModel::truncateToRelevantTimePeriod()
{
    d->truncateToRelevantTimePeriod();
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
    {
        using namespace std::chrono;
        return data(index, Qn::TimestampRole).value<qint64>()
            >= microseconds(milliseconds(fetchedTimeWindow().startTimeMs)).count();
    }

    bool handled = false;
    const auto result = d->data(index, role, handled);

    return handled ? result : base_type::data(index, role);
}

} // namespace desktop
} // namespace client
} // namespace nx
