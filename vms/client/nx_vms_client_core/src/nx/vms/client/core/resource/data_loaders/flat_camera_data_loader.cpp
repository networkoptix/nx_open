// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "flat_camera_data_loader.h"

#include <analytics/db/analytics_db_types.h>
#include <api/helpers/chunks_request_data.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <recording/time_period_list.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::core {

namespace {

/** Minimum time (in milliseconds) for overlapping time periods requests.  */
const int minOverlapDuration = 120 * 1000;

QString filterRepresentation(const QString& filter, Qn::TimePeriodContent dataType)
{
    switch (dataType)
    {
        case Qn::AnalyticsContent:
            return toString(QJson::deserialized<nx::analytics::db::Filter>(filter.toUtf8()));
        default:
            break;
    }
    return filter;
}

QString periodsLogString(const QnTimePeriodList& periodList, int count = 100)
{
    QStringList result;
    int index = std::max((int) periodList.size() - count, 0);
    for (int i = index; i < (int) periodList.size(); ++i)
    {
        result.append(
            nx::format(
                "%1 [%2, %3]",
                i,
                periodList[i].startTime().count(),
                periodList[i].endTime().count()));
    }
    return result.join('\n');
}

} // namespace

FlatCameraDataLoader::FlatCameraDataLoader(
    const QnVirtualCameraResourcePtr& camera,
    Qn::TimePeriodContent dataType,
    QObject* parent)
    :
    AbstractCameraDataLoader(camera, dataType, parent)
{
    NX_ASSERT(camera);
    NX_VERBOSE(this, "Creating loader");
}

FlatCameraDataLoader::~FlatCameraDataLoader()
{
}

void FlatCameraDataLoader::load(const QString &filter, const qint64 resolutionMs)
{
    if (filter != m_filter)
    {
        NX_VERBOSE(this, "Updating filter to %1", filterRepresentation(filter, m_dataType));
        discardCachedData();
    }
    m_filter = filter;

    /* Check whether data is currently being loaded. */
    if (m_loading.startTimeMs > 0)
    {
        NX_VERBOSE(this, "Data is already being loaded");
        return;
    }

    /* We need to load all data after the already loaded piece, assuming there were no periods before already loaded. */
    qint64 startTimeMs = 0;
    if (!m_loadedData.empty())
    {
        const auto last = (m_loadedData.cend() - 1);
        if (last->isInfinite())
            startTimeMs = last->startTimeMs;
        else
            startTimeMs = last->endTimeMs() - minOverlapDuration;

        /* If system time were changed back, we may have periods in the future, so currently recorded chunks will not be loaded. */
        qint64 currentSystemTime = qnSyncTime->currentMSecsSinceEpoch();
        if (currentSystemTime < startTimeMs)
            startTimeMs = currentSystemTime - minOverlapDuration;
    }

    m_loading.startTimeMs = startTimeMs;
    sendRequest(startTimeMs, resolutionMs);
    NX_VERBOSE(this, "Loading period since %1 (%2), filter %3",
        startTimeMs,
        nx::utils::timestampToDebugString(startTimeMs),
        filterRepresentation(m_filter, m_dataType));
}

void FlatCameraDataLoader::discardCachedData(const qint64 /*resolutionMs*/)
{
    NX_VERBOSE(this, "Discarding cached data");
    m_loading = LoadingInfo();
    m_loadedData.clear();
}

void FlatCameraDataLoader::setStorageLocation(nx::vms::api::StorageLocation value)
{
    if (m_storageLocation == value)
        return;

    NX_VERBOSE(this, "Select storage location: %1", value);
    m_storageLocation = value;
    discardCachedData();
}

int FlatCameraDataLoader::sendRequest(qint64 startTimeMs, qint64 resolutionMs)
{
    auto systemContext = SystemContext::fromResource(m_resource);
    if (!NX_ASSERT(systemContext))
        return 0;

    auto connection = systemContext->connectedServerApi();
    if (!connection)
    {
        NX_VERBOSE(this, "There is no current server, cannot send request");
        return 0;   //< TODO: #sivanov #bookmarks make sure invalid value is handled.
    }

    QnChunksRequestData requestData;
    requestData.resList << m_resource.dynamicCast<QnVirtualCameraResource>();
    requestData.startTimeMs = startTimeMs;
    requestData.endTimeMs = DATETIME_NOW,   /* Always load data to the end. */
    requestData.filter = m_filter;
    requestData.periodsType = m_dataType;
    requestData.detailLevel = std::chrono::milliseconds(resolutionMs);
    requestData.storageLocation = m_storageLocation;

    return connection->recordedTimePeriods(requestData,
        nx::utils::guarded(this,
            [this](bool success, int handle, const MultiServerPeriodDataList& timePeriods)
            {
                at_timePeriodsReceived(success, handle, timePeriods);
            }), thread());
}

void FlatCameraDataLoader::at_timePeriodsReceived(bool success,
    int requestHandle,
    const MultiServerPeriodDataList &timePeriods)
{
    NX_VERBOSE(this, "Received answer for req %1 (%2)", requestHandle, m_dataType);

    std::vector<QnTimePeriodList> rawPeriods;

    for(auto period: timePeriods)
    {
        rawPeriods.push_back(period.periods);
        NX_VERBOSE(this, "Received\n%1", periodsLogString(period.periods));
    }

    QnTimePeriodList periods(QnTimePeriodList::mergeTimePeriods(rawPeriods));

    NX_VERBOSE(this, "Merged\n%1", periodsLogString(periods));

    handleDataLoaded(success, std::move(periods), requestHandle);
}

void FlatCameraDataLoader::handleDataLoaded(
    bool success,
    QnTimePeriodList&& periods,
    int requestHandle)
{
    NX_VERBOSE(this, "Loaded data for %1 (%2), actual filter %3",
        m_loading.startTimeMs,
        nx::utils::timestampToDebugString(m_loading.startTimeMs),
        filterRepresentation(m_filter, m_dataType));

    if (!success)
    {
        NX_VERBOSE(this, "Load Failed");
        emit failed(requestHandle);
        m_loading = LoadingInfo();
        return;
    }

    if (m_loadedData.empty())
    {
        m_loadedData = periods;
        NX_VERBOSE(this, "First dataset received, size %1", periods.size());
    }
    else if (!periods.empty())
    {
        NX_VERBOSE(this,
            "New dataset received (size %1), merging with existing (size %2).",
            periods.size(),
            m_loadedData.size());

        QnTimePeriodList::overwriteTail(m_loadedData, periods, m_loading.startTimeMs);
        NX_VERBOSE(this, "Merging finished, size %1.", m_loadedData.size());
    }

    emit ready(m_loading.startTimeMs);
    m_loading = LoadingInfo();
}

} // namespace nx::vms::client::core
