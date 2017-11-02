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

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/log/log.h>

namespace {
    const qint64 requestIntervalMs = 30 * 1000;

    QString dt(qint64 time) {
        return QDateTime::fromMSecsSinceEpoch(time).toString(lit("MMM/dd/yyyy hh:mm:ss"));
    }
}

QnCachingCameraDataLoader::QnCachingCameraDataLoader(const QnMediaResourcePtr &resource,
    const QnMediaServerResourcePtr& server,
    QObject* parent)
    :
    base_type(parent),
    m_resource(resource),
    m_server(server)
{
    NX_ASSERT(supportedResource(resource), Q_FUNC_INFO, "Loaders must not be created for unsupported resources");
    init();
    initLoaders();

    QTimer* loadTimer = new QTimer(this);
    loadTimer->setInterval(requestIntervalMs / 10);  // time period will be loaded no often than once in 30 seconds, but timer should check it much more often
    loadTimer->setSingleShot(false);
    connect(loadTimer, &QTimer::timeout, this, [this]
    {
        trace(lit("Checking load by timer (enabled: %1)").arg(m_enabled));
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
    if(m_resource.dynamicCast<QnNetworkResource>()) {
        connect(qnSyncTime, &QnSyncTime::timeChanged,       this, &QnCachingCameraDataLoader::discardCachedData);
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
                [this, dataType](const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle)
                {
                    trace(lit("Handling reply %1 for period %2 (%3)")
                        .arg(handle)
                        .arg(updatedPeriod.startTimeMs)
                        .arg(dt(updatedPeriod.startTimeMs)),
                        dataType);

                	NX_ASSERT(updatedPeriod.isInfinite(), Q_FUNC_INFO, "We are always loading till very end.");
                    at_loader_ready(data, updatedPeriod.startTimeMs, dataType);
                });

            connect(loader, &QnAbstractCameraDataLoader::failed, this,
                [this, dataType]()
                {
                    NX_VERBOSE(this, lm("Chunks %1: failed to load")
                        .arg(dataType == Qn::RecordingContent ? lit("rec") : lit("mot")));
                    emit loadingFailed();
                });
        }
    }
}

void QnCachingCameraDataLoader::setEnabled(bool value) {
    if (m_enabled == value)
        return;

    trace(lit("Set loader enabled %1").arg(value));
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

void QnCachingCameraDataLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;

    if(!m_cameraChunks[Qn::MotionContent].isEmpty()) {
        m_cameraChunks[Qn::MotionContent].clear();
        emit periodsChanged(Qn::MotionContent);
    }
    updateTimePeriods(Qn::MotionContent, true);
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

bool QnCachingCameraDataLoader::loadInternal(Qn::TimePeriodContent periodType) {
    QnAbstractCameraDataLoaderPtr loader = m_loaders[periodType];
    NX_ASSERT(loader, Q_FUNC_INFO, "Loader must always exists");
    if(!loader)
    {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return false;
    }

    int handle = 0;
    switch (periodType)
    {
        case Qn::RecordingContent:
            handle = loader->load();
            break;
        case Qn::MotionContent:
            if(!isMotionRegionsEmpty())
            {
                QString filter = QString::fromUtf8(QJson::serialized(m_motionRegions));
                handle = loader->load(filter);
            }
            else if(!m_cameraChunks[Qn::MotionContent].isEmpty())
            {
                m_cameraChunks[Qn::MotionContent].clear();
                emit periodsChanged(Qn::MotionContent);
                return true;
            }
            break;
        default:
            Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
            break;
    }

    if (handle <= 0)
        trace(lit("Loading failed"), periodType);

    return handle > 0;
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(const QnAbstractCameraDataPtr &data, qint64 startTimeMs, Qn::TimePeriodContent dataType) {
    auto timePeriodType = (dataType);
    m_cameraChunks[timePeriodType] = data
        ? data->dataSource()
        : QnTimePeriodList();
    trace(lit("Received chunks update. Total size: %1 for period %2 (%3)")
        .arg(m_cameraChunks[timePeriodType].size())
        .arg(startTimeMs)
        .arg(dt(startTimeMs)),
        dataType);
    emit periodsChanged(timePeriodType, startTimeMs);

}

void QnCachingCameraDataLoader::discardCachedData()
{
    NX_VERBOSE(this) << "Chunks: clear local cache";
    for (int i = 0; i < Qn::TimePeriodContentCount; i++) {

        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        trace(lit("discardCachedData()"), timePeriodType);
        if (m_loaders[i])
            m_loaders[i]->discardCachedData();

        m_cameraChunks[timePeriodType].clear();
        if (m_enabled) {
            updateTimePeriods(timePeriodType, true);
            emit periodsChanged(timePeriodType);
        }
    }

}

void QnCachingCameraDataLoader::updateTimePeriods(Qn::TimePeriodContent periodType, bool forced) {
    // TODO: #GDM #2.4 make sure we are not sending requests while loader is disabled
    if (forced || m_previousRequestTime[periodType].hasExpired(requestIntervalMs))
    {
        if (forced)
            trace(lit("updateTimePeriods(forced)"), periodType);
        else
            trace(lit("updateTimePeriods(by timer)"), periodType);

        if (loadInternal(periodType))
            m_previousRequestTime[periodType].restart();
    }
}

void QnCachingCameraDataLoader::trace(const QString& message, Qn::TimePeriodContent periodType)
{
    if (m_resource)
    {
        NX_VERBOSE(this, lm("Chunks (cached) %1: (%2) %3")
            .arg(periodType == Qn::RecordingContent ? lit("rec") : lit("mot"))
            .arg(m_resource->toResourcePtr()->getName())
            .arg(message));
    }
}
