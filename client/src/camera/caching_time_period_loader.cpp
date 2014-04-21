#include "caching_time_period_loader.h"

#include <QtCore/QMetaType>

#include <api/serializer/serializer.h>

#include <core/resource/network_resource.h>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

#include "time_period_loader.h"
#include "multi_camera_time_period_loader.h"
#include "layout_file_time_period_loader.h"

namespace {
    const qint64 minLoadingMargin = 60 * 60 * 1000; /* 1 hour. */
    const qint64 defaultUpdateInterval = 10 * 1000;
}


QnCachingTimePeriodLoader::QnCachingTimePeriodLoader(const QnResourcePtr &resource, QObject *parent):
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

QnCachingTimePeriodLoader::QnCachingTimePeriodLoader(QnAbstractTimePeriodLoader **loaders, QObject *parent):
    QObject(parent),
    m_resource(loaders[0]->resource())
{
    init();
    initLoaders(loaders);
}

QnCachingTimePeriodLoader::~QnCachingTimePeriodLoader() {
    return;
}

void QnCachingTimePeriodLoader::init() {
    m_loadingMargin = 1.0;
    m_updateInterval = defaultUpdateInterval;
    m_resourceIsLocal = !m_resource.dynamicCast<QnNetworkResource>();

    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        m_handles[i] = -1;
        m_loaders[i] = NULL;
    }

    if(!m_resourceIsLocal)
        connect(qnSyncTime, SIGNAL(timeChanged()), this, SLOT(at_syncTime_timeChanged()));
}

void QnCachingTimePeriodLoader::initLoaders(QnAbstractTimePeriodLoader **loaders) {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        QnAbstractTimePeriodLoader *loader = loaders[i];
        m_loaders[i] = loader;

        if(loader) {
            loader->setParent(this);

            connect(loader, SIGNAL(ready(const QnTimePeriodList &, int)),   this,   SLOT(at_loader_ready(const QnTimePeriodList &, int)));
            connect(loader, SIGNAL(failed(int, int)),                       this,   SLOT(at_loader_failed(int, int)));
        }
    }
}

bool QnCachingTimePeriodLoader::createLoaders(const QnResourcePtr &resource, QnAbstractTimePeriodLoader **loaders) 
{
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        loaders[i] = NULL;

    bool success = true;
    bool isNetRes = resource.dynamicCast<QnNetworkResource>();
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) 
    {
        Qn::TimePeriodContent type = static_cast<Qn::TimePeriodContent>(i);
        if (isNetRes)
            loaders[i] = QnMultiCameraTimePeriodLoader::newInstance(resource, type);
        else
            loaders[i] = QnLayoutFileTimePeriodLoader::newInstance(resource, type);
        if(!loaders[i]) {
            success = false;
            break;
        }
    }

    if(success)
        return true;

    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        delete loaders[i];
    return false;
}

QnCachingTimePeriodLoader *QnCachingTimePeriodLoader::newInstance(const QnResourcePtr &resource, QObject *parent) 
{
    QnAbstractTimePeriodLoader *loaders[Qn::TimePeriodContentCount];
    if(createLoaders(resource, loaders)) {
        return new QnCachingTimePeriodLoader(loaders, parent);
    } else {
        return NULL;
    }
}

QnResourcePtr QnCachingTimePeriodLoader::resource() {
    return m_resource;
}

qreal QnCachingTimePeriodLoader::loadingMargin() const {
    return m_loadingMargin;
}

void QnCachingTimePeriodLoader::setLoadingMargin(qreal loadingMargin) {
    m_loadingMargin = loadingMargin;
}

qint64 QnCachingTimePeriodLoader::updateInterval() const {
    return m_updateInterval;
}

void QnCachingTimePeriodLoader::setUpdateInterval(qint64 msecs) {
    m_updateInterval = msecs;
}

const QnTimePeriod &QnCachingTimePeriodLoader::loadedPeriod() const {
    return m_loadedPeriod;
}

void QnCachingTimePeriodLoader::setTargetPeriods(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod) {
    if(m_loadedPeriod.contains(targetPeriod)) 
        return;
    
    m_loadedPeriod = addLoadingMargins(targetPeriod, boundingPeriod);
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        load(static_cast<Qn::TimePeriodContent>(i));
}

const QList<QRegion> &QnCachingTimePeriodLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingTimePeriodLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    load(Qn::MotionContent);
}

bool QnCachingTimePeriodLoader::isMotionRegionsEmpty() const {
    foreach(const QRegion &region, m_motionRegions)
        if(!region.isEmpty())
            return false;
    return true;
}

QnTimePeriodList QnCachingTimePeriodLoader::periods(Qn::TimePeriodContent type) {
    return m_periods[type];
}

QnTimePeriod QnCachingTimePeriodLoader::addLoadingMargins(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod) const {
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

void QnCachingTimePeriodLoader::load(Qn::TimePeriodContent type) {
    QnAbstractTimePeriodLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
        return;
    } 

    switch (type) {
    case Qn::RecordingContent:
    case Qn::BookmarksContent:
        m_handles[type] = loader->load(m_loadedPeriod, QString());
        break;
    case Qn::MotionContent:
        if(!isMotionRegionsEmpty()) {
            QString filter = serializeRegionList(m_motionRegions);
            m_handles[type] = loader->load(m_loadedPeriod, filter);
        } else if(!m_periods[type].isEmpty()) {
            m_periods[type].clear();
            emit periodsChanged(type);
        }
        break;
    default:
        assert(false); //should never get here
    }
}

void QnCachingTimePeriodLoader::trim(Qn::TimePeriodContent type, qint64 trimTime) {
    if(m_periods[type].isEmpty())
        return;

    QnTimePeriod period = m_periods[type].back();
    qint64 trimmedDurationMs = qMax(0ll, trimTime - period.startTimeMs);
    if(period.durationMs != -1 && period.durationMs <= trimmedDurationMs)
        return;

    period.durationMs = trimmedDurationMs;
    if(period.durationMs == 0) {
        m_periods[type].pop_back();
    } else {
        m_periods[type].back() = period;
    }

    emit periodsChanged(type);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingTimePeriodLoader::at_loader_ready(const QnTimePeriodList &timePeriods, int handle) {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        if(handle == m_handles[i] && m_periods[i] != timePeriods) {
            m_periods[i] = timePeriods;
            emit periodsChanged(static_cast<Qn::TimePeriodContent>(i));
        }
    }
}

void QnCachingTimePeriodLoader::at_loader_failed(int /*status*/, int handle) {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        if(handle == m_handles[i]) {
            m_handles[i] = -1;
            emit loadingFailed();

            qint64 trimTime = qnSyncTime->currentMSecsSinceEpoch();
            for(int j = 0; j < Qn::TimePeriodContentCount; j++)
                trim(static_cast<Qn::TimePeriodContent>(j), trimTime);
        }
    }
}

void QnCachingTimePeriodLoader::at_syncTime_timeChanged() {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        m_loaders[i]->discardCachedData();
        load(static_cast<Qn::TimePeriodContent>(i));
    }
}


