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

void AbstractAsyncSearchListModel::relevantTimePeriodChanged(const QnTimePeriod& previousValue)
{
    d->relevantTimePeriodChanged(previousValue);
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
    {
        using namespace std::chrono;
        return data(index, Qn::TimestampRole).value<qint64>()
            >= microseconds(milliseconds(d->fetchedTimePeriod().startTimeMs)).count();
    }

    bool handled = false;
    const auto result = d->data(index, role, handled);

    return handled ? result : base_type::data(index, role);
}

bool AbstractAsyncSearchListModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return d->canFetchMore();
}

void AbstractAsyncSearchListModel::fetchMore(const QModelIndex& /*parent*/)
{
    prefetchAsync(
        [this, guard = QPointer<AbstractAsyncSearchListModel>(this)](qint64 earliestTimeMs)
        {
            if (!guard)
                return;

            const bool cancelled = earliestTimeMs < 0;

            QnRaiiGuard finishFetch(
                [this]() { beginFinishFetch(); },
                [this, cancelled]() { endFinishFetch(cancelled); });

            if (!cancelled)
                commitPrefetch(earliestTimeMs);
        });
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

} // namespace desktop
} // namespace client
} // namespace nx
