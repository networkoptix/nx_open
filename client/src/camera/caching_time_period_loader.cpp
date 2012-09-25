#include "caching_time_period_loader.h"

#include <QtCore/QMetaType>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>
#include <utils/client_meta_types.h>

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
    QnClientMetaTypes::initialize();

    m_loadingMargin = 1.0;
    m_updateInterval = defaultUpdateInterval;

    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
        m_handles[i] = -1;
        m_loaders[i] = NULL;
    }
}

void QnCachingTimePeriodLoader::initLoaders(QnAbstractTimePeriodLoader **loaders) {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
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
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        loaders[i] = NULL;

    bool success = true;
    bool isNetRes = resource.dynamicCast<QnNetworkResource>();
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) 
    {
        if (isNetRes)
            loaders[i] = QnMultiCameraTimePeriodLoader::newInstance(resource);
        else
            loaders[i] = QnLayoutFileTimePeriodLoader::newInstance(resource);
        if(!loaders[i]) {
            success = false;
            break;
        }
    }

    if(success)
        return true;

    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        delete loaders[i];
    return false;
}

QnCachingTimePeriodLoader *QnCachingTimePeriodLoader::newInstance(const QnResourcePtr &resource, QObject *parent) 
{
    QnAbstractTimePeriodLoader *loaders[Qn::TimePeriodRoleCount];
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

void QnCachingTimePeriodLoader::setTargetPeriod(const QnTimePeriod &targetPeriod) {
    if(m_loadedPeriod.contains(targetPeriod)) 
        return;
    
    m_loadedPeriod = addLoadingMargins(targetPeriod);
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        load(static_cast<Qn::TimePeriodRole>(i));
}

const QList<QRegion> &QnCachingTimePeriodLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingTimePeriodLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    load(Qn::MotionRole);
}

bool QnCachingTimePeriodLoader::isMotionRegionsEmpty() const {
    foreach(const QRegion &region, m_motionRegions)
        if(!region.isEmpty())
            return false;
    return true;
}

QnTimePeriodList QnCachingTimePeriodLoader::periods(Qn::TimePeriodRole type) {
    return m_periods[type];
}

QnTimePeriod QnCachingTimePeriodLoader::addLoadingMargins(const QnTimePeriod &targetPeriod) const {
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();

    qint64 startTime = targetPeriod.startTimeMs;
    qint64 endTime = targetPeriod.durationMs == -1 ? currentTime : targetPeriod.startTimeMs + targetPeriod.durationMs;

    /* Adjust for margin. */
    qint64 margin = qMax(minLoadingMargin, static_cast<qint64>((endTime - startTime) * m_loadingMargin));
    startTime -= margin;
    endTime += margin;

    /* Adjust for update interval. */
    endTime = qMin(endTime, currentTime + m_updateInterval);

    return QnTimePeriod(startTime, endTime - startTime);
}

void QnCachingTimePeriodLoader::load(Qn::TimePeriodRole type) {
    QnAbstractTimePeriodLoader *loader = m_loaders[type];
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
    } else {
        if(type == Qn::RecordingRole) {
            m_handles[type] = loader->load(m_loadedPeriod);
        } else { /* type == Qn::MotionRole */
            if(!isMotionRegionsEmpty()) {
                m_handles[type] = loader->load(m_loadedPeriod, m_motionRegions);
            } else if(!m_periods[type].isEmpty()) {
                m_periods[type].clear();
                emit periodsChanged(type);
            }
        }
    }
}

void QnCachingTimePeriodLoader::trim(Qn::TimePeriodRole type, qint64 trimTime) {
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
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
        if(handle == m_handles[i] && m_periods[i] != timePeriods) {
            m_periods[i] = timePeriods;
            emit periodsChanged(static_cast<Qn::TimePeriodRole>(i));
        }
    }
}

void QnCachingTimePeriodLoader::at_loader_failed(int /*status*/, int handle) {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++) {
        if(handle == m_handles[i]) {
            m_handles[i] = -1;
            emit loadingFailed();

            qint64 trimTime = qnSyncTime->currentMSecsSinceEpoch();
            for(int j = 0; j < Qn::TimePeriodRoleCount; j++)
                trim(static_cast<Qn::TimePeriodRole>(j), trimTime);
        }
    }
}

