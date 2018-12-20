#include "abstract_async_search_list_model.h"
#include "private/abstract_async_search_list_model_p.h"

#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(
    QnWorkbenchContext* context, CreatePrivate dCreator, QObject* parent)
    :
    base_type(context, parent),
    d(dCreator())
{
}

AbstractAsyncSearchListModel::~AbstractAsyncSearchListModel()
{
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

bool AbstractAsyncSearchListModel::canFetchNow() const
{
    return !d->fetchInProgress();
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

    bool handled = false;
    const auto result = d->data(index, role, handled);

    if (role == Qt::DisplayRole && result.toString().isEmpty())
    {
        NX_ASSERT(false);
        return QString("?"); //< Title must not be empty.
    }

    return handled ? result : base_type::data(index, role);
}

} // namespace nx::vms::client::desktop
