#include "flat_camera_data_loader.h"

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>

namespace {
    /** Fake handle for simultaneous load request. Initial value is big enough to not conflict with real request handles. */
    QAtomicInt qn_fakeHandle(INT_MAX / 2);

    /** Minimum time (in milliseconds) for overlapping time periods requests.  */
    const int minOverlapDuration = 120*1000;

    QString dt(qint64 time) {
        return QDateTime::fromMSecsSinceEpoch(time).toString(lit("MMM/dd/yyyy hh:mm:ss"));
    }
}

QnFlatCameraDataLoader::QnFlatCameraDataLoader(
    const QnVirtualCameraResourcePtr& camera,
    const QnMediaServerResourcePtr& server,
    Qn::TimePeriodContent dataType,
    QObject* parent)
    :
    QnAbstractCameraDataLoader(camera, dataType, parent),
    m_server(server)
{
    trace(lit("Creating loader"));
    if(!camera)
        qnNullWarning(camera);
}

QnFlatCameraDataLoader::~QnFlatCameraDataLoader()
{
    trace(lit("Deleting loader"));
}

int QnFlatCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    Q_UNUSED(resolutionMs);

    if (filter != m_filter)
    {
        trace(lit("Updating filter from %1 to %2").arg(m_filter).arg(filter));
        discardCachedData();
    }
    m_filter = filter;

    /* Check whether data is currently being loaded. */
    if (m_loading.handle > 0)
    {
        trace(lit("Data is already being loaded"));
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
    m_loading.handle = sendRequest(startTimeMs);
    trace(lit("Loading period since %1 (%2), request %3").arg(startTimeMs).arg(dt(startTimeMs)).arg(m_loading.handle));
    return m_loading.handle;
}

void QnFlatCameraDataLoader::discardCachedData(const qint64 resolutionMs)
{
    trace(lit("Discarding cached data"));
    Q_UNUSED(resolutionMs);
    m_loading.clear();
    m_loadedData.clear();
}

void QnFlatCameraDataLoader::updateServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;
}

int QnFlatCameraDataLoader::sendRequest(qint64 startTimeMs)
{
    if (!m_server)
    {
        trace(lit("There is no current server, cannot send request"));
        return 0;   //TODO: #GDM #bookmarks make sure invalid value is handled
    }

    auto connection = m_server->apiConnection();
    if (!connection)
    {
        trace(lit("There is no server api connection, cannot send request"));
        return 0;   //TODO: #GDM #bookmarks make sure invalid value is handled
    }

    QnChunksRequestData requestData;
    requestData.resList << m_resource.dynamicCast<QnVirtualCameraResource>();
    requestData.startTimeMs = startTimeMs;
    requestData.endTimeMs = DATETIME_NOW,   /* Always load data to the end. */
    requestData.filter = m_filter;
    requestData.periodsType = m_dataType;

    return connection->recordedTimePeriods(requestData, this, SLOT(at_timePeriodsReceived(int, const MultiServerPeriodDataList &, int)));
}

void QnFlatCameraDataLoader::at_timePeriodsReceived(int status, const MultiServerPeriodDataList &timePeriods, int requestHandle)
{
    trace(lit("Received answer for req %1").arg(requestHandle));

    std::vector<QnTimePeriodList> rawPeriods;

    for(auto period: timePeriods)
        rawPeriods.push_back(period.periods);
    QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(QnTimePeriodList::mergeTimePeriods(rawPeriods)));

    handleDataLoaded(status, data, requestHandle);
}

void QnFlatCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requestHandle)
{
    if (m_loading.handle != requestHandle)
        return;

    trace(lit("Loaded data for %1 (%2)").arg(m_loading.startTimeMs).arg(dt(m_loading.startTimeMs)));

    if (status != 0)
    {
        trace(lit("Load Failed"));
        for(auto handle: m_loading.waitingHandles)
            emit failed(status, handle);
        emit failed(status, requestHandle);
        m_loading.clear();
        return;
    }

    QnTimePeriod loadedPeriod(m_loading.startTimeMs, QnTimePeriod::infiniteDuration());

    if (data)
    {
        if (!m_loadedData)
        {
            m_loadedData = data;
            trace(lit("First dataset received, size %1").arg(data->dataSource().size()));
        }
        else if (!data->isEmpty())
        {
            trace(lit("New dataset received (size %1), merging with existing (size %2).")
                .arg(data->dataSource().size())
                .arg(m_loadedData->dataSource().size()));
            m_loadedData->update(data, loadedPeriod);
            trace(lit("Merging finished, size %1.").arg(m_loadedData->dataSource().size()));
        }
    }
    else
    {
        trace(lit("Empty data received"));
    }

    for(auto handle: m_loading.waitingHandles)
        emit ready(m_loadedData, loadedPeriod, handle);
    emit ready(m_loadedData, loadedPeriod, requestHandle);
    m_loading.clear();
}

void QnFlatCameraDataLoader::trace(const QString& message)
{
    if (m_dataType != Qn::RecordingContent)
        return;

    QString name = m_resource ? m_resource->getName() : lit("_invalid_camera_");
    NX_LOG(lit("Chunks: (%1) %2").arg(name).arg(message), cl_logDEBUG1);
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