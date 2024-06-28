// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_async_search_list_model.h"

#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/client_core_globals.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

#include "fetch_request.h"
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
};

// ------------------------------------------------------------------------------------------------

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(
    int maximumCount,
    SystemContext* systemContext,
    QObject* parent)
    :
    base_type(maximumCount, systemContext, parent),
    d(new Private{.q = this})
{
}

AbstractAsyncSearchListModel::AbstractAsyncSearchListModel(
    SystemContext* systemContext,
    QObject* parent)
    :
    AbstractAsyncSearchListModel(kStandardMaximumItemCount, systemContext, parent)
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
    auto completionHandler =
        [this, id = data.id, request](EventSearch::FetchResult result,
            const FetchedDataRanges& fetchedRanges,
            const OptionalTimePeriod& timeWindow)
        {
            if (!d->requestData || d->requestData->id != id)
                return; // We suppose that the requests can be overriden.

            d->requestData = {};
            setFetchInProgress(false);
            setFetchedTimeWindow(timeWindow);
            const int centralIndex = calculateCentralItemIndex(fetchedRanges, request.direction);
            emit fetchFinished(result, centralIndex, request);
        };

    setFetchInProgress(true);
    emit fetchCommitStarted(request);
    if (!requestFetch(request, std::move(completionHandler)))
    {
        setFetchInProgress(false);
        emit fetchFinished(EventSearch::FetchResult::failed, -1, request);
        return false;
    }

    NX_VERBOSE(this, "Fetch data request id: %1", data.id);

    d->requestData = data;
    emit asyncFetchStarted(request);

    return true;
}

bool AbstractAsyncSearchListModel::cancelFetch()
{
    if (!fetchInProgress())
        return false;

    if (d->requestData && d->requestData->completionHandler)
        d->requestData->completionHandler(EventSearch::FetchResult::cancelled, {}, {});

    d->requestData = {};
    return true;
}

} // namespace nx::vms::client::core
