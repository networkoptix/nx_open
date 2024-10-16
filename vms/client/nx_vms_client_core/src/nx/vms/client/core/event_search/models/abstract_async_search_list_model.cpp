// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_async_search_list_model.h"

#include "fetched_data.h"

namespace nx::vms::client::core {

struct AbstractAsyncSearchListModel::Private
{
    AbstractAsyncSearchListModel* q;

    struct FetchRequestData
    {
        nx::Uuid id;
        FetchRequest request;
        AbstractSearchListModel::FetchCompletionHandler completionHandler;
    };
    std::optional<FetchRequestData> requestData;

    void finishFetch(EventSearch::FetchResult result,
        const FetchedDataRanges& fetchedRanges,
        const OptionalTimePeriod& timeWindow)
    {
        if (!NX_ASSERT(requestData))
            return;

        std::optional<FetchRequestData> data;
        std::swap(requestData, data);

        switch (result)
        {
            case EventSearch::FetchResult::failed:
                q->setFetchedTimeWindow({});
                [[fallthrough]];

            case EventSearch::FetchResult::cancelled:
            {
                emit q->fetchFinished(result, -1, data->request);
                break;
            }

            default:
            {
                q->setFetchedTimeWindow(timeWindow);
                const int centralIndex = q->calculateCentralItemIndex(
                    fetchedRanges, data->request.direction);
                emit q->fetchFinished(result, centralIndex, data->request);
                break;
            }
        }
    }
};

// ------------------------------------------------------------------------------------------------

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(
    int maximumCount,
    QObject* parent)
    :
    base_type(maximumCount, parent),
    d(new Private{.q = this})
{
}

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(QObject* parent):
    AbstractAsyncSearchListModel(kStandardMaximumItemCount, parent)
{
}

AbstractAsyncSearchListModel::~AbstractAsyncSearchListModel()
{
}

bool AbstractAsyncSearchListModel::fetchData(const FetchRequest& request)
{
    if (fetchInProgress() || !canFetchData(request.direction))
        return false;

    Private::FetchRequestData data;
    data.id = nx::Uuid::createUuid();
    data.request = request;

    const auto completionHandler =
        [this, id = data.id](EventSearch::FetchResult result,
            const FetchedDataRanges& fetchedRanges,
            const OptionalTimePeriod& timeWindow)
        {
            if (d->requestData && d->requestData->id == id)
                d->finishFetch(result, fetchedRanges, timeWindow);
        };

    d->requestData = data;

    if (!requestFetch(request, std::move(completionHandler)))
    {
        d->requestData = {};
        return false;
    }

    NX_VERBOSE(this, "Fetch data request id: %1", data.id);
    emit asyncFetchStarted(request);

    return true;
}

bool AbstractAsyncSearchListModel::fetchInProgress() const
{
    return (bool) d->requestData;
}

bool AbstractAsyncSearchListModel::cancelFetch()
{
    if (!fetchInProgress())
        return false;

    d->finishFetch(EventSearch::FetchResult::cancelled, {}, {});
    return true;
}

} // namespace nx::vms::client::core
