#include "flat_camera_data_loader.h"

#include <api/helpers/chunks_request_data.h>

#include <camera/data/abstract_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <recording/time_period_list.h>

#include <utils/common/warnings.h>

//#define QN_FLAT_CAMERA_DATA_LOADER_DEBUG

namespace {
    /** Fake handle for simultaneous load request. Initial value is big enough to not conflict with real request handles. */
    QAtomicInt qn_fakeHandle(INT_MAX / 2);

    /** Minimum time (in milliseconds) for overlapping time periods requests.  */
    const int minOverlapDuration = 120*1000;

    /** Detail level for data loading - most precise value is used. */
    const int detailLevel = 1;
}

QnFlatCameraDataLoader::QnFlatCameraDataLoader(const QnMediaServerConnectionPtr &connection, const QnNetworkResourcePtr &camera, Qn::CameraDataType dataType, QObject *parent):
    QnAbstractCameraDataLoader(camera, dataType, parent),
    m_connection(connection)
{
    if(!connection)
        qnNullWarning(connection);

    if(!camera)
        qnNullWarning(camera);
}

QnFlatCameraDataLoader *QnFlatCameraDataLoader::newInstance(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera,  Qn::CameraDataType dataType, QObject *parent) {
    if (!server || !camera)
        return NULL;
    
    QnMediaServerConnectionPtr serverConnection = server->apiConnection();
    if (!serverConnection)
        return NULL;

    return new QnFlatCameraDataLoader(serverConnection, camera, dataType, parent);
}

int QnFlatCameraDataLoader::load(const QString &filter, const qint64 resolutionMs) {
    Q_ASSERT_X(resolutionMs == detailLevel, Q_FUNC_INFO, "this loader is supposed to load only flat data");
    Q_UNUSED(resolutionMs);

    if (filter != m_filter)
        discardCachedData();
    m_filter = filter;

    /* Check whether data is currently being loaded. */
    if (m_loading.handle > 0) {
#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
        qDebug() << "QnFlatCameraDataLoader::" << "data is already being loaded";
#endif
        auto handle = qn_fakeHandle.fetchAndAddAcquire(1);
        m_loading.waitingHandles << handle;
        return handle;
    }

    /* We need to load all data after the already loaded piece, assuming there were no periods before already loaded. */
    qint64 startTimeMs = 0;
    if (m_loadedData && !m_loadedData->dataSource().isEmpty()) {
        auto last = m_loadedData->dataSource().last();
        if (last.isInfinite())
            startTimeMs = last.startTimeMs;
        else
            startTimeMs = last.endTimeMs() - minOverlapDuration;
    }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "loading period from" << startTimeMs;
#endif

    m_loading.clear(); /* Just in case. */
    m_loading.startTimeMs = startTimeMs;
    m_loading.handle = sendRequest(startTimeMs);
    return m_loading.handle;
}

void QnFlatCameraDataLoader::discardCachedData(const qint64 resolutionMs) {
#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "discarding cached data";
#endif

    Q_UNUSED(resolutionMs);
    m_loading.clear();
    m_loadedData.clear();
}

int QnFlatCameraDataLoader::sendRequest(qint64 startTimeMs) {
    Q_ASSERT_X(m_dataType != Qn::BookmarkData, Q_FUNC_INFO, "this loader should NOT be used to load bookmarks");

    QnChunksRequestData requestData;
    requestData.resList << m_resource.dynamicCast<QnVirtualCameraResource>();
    requestData.startTimeMs = startTimeMs;
    requestData.endTimeMs = DATETIME_NOW,   /* Always load data to the end. */ 
    requestData.filter = m_filter;
    requestData.periodsType = dataTypeToPeriod(m_dataType);

    return m_connection->recordedTimePeriods(requestData, this, SLOT(at_timePeriodsReceived(int, const MultiServerPeriodDataList &, int)));
}

void QnFlatCameraDataLoader::at_timePeriodsReceived(int status, const MultiServerPeriodDataList &timePeriods, int requestHandle) {

    std::vector<QnTimePeriodList> rawPeriods;

    for(auto period: timePeriods)
        rawPeriods.push_back(period.periods);
    QnAbstractCameraDataPtr data(new QnTimePeriodCameraData(QnTimePeriodList::mergeTimePeriods(rawPeriods)));

    handleDataLoaded(status, data, requestHandle);
}

void QnFlatCameraDataLoader::handleDataLoaded(int status, const QnAbstractCameraDataPtr &data, int requestHandle) {
    if (m_loading.handle != requestHandle)
        return;

    if (status != 0) {
        for(auto handle: m_loading.waitingHandles)
            emit failed(status, handle);
        emit failed(status, requestHandle);
        m_loading.clear();
        return;
    }

#ifdef QN_FLAT_CAMERA_DATA_LOADER_DEBUG
    qDebug() << "QnFlatCameraDataLoader::" << "loaded data for" << m_loading.startTimeMs;
#endif

    QnTimePeriod loadedPeriod(m_loading.startTimeMs, QnTimePeriod::infiniteDuration());

    if (data) {
        if (!m_loadedData) {
            m_loadedData = data;
        }
        else if (!data->isEmpty()) {
            m_loadedData->update(data, loadedPeriod);
        }
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