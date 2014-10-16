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
    base_type(parent),
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
    base_type(parent),
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
        m_loaders[i] = NULL;
    }

    if(!m_resourceIsLocal) {
        connect(qnSyncTime, &QnSyncTime::timeChanged,       this, &QnCachingCameraDataLoader::discardCachedData);
    }
}

void QnCachingCameraDataLoader::initLoaders(QnAbstractCameraDataLoader **loaders) {
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        Qn::CameraDataType dataType = static_cast<Qn::CameraDataType>(i);
        QnAbstractCameraDataLoader *loader = loaders[i];
        m_loaders[i] = loader;

        if(loader) {
            loader->setParent(this);

            connect(loader, &QnAbstractCameraDataLoader::ready,         this,  [this, dataType](const QnAbstractCameraDataPtr &data){ 
                at_loader_ready(data, dataType);
            });

            connect(loader, &QnAbstractCameraDataLoader::failed,        this,  [this, dataType] {
                at_loader_failed(dataType);
            });
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

#ifndef QN_ENABLE_BOOKMARKS
        if (type == Qn::BookmarkData)
            continue;
#endif
       
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

    if(!m_timePeriodCameraData[Qn::MotionContent].isEmpty()) {
        m_timePeriodCameraData[Qn::MotionContent].clear();
        emit periodsChanged(Qn::MotionContent);
    }
    updateTimePeriods(Qn::MotionTimePeriod);
}

bool QnCachingCameraDataLoader::isMotionRegionsEmpty() const {
    foreach(const QRegion &region, m_motionRegions)
        if(!region.isEmpty())
            return false;
    return true;
}

QnTimePeriodList QnCachingCameraDataLoader::periods(Qn::TimePeriodContent timePeriodType) const {
    return m_timePeriodCameraData[timePeriodType].dataSource();
}

QnCameraBookmarkList QnCachingCameraDataLoader::bookmarks() const {
    return m_bookmarkCameraData.data();
}

QString QnCachingCameraDataLoader::bookmarksTextFilter() const {
    return m_bookmarksTextFilter;
}

void QnCachingCameraDataLoader::setBookmarksTextFilter(const QString &filter) {
    if (m_bookmarksTextFilter == filter)
        return;

    m_bookmarksTextFilter = filter;

    //TODO: #GDM #Bookmarks clearing cache too often, possibly search in cache by name/tags will be faster
    m_requestedTimePeriods[Qn::BookmarksContent].clear();
    m_timePeriodCameraData[Qn::BookmarksContent].clear();
    if (m_loaders[Qn::BookmarkTimePeriod])
        m_loaders[Qn::BookmarkTimePeriod]->discardCachedData();
    updateTimePeriods(Qn::BookmarkTimePeriod);
    emit periodsChanged(Qn::BookmarksContent);

    m_requestedBookmarkPeriodsByResolution.clear();
    m_bookmarkCameraData.clear();
    if (m_loaders[Qn::BookmarkData])
        m_loaders[Qn::BookmarkData]->discardCachedData();
    updateBookmarks();
    emit bookmarksChanged();
}

void QnCachingCameraDataLoader::addBookmark(const QnCameraBookmark &bookmark) {
    QnAbstractCameraDataPtr bookmarkData(new QnBookmarkCameraData(QnCameraBookmarkList() << bookmark));
    m_bookmarkCameraData.append(bookmarkData); //TODO: #GDM #Bookmarks additional method for appending a single bookmark is required

    QnTimePeriod bookmarkPeriod(bookmark.startTimeMs, bookmark.durationMs);
    m_timePeriodCameraData[Qn::BookmarksContent].append(QnTimePeriodList(bookmarkPeriod));  //TODO: #GDM #Bookmarks additional method for appending a single period is required

    emit periodsChanged(Qn::BookmarksContent);
    emit bookmarksChanged();
}


void QnCachingCameraDataLoader::updateBookmark(const QnCameraBookmark &bookmark) {
    m_bookmarkCameraData.updateBookmark(bookmark);
    emit bookmarksChanged();
}

void QnCachingCameraDataLoader::removeBookmark(const QnCameraBookmark & bookmark) {
    m_bookmarkCameraData.removeBookmark(bookmark);

    m_requestedTimePeriods[Qn::BookmarksContent].clear();
    m_timePeriodCameraData[Qn::BookmarksContent].clear();
    if (m_loaders[Qn::BookmarkTimePeriod])
        m_loaders[Qn::BookmarkTimePeriod]->discardCachedData();
    updateTimePeriods(Qn::BookmarkTimePeriod);

    emit bookmarksChanged();
}

QnCameraBookmark QnCachingCameraDataLoader::bookmarkByTime(qint64 position) const {
    return m_bookmarkCameraData.find(position);
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

void QnCachingCameraDataLoader::load(Qn::CameraDataType type, const QnTimePeriod &targetPeriod, const qint64 resolutionMs) {
    QnAbstractCameraDataLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (type) {
    case Qn::RecordedTimePeriod:
        qDebug() << "prepare to load";
        loader->load(targetPeriod);
        break;
#ifdef QN_ENABLE_BOOKMARKS
    case Qn::BookmarkTimePeriod:
    case Qn::BookmarkData:
        loader->load(targetPeriod, m_bookmarksTextFilter, resolutionMs);  //TODO: #GDM #Bookmarks process tags list on the server side
        break;
#else
        Q_UNUSED(resolutionMs);
#endif
    case Qn::MotionTimePeriod:
        if(!isMotionRegionsEmpty()) {
            QString filter = serializeRegionList(m_motionRegions);
            loader->load(targetPeriod, filter);
        } else if(!m_timePeriodCameraData[Qn::MotionContent].isEmpty()) {
            m_requestedTimePeriods[Qn::MotionContent].clear();
            m_timePeriodCameraData[Qn::MotionContent].clear();
            emit periodsChanged(Qn::MotionContent);
        }
        break;
    default:
        assert(false); //should never get here
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(const QnAbstractCameraDataPtr &data, Qn::CameraDataType dataType) {
    switch (dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod:
    case Qn::BookmarkTimePeriod:
        {
            Qn::TimePeriodContent timePeriodType = dataTypeToPeriod(dataType);

            if (m_timePeriodCameraData[timePeriodType].contains(data))
                return;
            m_timePeriodCameraData[timePeriodType].append(data);
#ifdef QN_ENABLE_BOOKMARKS
            if (dataType == Qn::BookmarkTimePeriod)
                updateBookmarks();
#endif
            emit periodsChanged(timePeriodType);
            break;
        }
    case Qn::BookmarkData:
        {
            if (m_bookmarkCameraData.contains(data))
                return;
            m_bookmarkCameraData.append(data);
            emit bookmarksChanged();
            break;
        }
    default:
        break;
    }


}

void QnCachingCameraDataLoader::at_loader_failed(Qn::CameraDataType dataType) {
    emit loadingFailed();

    switch (dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod:
    case Qn::BookmarkTimePeriod:
        {
            Qn::TimePeriodContent timePeriodType = dataTypeToPeriod(dataType);
            //TODO: #GDM investigate this old code
            // Live time periods should be trimmed to fixed time. Why are we trimming this by the error reply time? --gdm
            if (m_timePeriodCameraData[timePeriodType].trim(qnSyncTime->currentMSecsSinceEpoch()))
                emit periodsChanged(timePeriodType);
            break;
        }
    default:
        break;
    }
}

void QnCachingCameraDataLoader::discardCachedData() {
    for (int i = 0; i < Qn::CameraDataTypeCount; i++) {
        if (m_loaders[i])
            m_loaders[i]->discardCachedData();
    }

    for (int i = 0; i < Qn::TimePeriodContentCount; ++i) {
        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        m_requestedTimePeriods[timePeriodType].clear();
        m_timePeriodCameraData[timePeriodType].clear();
        updateTimePeriods(timePeriodToDataType(timePeriodType));
    }

#ifdef QN_ENABLE_BOOKMARKS
    m_requestedBookmarkPeriodsByResolution.clear();
    m_bookmarkCameraData.clear();
    updateBookmarks();
#endif
}

qint64 QnCachingCameraDataLoader::bookmarkResolution(qint64 periodDuration) const {
    static const std::vector<qint64> steps = [](){ 
        std::vector<qint64> result;
        for (int i = 0; i < 40; ++i)
            result.push_back(1ll << i);
        return result; 
    }();

    const int maxPeriodsPerTimeline = 1024; // magic const, thus visible periods can be edited through right-click
    qint64 maxPeriodLength = periodDuration / maxPeriodsPerTimeline;
    auto step = std::lower_bound(steps.cbegin(), steps.cend(), maxPeriodLength);
    return *step;
}

void QnCachingCameraDataLoader::updateTimePeriods(Qn::CameraDataType dataType) {
#ifndef QN_ENABLE_BOOKMARKS
    if (dataType == Qn::BookmarkTimePeriod || dataType == Qn::BookmarkData)
        return;
#endif

    QnTimePeriodList& requestedPeriods = m_requestedTimePeriods[dataTypeToPeriod(dataType)];
    if (requestedPeriods.containPeriod(m_targetPeriod[dataType]))
        return;

    QnTimePeriod requestedPeriod = addLoadingMargins(m_targetPeriod[dataType], m_boundingPeriod, minTimePeriodLoadingMargin);
    requestedPeriods = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << requestedPeriods << QnTimePeriodList(requestedPeriod));
    load(dataType, requestedPeriod);
}

void QnCachingCameraDataLoader::updateBookmarks() {
#ifdef QN_ENABLE_BOOKMARKS
    qint64 resolutionMs = bookmarkResolution(m_targetPeriod[Qn::BookmarkData].durationMs);
    QnTimePeriodList& requestedPeriods = m_requestedBookmarkPeriodsByResolution[resolutionMs];
    if (requestedPeriods.containPeriod(m_targetPeriod[Qn::BookmarkData]))
        return;

    QnTimePeriod requestedPeriod = addLoadingMargins(m_targetPeriod[Qn::BookmarkData], m_boundingPeriod, resolutionMs * 2);
    if (!periods(Qn::BookmarksContent).intersects(requestedPeriod)) // check that there are any bookmarks in this time period
        return;

    requestedPeriods = QnTimePeriodList::mergeTimePeriods(QVector<QnTimePeriodList>() << requestedPeriods << QnTimePeriodList(requestedPeriod));
    load(Qn::BookmarkData, requestedPeriod, resolutionMs);
#endif
}
