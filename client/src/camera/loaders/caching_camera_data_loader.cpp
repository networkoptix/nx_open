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
    const qint64 minTimePeriodLoadingMargin = 60 * 60 * 1000; /* 1 hour. */
    const qint64 defaultUpdateInterval = 10 * 1000;
}

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

QnTimePeriod QnCachingCameraDataLoader::boundingPeriod() const {
    return m_boundingPeriod;
}

void QnCachingCameraDataLoader::setBoundingPeriod(const QnTimePeriod &boundingPeriod) {
    m_boundingPeriod = boundingPeriod;
}

QnTimePeriod QnCachingCameraDataLoader::targetPeriod(Qn::CameraDataType dataType) const {
    return m_targetPeriod[dataType];
}

void QnCachingCameraDataLoader::setTargetPeriod(const QnTimePeriod &targetPeriod, Qn::CameraDataType dataType) {
    m_targetPeriod[dataType] = targetPeriod;

    switch (dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod:
    case Qn::BookmarkTimePeriod:
        updateTimePeriods(dataType);
        break;
    case Qn::BookmarkData:
        updateBookmarks();
        break;
    default:
        assert(false); //should never get here
        break;
    }
}

const QList<QRegion> &QnCachingCameraDataLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingCameraDataLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    m_requestedTimePeriods[Qn::MotionContent].clear();
    updateTimePeriods(Qn::MotionTimePeriod);
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

QnTimePeriod QnCachingCameraDataLoader::addLoadingMargins(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod, const qint64 minMargin) const {
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    qint64 startTime = targetPeriod.startTimeMs;
    qint64 endTime = targetPeriod.durationMs == -1 ? currentTime : targetPeriod.startTimeMs + targetPeriod.durationMs;

    qint64 minStartTime = boundingPeriod.startTimeMs;
    qint64 maxEndTime = boundingPeriod.durationMs == -1 ? currentTime : boundingPeriod.startTimeMs + boundingPeriod.durationMs;

    /* Adjust for margin. */
    qint64 margin = qMax(minMargin, static_cast<qint64>((endTime - startTime) * m_loadingMargin));
    
    startTime = qMax(startTime - margin, minStartTime);
    endTime = qMin(endTime + margin, maxEndTime + m_updateInterval);

    return QnTimePeriod(startTime, endTime - startTime);
}

void QnCachingCameraDataLoader::load(Qn::CameraDataType type, const QnTimePeriod &targetPeriod) {
    QnAbstractCameraDataLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (type) {
    case Qn::RecordedTimePeriod:
        m_handles[type] = loader->load(targetPeriod);
        break;
    case Qn::BookmarkTimePeriod:
        m_handles[type] = loader->load(targetPeriod, m_bookmarkTags.join(L','));  //TODO: #GDM process tags list on the server side
        break;
    case Qn::MotionTimePeriod:
        if(!isMotionRegionsEmpty()) {
            QString filter = serializeRegionList(m_motionRegions);
            m_handles[type] = loader->load(targetPeriod, filter);
        } else if(m_data[type] && !m_data[type]->isEmpty()) {
            m_data[type]->clear();
            emit periodsChanged(Qn::MotionContent);
        }
        break;
    case Qn::BookmarkData: 
        { 
            qint64 resolutionMs = bookmarkResolution(targetPeriod.durationMs);
            qDebug() << "Caching: requesting bookmarks with resolution" << resolutionMs;
            m_handles[type] = loader->load(targetPeriod, m_bookmarkTags.join(L','), resolutionMs);
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
    for(int i = 0; i < Qn::CameraDataTypeCount; i++)
        m_loaders[i]->discardCachedData();

    for (int i = 0; i < Qn::TimePeriodContentCount; ++i) {
        Qn::TimePeriodContent timePeriod = static_cast<Qn::TimePeriodContent>(i);
        m_requestedTimePeriods[timePeriod].clear();
        updateTimePeriods(timePeriodToDataType(timePeriod));
    }

    m_requestedBookmarkPeriodsByResolution.clear();
    updateBookmarks();
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
    m_requestedBookmarkPeriodsByResolution.clear();
    updateBookmarks();
}

qint64 QnCachingCameraDataLoader::bookmarkResolution(qint64 periodDuration) const {
    static const std::vector<qint64> steps = [](){ 
        std::vector<qint64> result;
        for (int i = 0; i < 40; ++i)
            result.push_back(1ll << i);
        return result; 
    }();

    const int maxPeriodsPerTimeline = 8;
    qint64 maxPeriodLength = periodDuration / maxPeriodsPerTimeline;
    auto step = std::lower_bound(steps.cbegin(), steps.cend(), maxPeriodLength);
    return *step;
}

void QnCachingCameraDataLoader::updateTimePeriods(Qn::CameraDataType dataType) {
    QnTimePeriodList& requestedPeriods = m_requestedTimePeriods[dataTypeToPeriod(dataType)];
    if (requestedPeriods.containPeriod(m_targetPeriod[dataType]))
        return;

    QnTimePeriod requestedPeriod = addLoadingMargins(m_targetPeriod[dataType], m_boundingPeriod, minTimePeriodLoadingMargin);
    requestedPeriods = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << requestedPeriods << QnTimePeriodList(requestedPeriod));
    load(dataType, requestedPeriod);
}

void QnCachingCameraDataLoader::updateBookmarks() {
    qint64 step = bookmarkResolution(m_targetPeriod[Qn::BookmarkData].durationMs);
    QnTimePeriodList& requestedPeriods = m_requestedBookmarkPeriodsByResolution[step];
    if (requestedPeriods.containPeriod(m_targetPeriod[Qn::BookmarkData]))
        return;

    QnTimePeriod requestedPeriod = addLoadingMargins(m_targetPeriod[Qn::BookmarkData], m_boundingPeriod, step * 2);
    requestedPeriods = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << requestedPeriods << QnTimePeriodList(requestedPeriod));
    load(Qn::BookmarkData, requestedPeriod);
}
