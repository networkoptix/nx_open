#include "caching_camera_data_loader.h"

#include <QtCore/QMetaType>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <plugins/resource/avi/avi_resource.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/common/model_functions.h>

#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/loaders/layout_file_camera_data_loader.h>

#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>

namespace {
    const qint64 requestIntervalMs = 30 * 1000;
}

QnCachingCameraDataLoader::QnCachingCameraDataLoader(const QnMediaResourcePtr &resource, QObject *parent):
    base_type(parent),
    m_enabled(true),
    m_resource(resource)
{
    Q_ASSERT_X(supportedResource(resource), Q_FUNC_INFO, "Loaders must not be created for unsupported resources");
    init();
    initLoaders();

    QTimer* loadTimer = new QTimer(this);
    loadTimer->setInterval(requestIntervalMs / 10);  // time period will be loaded no often than once in 30 seconds, but timer should check it much more often
    loadTimer->setSingleShot(false);
    connect(loadTimer, &QTimer::timeout, this, [this] {
        if (m_enabled)
            load();
    });
    loadTimer->start();
    load(true);
}

QnCachingCameraDataLoader::~QnCachingCameraDataLoader() {
}

bool QnCachingCameraDataLoader::supportedResource(const QnMediaResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    QnAviResourcePtr aviFile = resource.dynamicCast<QnAviResource>();
    return camera || aviFile;
}

void QnCachingCameraDataLoader::init() {
    //TODO: #GDM 2.4 move to camera history
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
            loader = new QnFlatCameraDataLoader(camera, dataType);
        else if (aviFile) {
            loader = new QnLayoutFileCameraDataLoader(aviFile, dataType);
        }

        m_loaders[i].reset(loader);

        if (loader) {           
            connect(loader, &QnAbstractCameraDataLoader::ready,         this,  [this, dataType](const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle){ 
                Q_UNUSED(handle);
                Q_ASSERT_X(updatedPeriod.isInfinite(), Q_FUNC_INFO, "We are always loading till very end.");
                at_loader_ready(data, updatedPeriod.startTimeMs, dataType);
            });

            connect(loader, &QnAbstractCameraDataLoader::failed,        this,  &QnCachingCameraDataLoader::loadingFailed);
        }
    }
}

void QnCachingCameraDataLoader::setEnabled(bool value) {
    if (m_enabled == value)
        return;
    m_enabled = value;
    if (value)
        load(true);
}

bool QnCachingCameraDataLoader::enabled() const {
    return m_enabled;
}

QnMediaResourcePtr QnCachingCameraDataLoader::resource() const {
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

void QnCachingCameraDataLoader::loadInternal(Qn::TimePeriodContent periodType) {
    QnAbstractCameraDataLoaderPtr loader = m_loaders[periodType];
    Q_ASSERT_X(loader, Q_FUNC_INFO, "Loader must always exists");
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (periodType) {
    case Qn::RecordingContent:
    case Qn::BookmarksContent:
        loader->load();
        break;
    case Qn::MotionContent:
        if(!isMotionRegionsEmpty()) {
            QString filter = QString::fromUtf8(QJson::serialized(m_motionRegions));
            loader->load(filter);
        } else if(!m_cameraChunks[Qn::MotionContent].isEmpty()) {
            m_cameraChunks[Qn::MotionContent].clear();
            emit periodsChanged(Qn::MotionContent);
        }
        break;
    default:
        Q_ASSERT_X(false, Q_FUNC_INFO, "Should never get here");
        break;
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(const QnAbstractCameraDataPtr &data, qint64 startTimeMs, Qn::TimePeriodContent dataType) {
    auto timePeriodType = (dataType);
    m_cameraChunks[timePeriodType] = data->dataSource();
    emit periodsChanged(timePeriodType, startTimeMs);

}

void QnCachingCameraDataLoader::discardCachedData() {
    for (int i = 0; i < Qn::TimePeriodContentCount; i++) {

        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);

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
    //TODO: #GDM #2.4 make sure we are not sending requests while loader is disabled
    if (forced || m_previousRequestTime[periodType].hasExpired(requestIntervalMs)) {
        loadInternal(periodType);
        m_previousRequestTime[periodType].restart();
    }
}
