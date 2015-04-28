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
#include "api/common_message_processor.h"

namespace {
    const qint64 requestIntervalMs = 10 * 1000;
}

QnCachingCameraDataLoader::QnCachingCameraDataLoader(QnAbstractCameraDataLoader **loaders, QObject *parent):
    base_type(parent),
    m_resource(loaders[0]->resource())
{
    init();
    initLoaders(loaders);

    for (int i = 0; i < Qn::TimePeriodContentCount; ++i) {
        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        m_previousRequestTime[timePeriodType] = 0;
    }

    QTimer* loadTimer = new QTimer(this);
    loadTimer->setInterval(requestIntervalMs);  // time period will be loaded no often than once in 30 seconds
    loadTimer->setSingleShot(false);
    connect(loadTimer, &QTimer::timeout, this, [this] {
        load();
    });
    loadTimer->start();
    load();
}

QnCachingCameraDataLoader::~QnCachingCameraDataLoader() {
}

void QnCachingCameraDataLoader::init() {
    m_resourceIsLocal = !m_resource.dynamicCast<QnNetworkResource>();

    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        m_loaders[i] = NULL;
    }

    if(!m_resourceIsLocal) {
        connect(qnSyncTime, &QnSyncTime::timeChanged,       this, &QnCachingCameraDataLoader::discardCachedData);
        connect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::initialResourcesReceived, this, &QnCachingCameraDataLoader::discardCachedData);
    }
}

void QnCachingCameraDataLoader::initLoaders(QnAbstractCameraDataLoader **loaders) {
    for(int i = 0; i < Qn::CameraDataTypeCount; i++) {
        Qn::CameraDataType dataType = static_cast<Qn::CameraDataType>(i);
        QnAbstractCameraDataLoader *loader = loaders[i];
        m_loaders[i] = loader;

        if(loader) {
            loader->setParent(this);

            connect(loader, &QnAbstractCameraDataLoader::ready,         this,  [this, dataType](const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle){ 
                Q_UNUSED(handle);
                Q_ASSERT_X(updatedPeriod.isInfinite(), Q_FUNC_INFO, "We are always loading till very end.");
                at_loader_ready(data, updatedPeriod.startTimeMs, dataType);
            });

            connect(loader, &QnAbstractCameraDataLoader::failed,        this,  &QnCachingCameraDataLoader::loadingFailed);
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

void QnCachingCameraDataLoader::load() {
    for (int i = 0; i < Qn::TimePeriodContentCount; ++i) {
        Qn::TimePeriodContent timePeriodType = static_cast<Qn::TimePeriodContent>(i);
        updateTimePeriods(timePeriodType);
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
    updateTimePeriods(Qn::MotionContent);
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
    m_cameraChunks[Qn::BookmarksContent].clear();
    if (m_loaders[Qn::BookmarkTimePeriod])
        m_loaders[Qn::BookmarkTimePeriod]->discardCachedData();
    updateTimePeriods(Qn::BookmarksContent);
    emit periodsChanged(Qn::BookmarksContent);

    m_requestedBookmarkPeriodsByResolution.clear();
    m_bookmarkCameraData.clear();
    if (m_loaders[Qn::BookmarkData])
        m_loaders[Qn::BookmarkData]->discardCachedData();
    updateBookmarks();
    emit bookmarksChanged();
}

void QnCachingCameraDataLoader::addBookmark(const QnCameraBookmark &bookmark) {
    QnTimePeriod bookmarkPeriod(bookmark.startTimeMs, bookmark.durationMs); 

    QnAbstractCameraDataPtr bookmarkData(new QnBookmarkCameraData(QnCameraBookmarkList() << bookmark));
    m_bookmarkCameraData.update(bookmarkData, bookmarkPeriod); //TODO: #GDM #Bookmarks additional method for appending a single bookmark is required

    m_cameraChunks[Qn::BookmarksContent] = m_bookmarkCameraData.dataSource();;

//    emit periodsChanged(Qn::BookmarksContent, bookmarkPeriod);
    emit bookmarksChanged();
}


void QnCachingCameraDataLoader::updateBookmark(const QnCameraBookmark &bookmark) {
    m_bookmarkCameraData.updateBookmark(bookmark);
    emit bookmarksChanged();
}

void QnCachingCameraDataLoader::removeBookmark(const QnCameraBookmark & bookmark) {
    m_bookmarkCameraData.removeBookmark(bookmark);

    m_cameraChunks[Qn::BookmarksContent].clear();
    if (m_loaders[Qn::BookmarkTimePeriod])
        m_loaders[Qn::BookmarkTimePeriod]->discardCachedData();
    updateTimePeriods(Qn::BookmarksContent);

    emit bookmarksChanged();
}

QnCameraBookmark QnCachingCameraDataLoader::bookmarkByTime(qint64 position) const {
    return m_bookmarkCameraData.find(position);
}

void QnCachingCameraDataLoader::loadInternal(Qn::TimePeriodContent periodType) {
#ifndef QN_ENABLE_BOOKMARKS
    if (periodType == Qn::BookmarksContent)
        return;
#endif

    Qn::CameraDataType type = timePeriodToDataType(periodType);

    QnAbstractCameraDataLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (type) {
    case Qn::RecordedTimePeriod:
        loader->load();
        break;
    case Qn::MotionTimePeriod:
        if(!isMotionRegionsEmpty()) {
            QString filter = serializeRegionList(m_motionRegions);
            loader->load(filter);
        } else if(!m_cameraChunks[Qn::MotionContent].isEmpty()) {
            m_cameraChunks[Qn::MotionContent].clear();
            emit periodsChanged(Qn::MotionContent);
        }
        break;
    default:
        break;
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::at_loader_ready(const QnAbstractCameraDataPtr &data, qint64 startTimeMs, Qn::CameraDataType dataType) {
    switch (dataType) {
    case Qn::RecordedTimePeriod:
    case Qn::MotionTimePeriod:
    case Qn::BookmarkTimePeriod:
        {
            auto timePeriodType = dataTypeToPeriod(dataType);
            m_cameraChunks[timePeriodType] = data->dataSource();
#ifdef QN_ENABLE_BOOKMARKS
            if (dataType == Qn::BookmarkTimePeriod)
                updateBookmarks();
#endif
            emit periodsChanged(timePeriodType, startTimeMs);
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
        m_previousRequestTime[timePeriodType] = 0;
        m_cameraChunks[timePeriodType].clear();
        updateTimePeriods(timePeriodType);
        emit periodsChanged(timePeriodType);
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

void QnCachingCameraDataLoader::updateTimePeriods(Qn::TimePeriodContent periodType) {
#ifndef QN_ENABLE_BOOKMARKS
    if (periodType == Qn::BookmarksContent)
        return;
#endif

    qint64 curTime = QDateTime::currentMSecsSinceEpoch();
    qint64 timeSpan = curTime - m_previousRequestTime[periodType];
    if (periodType != Qn::RecordingContent || m_previousRequestTime[periodType] == 0 || timeSpan > requestIntervalMs) {
        loadInternal(periodType);
        m_previousRequestTime[periodType] = curTime;
    }
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
