#include "flat_camera_data_loader.h"

#include <api/helpers/chunks_request_data.h>

#include <analytics/db/analytics_db_types.h>

#include <common/common_module.h>

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <api/server_rest_connection.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>
#include <nx/utils/guarded_callback.h>

namespace {

/** Fake handle for simultaneous load request. Initial value is big enough to not conflict with real request handles. */
QAtomicInt qn_fakeHandle(INT_MAX / 2);

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

} // namespace

QnFlatCameraDataLoader::QnFlatCameraDataLoader(
    const QnVirtualCameraResourcePtr& camera,
    const QnMediaServerResourcePtr& server,
    Qn::TimePeriodContent dataType,
    QObject* parent)
    :
    QnAbstractCameraDataLoader(camera, dataType, parent),
    m_server(server)
{
    NX_ASSERT(camera);
    NX_VERBOSE(this, "Creating loader");
}

QnFlatCameraDataLoader::~QnFlatCameraDataLoader()
{
}

int QnFlatCameraDataLoader::load(const QString &filter, const qint64 resolutionMs)
{
    if (filter != m_filter)
    {
        NX_VERBOSE(this, "Updating filter to %1", filterRepresentation(filter, m_dataType));
        discardCachedData();
    }
    m_filter = filter;

    /* Check whether data is currently being loaded. */
    if (m_loading.handle > 0)
    {
        NX_VERBOSE(this, "Data is already being loaded");
        auto handle = qn_fakeHandle.fetchAndAddAcquire(1);
        m_loading.waitingHandles << handle;
        return handle;
    }

    /* We need to load all data after the already loaded piece, assuming there were no periods before already loaded. */
    qint64 startTimeMs = 0;
    if (m_loadedData && !m_loadedData->dataSource().isEmpty())
    {
        const auto last = (m_loadedData->dataSource().cend() - 1);
        if (last->isInfinite())
            startTimeMs = last->startTimeMs;
        else
            startTimeMs = last->endTimeMs() - minOverlapDuration;

        /* If system time were changed back, we may have periods in the future, so currently recorded chunks will not be loaded. */
        qint64 currentSystemTime = qnSyncTime->currentMSecsSinceEpoch();
        if (currentSystemTime < startTimeMs)
            startTimeMs = currentSystemTime - minOverlapDuration;
    }

    m_loading.clear(); /* Just in case. */
    m_loading.startTimeMs = startTimeMs;
    m_loading.handle = sendRequest(startTimeMs, resolutionMs);
    NX_VERBOSE(this, "Loading period since %1 (%2), request %3, filter %4",
        startTimeMs,
        nx::utils::timestampToDebugString(startTimeMs),
        m_loading.handle,
        filterRepresentation(m_filter, m_dataType));
    return m_loading.handle;
}

void QnFlatCameraDataLoader::discardCachedData(const qint64 /*resolutionMs*/)
{
    NX_VERBOSE(this, "Discarding cached data");
    m_loading.clear();
    m_loadedData.clear();
}

void QnFlatCameraDataLoader::updateServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;
}

int QnFlatCameraDataLoader::sendRequest(qint64 startTimeMs, qint64 resolutionMs)
{
    if (!m_server)
    {
        NX_VERBOSE(this, "There is no current server, cannot send request");
        return 0;   // TODO: #GDM #bookmarks make sure invalid value is handled
    }

    auto connection = m_server->restConnection();
    if (!NX_ASSERT(connection))
    {
        return 0;   // TODO: #GDM #bookmarks make sure invalid value is handled
    }

    QnChunksRequestData requestData;
    requestData.resList << m_resource.dynamicCast<QnVirtualCameraResource>();
    requestData.startTimeMs = startTimeMs;
    requestData.endTimeMs = DATETIME_NOW,   /* Always load data to the end. */
    requestData.filter = m_filter;
    requestData.periodsType = m_dataType;
    requestData.detailLevel = std::chrono::milliseconds(resolutionMs);
    requestData.pickRequestVersion(m_server->commonModule());

    return connection->recordedTimePeriods(requestData,
        nx::utils::guarded(this,
            [this](bool success, int handle, const MultiServerPeriodDataList& timePeriods)
            {
                at_timePeriodsReceived(success, handle, timePeriods);
            }), thread());
}

void QnFlatCameraDataLoader::at_timePeriodsReceived(bool success,
    int requestHandle,
    const MultiServerPeriodDataList &timePeriods)
{
    NX_VERBOSE(this, "Received answer for req %1", requestHandle);

    std::vector<QnTimePeriodList> rawPeriods;

    for(auto period: timePeriods)
        rawPeriods.push_back(period.periods);
    QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(QnTimePeriodList::mergeTimePeriods(rawPeriods)));

    handleDataLoaded(success, requestHandle, data);
}

void QnFlatCameraDataLoader::handleDataLoaded(bool success, int requestHandle, const QnAbstractCameraDataPtr &data)
{
    if (m_loading.handle != requestHandle)
        return;

    NX_VERBOSE(this, "Loaded data for %1 (%2), actual filter %3",
        m_loading.startTimeMs,
        nx::utils::timestampToDebugString(m_loading.startTimeMs),
        filterRepresentation(m_filter, m_dataType));

    if (!success)
    {
        NX_VERBOSE(this, "Load Failed");
        for(auto handle: m_loading.waitingHandles)
            emit failed(handle);
        emit failed(requestHandle);
        m_loading.clear();
        return;
    }

    QnTimePeriod loadedPeriod(m_loading.startTimeMs, QnTimePeriod::kInfiniteDuration);

    if (data)
    {
        if (!m_loadedData)
        {
            m_loadedData = data;
            NX_VERBOSE(this, "First dataset received, size %1", data->dataSource().size());
        }
        else if (!data->isEmpty())
        {
            NX_VERBOSE(this, "New dataset received (size %1), merging with existing (size %2).",
                data->dataSource().size(),
                m_loadedData->dataSource().size());
            m_loadedData->update(data, loadedPeriod);
            NX_VERBOSE(this, "Merging finished, size %1.", m_loadedData->dataSource().size());
        }
    }
    else
    {
        NX_VERBOSE(this, "Empty data received");
    }

    for(auto handle: m_loading.waitingHandles)
        emit ready(m_loadedData, loadedPeriod, handle);
    emit ready(m_loadedData, loadedPeriod, requestHandle);
    m_loading.clear();
}

QnFlatCameraDataLoader::LoadingInfo::LoadingInfo():
    handle(0),
    startTimeMs(0)
{}

void QnFlatCameraDataLoader::LoadingInfo::clear() {
    handle = 0;
    startTimeMs = 0;
    waitingHandles.clear();
}
