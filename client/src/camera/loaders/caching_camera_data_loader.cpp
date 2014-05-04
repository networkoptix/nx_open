#include "caching_camera_data_loader.h"

#include <QtCore/QMetaType>

#include <api/serializer/serializer.h>

#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/common/model_functions.h>

#include <camera/loaders/multi_server_camera_data_loader.h>
#include <camera/loaders/layout_file_camera_data_loader.h>

namespace {
    const qint64 minLoadingMargin = 60 * 60 * 1000; /* 1 hour. */
    const qint64 defaultUpdateInterval = 10 * 1000;
}

//TODO: #GDM integrate with timeSlider steps
class QnBookmarkStepStorage {
public:
    QnBookmarkStepStorage() {
        m_steps <<  
            1ll *                               10 <<
            1ll *                               50 <<
            1ll *                               100 <<
            1ll *                               500 <<
            1000ll *                            1 <<
            1000ll *                            5 <<
            1000ll *                            10 <<
            1000ll *                            30 <<
            1000ll * 60 *                       1 <<
            1000ll * 60 *                       5 <<
            1000ll * 60 *                       10 <<
            1000ll * 60 *                       30<<
            1000ll * 60 * 60 *                  1 <<
            1000ll * 60 * 60 *                  3 <<
            1000ll * 60 * 60 *                  12 <<
            1000ll * 60 * 60 * 24 *             1 <<
            1000ll * 60 * 60 * 24 * 31 *        1 <<
            1000ll * 60 * 60 * 24 * 365 *       1 <<
            1000ll * 60 * 60 * 24 * 365 *       5 <<
            1000ll * 60 * 60 * 24 * 365 *       10;
    }

    const QVector<qint64> &steps() const {
        return m_steps;
    }


private:
    QVector<qint64> m_steps;
};

Q_GLOBAL_STATIC(QnBookmarkStepStorage, bookmarkSteps);

QnCachingCameraDataLoader::QnCachingCameraDataLoader(const QnResourcePtr &resource, QObject *parent):
    QObject(parent),
    m_resource(resource)
{
    init();

    if(!m_resource) {
        qnNullWarning(m_resource);
    } else {
        if(createLoaders(m_resource, m_loaders)) {
            initLoaders(m_loaders);
        } else {
            qnWarning("Could not create time period loader for resource '%1'.", m_resource->getName());
        }
    }
}

QnCachingCameraDataLoader::QnCachingCameraDataLoader(QnAbstractCameraDataLoader **loaders, QObject *parent):
    QObject(parent),
    m_resource(loaders[0]->resource())
{
    init();
    initLoaders(loaders);
}

QnCachingCameraDataLoader::~QnCachingCameraDataLoader() {
    return;
}

void QnCachingCameraDataLoader::init() {
    m_loadingMargin = 1.0;
    m_updateInterval = defaultUpdateInterval;
    m_resourceIsLocal = !m_resource.dynamicCast<QnNetworkResource>();

    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        m_handles[i] = -1;
        m_loaders[i] = NULL;
    }

    if(!m_resourceIsLocal)
        connect(qnSyncTime, SIGNAL(timeChanged()), this, SLOT(at_syncTime_timeChanged()));
}

void QnCachingCameraDataLoader::initLoaders(QnAbstractCameraDataLoader **loaders) {
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        QnAbstractCameraDataLoader *loader = loaders[i];
        m_loaders[i] = loader;

        if(loader) {
            loader->setParent(this);

            connect(loader, &QnAbstractCameraDataLoader::ready,     this,   &QnCachingCameraDataLoader::at_loader_ready);
            connect(loader, &QnAbstractCameraDataLoader::failed,    this,   &QnCachingCameraDataLoader::at_loader_failed);
        }
    }
}

bool QnCachingCameraDataLoader::createLoaders(const QnResourcePtr &resource, QnAbstractCameraDataLoader **loaders) 
{
    for(int i = 0; i < Qn::CameraDataTypeCount; i++)
        loaders[i] = NULL;

    bool success = true;
    bool isNetRes = resource.dynamicCast<QnNetworkResource>();
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) 
    {
        Qn::CameraDataType type = static_cast<Qn::CameraDataType>(i);
        if (isNetRes)
            loaders[i] = QnMultiServerCameraDataLoader::newInstance(resource, type);
        else
            loaders[i] = QnLayoutFileCameraDataLoader::newInstance(resource, type);
        if(!loaders[i]) {
            success = false;
            break;
        }
    }

    if(success)
        return true;

    for (int i = 0; i < Qn::CameraDataTypeCount; i++) {
        delete loaders[i];
        loaders[i] = NULL;
    }
    return false;
}

QnCachingCameraDataLoader *QnCachingCameraDataLoader::newInstance(const QnResourcePtr &resource, QObject *parent) 
{
    QnAbstractCameraDataLoader *loaders[Qn::CameraDataTypeCount];
    if(createLoaders(resource, loaders)) {
        return new QnCachingCameraDataLoader(loaders, parent);
    } else {
        return NULL;
    }
}

QnResourcePtr QnCachingCameraDataLoader::resource() const {
    return m_resource;
}

qreal QnCachingCameraDataLoader::loadingMargin() const {
    return m_loadingMargin;
}

void QnCachingCameraDataLoader::setLoadingMargin(qreal loadingMargin) {
    m_loadingMargin = loadingMargin;
}

qint64 QnCachingCameraDataLoader::updateInterval() const {
    return m_updateInterval;
}

void QnCachingCameraDataLoader::setUpdateInterval(qint64 msecs) {
    m_updateInterval = msecs;
}

const QnTimePeriod &QnCachingCameraDataLoader::loadedPeriod() const {
    return m_loadedPeriod;
}

void QnCachingCameraDataLoader::setTargetPeriods(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod) {
    //TODO: #GDM this logic does not work for bookmarks, we need update them if target period differs from loaded depending on resolution (at least half a step further).
    if(m_loadedPeriod.contains(targetPeriod)) 
        return;
    
    m_loadedPeriod = addLoadingMargins(targetPeriod, boundingPeriod);
    for(int i = 0; i < Qn::CameraDataTypeCount; i++)
        load(static_cast<Qn::CameraDataType>(i));
}

const QList<QRegion> &QnCachingCameraDataLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingCameraDataLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    load(Qn::MotionTimePeriod);
}

bool QnCachingCameraDataLoader::isMotionRegionsEmpty() const {
    foreach(const QRegion &region, m_motionRegions)
        if(!region.isEmpty())
            return false;
    return true;
}

QnTimePeriodList QnCachingCameraDataLoader::periods(Qn::TimePeriodContent timePeriodType) {
    if (QnTimePeriodCameraData* list = dynamic_cast<QnTimePeriodCameraData*>(m_data[timePeriodToDataType(timePeriodType)].data()))
        return list->dataSource();
    return QnTimePeriodList();
}

QnTimePeriod QnCachingCameraDataLoader::addLoadingMargins(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod) const {
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    qint64 startTime = targetPeriod.startTimeMs;
    qint64 endTime = targetPeriod.durationMs == -1 ? currentTime : targetPeriod.startTimeMs + targetPeriod.durationMs;

    qint64 minStartTime = boundingPeriod.startTimeMs;
    qint64 maxEndTime = boundingPeriod.durationMs == -1 ? currentTime : boundingPeriod.startTimeMs + boundingPeriod.durationMs;

    /* Adjust for margin. */
    qint64 margin = qMax(minLoadingMargin, static_cast<qint64>((endTime - startTime) * m_loadingMargin));
    
    startTime = qMax(startTime - margin, minStartTime);
    endTime = qMin(endTime + margin, maxEndTime + m_updateInterval);

    return QnTimePeriod(startTime, endTime - startTime);
}

void QnCachingCameraDataLoader::load(Qn::CameraDataType type) {
    QnAbstractCameraDataLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (type) {
    case Qn::RecordedTimePeriod:
        m_handles[type] = loader->load(m_loadedPeriod);
        break;
    case Qn::BookmarkTimePeriod:
        m_handles[type] = loader->load(m_loadedPeriod, m_bookmarkTags.join(L','));  //TODO: #GDM process tags list on the server side
        break;
    case Qn::MotionTimePeriod:
        if(!isMotionRegionsEmpty()) {
            QString filter = serializeRegionList(m_motionRegions);
            m_handles[type] = loader->load(m_loadedPeriod, filter);
        } else if(m_data[type] && !m_data[type]->isEmpty()) {
            m_data[type]->clear();
            emit periodsChanged(Qn::MotionContent);
        }
        break;
    case Qn::BookmarkData: 
        { 
            qint64 resolutionMs = bookmarkResolution(m_loadedPeriod.durationMs);
            qDebug() << "Caching: requesting bookmarks with resolution" << resolutionMs;
            m_handles[type] = loader->load(m_loadedPeriod, m_bookmarkTags.join(L','), resolutionMs);
            break;
        }
    default:
        assert(false); //should never get here
    }
}

bool QnCachingCameraDataLoader::trim(Qn::CameraDataType type, qint64 trimTime) {
    if (!m_data[type])
        return false;

    return m_data[type]->trimDataSource(trimTime);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(const QnAbstractCameraDataPtr &data, int handle) {
    for (int i = 0; i < Qn::CameraDataTypeCount; ++i) {
        Qn::CameraDataType dataType = static_cast<Qn::CameraDataType>(i);
        if (handle != m_handles[dataType])
            continue;

        m_handles[dataType] = -1;
        if (m_data[dataType] && m_data[dataType].data() == data)
            return;

        m_data[dataType] = data;
        switch (dataType) {
        case Qn::RecordedTimePeriod:
        case Qn::MotionTimePeriod:
        case Qn::BookmarkTimePeriod:
            emit periodsChanged(dataTypeToPeriod(dataType));
            break;
        case Qn::BookmarkData:
            emit bookmarksChanged();
            break;
        default:
            break;
        }
        return;
    }
}

void QnCachingCameraDataLoader::at_loader_failed(int /*status*/, int handle) {
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        Qn::CameraDataType dataType = static_cast<Qn::CameraDataType>(i);
        if (handle != m_handles[dataType])
            continue;

        m_handles[i] = -1;
        emit loadingFailed();

        switch (dataType) {
        case Qn::RecordedTimePeriod:
        case Qn::MotionTimePeriod:
        case Qn::BookmarkTimePeriod:
            // live time periods should be trimmed to fixed time
            if (trim(dataType, qnSyncTime->currentMSecsSinceEpoch())) // why are we trimming this by the error reply time?
                emit periodsChanged(dataTypeToPeriod(dataType));
            break;
        default:
            break;
        }
        return;
    }
}

void QnCachingCameraDataLoader::at_syncTime_timeChanged() {
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        m_loaders[i]->discardCachedData();
        load(static_cast<Qn::CameraDataType>(i));
    }
}

void QnCachingCameraDataLoader::addBookmark(const QnCameraBookmark &bookmark) {
    throw std::logic_error("The method or operation is not implemented.");
}


QnCameraBookmark QnCachingCameraDataLoader::bookmarkByTime(qint64 position) const {
    throw std::logic_error("The method or operation is not implemented.");
}

QnCameraBookmarkTags QnCachingCameraDataLoader::bookmarkTags() const {
    return m_bookmarkTags;
}

void QnCachingCameraDataLoader::setBookmarkTags(const QnCameraBookmarkTags &tags) {
    if (m_bookmarkTags == tags)
        return;

    m_bookmarkTags = tags;
    load(Qn::BookmarkData);
}

qint64 QnCachingCameraDataLoader::bookmarkResolution(qint64 periodDuration) const {
    //TODO: #GDM integrate with timeSlider steps

    const int maxPeriodsPerTimeline = 8;
    qint64 maxPeriodLength = periodDuration / maxPeriodsPerTimeline;

    auto step = std::lower_bound(bookmarkSteps->steps().begin(), bookmarkSteps->steps().end(), maxPeriodLength);
    return *step;
}
