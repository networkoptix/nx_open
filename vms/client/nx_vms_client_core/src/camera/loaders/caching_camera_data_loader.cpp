#include "caching_camera_data_loader.h"

#include <QtCore/QMetaType>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/avi/avi_resource.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <nx/fusion/model_functions.h>

#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/loaders/layout_file_camera_data_loader.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>

namespace {

const qint64 requestIntervalMs = 30 * 1000;

} // namespace

QnCachingCameraDataLoader::QnCachingCameraDataLoader(const QnMediaResourcePtr &resource,
    const QnMediaServerResourcePtr& server,
    QObject* parent)
    :
    base_type(parent),
    m_resource(resource),
    m_server(server)
{
    NX_ASSERT(supportedResource(resource),
        "Loaders must not be created for unsupported resources");
    init();
    initLoaders();

    m_analyticsFilter.deviceIds.push_back(m_resource->toResource()->getId());

    QTimer* loadTimer = new QTimer(this);
    // Time period will be loaded no often than once in 30 seconds,
    // but timer should check it much more often.
    loadTimer->setInterval(requestIntervalMs / 10);
    loadTimer->setSingleShot(false);
    connect(loadTimer, &QTimer::timeout, this, [this]
    {
        NX_VERBOSE(this, "Checking load by timer (enabled: %1)", m_enabled);
        if (m_enabled)
            load();
    });
    loadTimer->start();
    load(true);
}

QnCachingCameraDataLoader::~QnCachingCameraDataLoader() {
}

bool QnCachingCameraDataLoader::supportedResource(const QnMediaResourcePtr &resource) {
    bool result = !resource.dynamicCast<QnVirtualCameraResource>().isNull();
    result |= !resource.dynamicCast<QnAviResource>().isNull();
    return result;
}

void QnCachingCameraDataLoader::init() {
    // TODO: #GDM 2.4 move to camera history
    if (m_resource.dynamicCast<QnNetworkResource>())
    {
        connect(qnSyncTime, &QnSyncTime::timeChanged,
                this, &QnCachingCameraDataLoader::discardCachedData);
    }
}

void QnCachingCameraDataLoader::initLoaders() {
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    QnAviResourcePtr aviFile = m_resource.dynamicCast<QnAviResource>();

    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        Qn::TimePeriodContent dataType = static_cast<Qn::TimePeriodContent>(i);
        QnAbstractCameraDataLoader* loader = NULL;

        if (camera)
            loader = new QnFlatCameraDataLoader(camera, m_server, dataType);
        else if (aviFile)
            loader = new QnLayoutFileCameraDataLoader(aviFile, dataType);

        m_loaders[i].reset(loader);

        if (loader) {
            connect(loader, &QnAbstractCameraDataLoader::ready, this,
                [this, dataType]
                (const QnAbstractCameraDataPtr &data,
                    const QnTimePeriod &updatedPeriod, int handle)
                {
                    NX_VERBOSE(this, "Handling %1 reply %2 for period %3 (%4)",
                        dataType,
                        handle,
                        updatedPeriod.startTimeMs,
                        nx::utils::timestampToDebugString(updatedPeriod.startTimeMs));

                    NX_ASSERT(updatedPeriod.isInfinite(), "We are always loading till very end.");
                    at_loader_ready(data, updatedPeriod.startTimeMs, dataType);
                });

            connect(loader, &QnAbstractCameraDataLoader::failed, this,
                [this, dataType]()
                {
                    NX_VERBOSE(this, "Chunks %1: failed to load", dataType);
                    emit loadingFailed();
                });
        }
    }
}

void QnCachingCameraDataLoader::setEnabled(bool value)
{
    if (m_enabled == value)
        return;

    NX_VERBOSE(this, "Set loader enabled %1", value);
    m_enabled = value;
    if (value)
        load(true);
}

bool QnCachingCameraDataLoader::enabled() const {
    return m_enabled;
}

void QnCachingCameraDataLoader::updateServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;

    for (auto loader: m_loaders)
    {
        if (auto remoteLoader = dynamic_cast<QnFlatCameraDataLoader*>(loader.data()))
            remoteLoader->updateServer(server);
    }
}

QString QnCachingCameraDataLoader::idForToStringFromPtr() const
{
    return m_resource->toResourcePtr()->getName();
}

QnMediaResourcePtr QnCachingCameraDataLoader::resource() const
{
    return m_resource;
}

void QnCachingCameraDataLoader::load(bool forced) {
    for (int i = 0; i < Qn::TimePeriodContentCount; ++i) {
        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        updateTimePeriods(timePeriodType, forced);
    }
}


const QList<QRegion> &QnCachingCameraDataLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingCameraDataLoader::setMotionRegions(const QList<QRegion>& motionRegions)
{
    if (m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    discardCachedDataType(Qn::MotionContent);
}

const QnCachingCameraDataLoader::AnalyticsFilter& QnCachingCameraDataLoader::analyticsFilter() const
{
    return m_analyticsFilter;
}

void QnCachingCameraDataLoader::setAnalyticsFilter(const AnalyticsFilter& value)
{
    if (m_analyticsFilter == value)
        return;

    const auto deviceId = m_analyticsFilter.deviceIds.empty()
        ? QnUuid()
        : m_analyticsFilter.deviceIds.front();

    NX_ASSERT(value.deviceIds == std::vector<QnUuid>{deviceId});

    m_analyticsFilter = value;

    if (m_analyticsFilter.deviceIds != std::vector<QnUuid>{deviceId})
        m_analyticsFilter.deviceIds = std::vector<QnUuid>{deviceId}; //< Just for safety.

    discardCachedDataType(Qn::AnalyticsContent);
}

bool QnCachingCameraDataLoader::isMotionRegionsEmpty() const {
    foreach(const QRegion &region, m_motionRegions)
        if(!region.isEmpty())
            return false;
    return true;
}

QnTimePeriodList QnCachingCameraDataLoader::periods(Qn::TimePeriodContent timePeriodType) const {
    return m_cameraChunks[timePeriodType];
}

bool QnCachingCameraDataLoader::loadInternal(Qn::TimePeriodContent periodType)
{
    QnAbstractCameraDataLoaderPtr loader = m_loaders[periodType];
    NX_ASSERT(loader, "Loader must always exists");

    if (!loader)
    {
        emit loadingFailed();
        return false;
    }

    int handle = 0;
    switch (periodType)
    {
        case Qn::RecordingContent:
        {
            handle = loader->load();
            break;
        }

        case Qn::MotionContent:
        {
            const auto filter = isMotionRegionsEmpty()
                ? QString() //< Treat empty selection as whole area selection.
                : QJson::serialized(m_motionRegions);

            handle = loader->load(filter);
            break;
        }

        case Qn::AnalyticsContent:
        {
            // Minimum required value to provide continuous display on the timeline.
            const qint64 analyticsDetailLevelMs = 1000;
            const auto filter = QString::fromUtf8(QJson::serialized(m_analyticsFilter));
            handle = loader->load(filter, analyticsDetailLevelMs);
            break;
        }

        default:
        {
            NX_ASSERT(false, "Should never get here");
            break;
        }
    }

    if (handle <= 0)
        NX_VERBOSE(this, "Loading of %1 failed", periodType);

    return handle > 0;
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(
    const QnAbstractCameraDataPtr& data,
    qint64 startTimeMs,
    Qn::TimePeriodContent dataType)
{
    m_cameraChunks[dataType] = data
        ? data->dataSource()
        : QnTimePeriodList();
    NX_VERBOSE(this, "Received %1 chunks update. Total size: %2 for period %3 (%4)",
        dataType,
        m_cameraChunks[dataType].size(),
        startTimeMs,
        nx::utils::timestampToDebugString(startTimeMs));
    emit periodsChanged(dataType, startTimeMs);
}

void QnCachingCameraDataLoader::invalidateCachedData()
{
    NX_VERBOSE(this, "Mark local cache as dirty");

    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
    {
        if (auto loader = m_loaders[i])
            loader->discardCachedData();
    }
}

void QnCachingCameraDataLoader::discardCachedDataType(Qn::TimePeriodContent type)
{
    NX_VERBOSE(this, "Discard cached %1 chunks", type);
    if (auto loader = m_loaders[type])
        loader->discardCachedData();

    m_cameraChunks[type].clear();
    if (m_enabled)
    {
        updateTimePeriods(type, true);
        emit periodsChanged(type);
    }
}

void QnCachingCameraDataLoader::discardCachedData()
{
    NX_VERBOSE(this, "Discard all cached data");
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        discardCachedDataType(Qn::TimePeriodContent(i));
}

void QnCachingCameraDataLoader::updateTimePeriods(Qn::TimePeriodContent periodType, bool forced) {
    // TODO: #GDM #2.4 make sure we are not sending requests while loader is disabled
    if (forced || m_previousRequestTime[periodType].hasExpired(requestIntervalMs))
    {
        if (forced)
            NX_VERBOSE(this, "updateTimePeriods %1 (forced)", periodType);
        else
            NX_VERBOSE(this, "updateTimePeriods %1 (by timer)", periodType);

        if (loadInternal(periodType))
            m_previousRequestTime[periodType].restart();
    }
}
