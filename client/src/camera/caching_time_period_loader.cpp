#include "caching_time_period_loader.h"

#include <QtCore/QMetaType>

#include <utils/common/warnings.h>
#include <utils/common/synctime.h>

#include "time_period_loader.h"

namespace {
    qint64 minLoadingMargin = 60 * 60 * 1000; /* 1 hour. */
    qint64 defaultUpdateInterval = 10 * 1000;
}


QnCachingTimePeriodLoader::QnCachingTimePeriodLoader(QObject *parent):
    QObject(parent),
    m_loader(NULL),
    m_loadingMargin(1.0),
    m_updateInterval(defaultUpdateInterval)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<Qn::TimePeriodType>();
        metaTypesInitialized = true;
    }

    for(int i = 0; i < Qn::TimePeriodTypeCount; i++)
        m_handles[i] = -1;
}

QnCachingTimePeriodLoader::~QnCachingTimePeriodLoader() {
    return;
}

QnCachingTimePeriodLoader *QnCachingTimePeriodLoader::newInstance(const QnResourcePtr &resouce, QObject *parent) {
    QnTimePeriodLoader *loader = QnTimePeriodLoader::newInstance(resouce);
    if(!loader)
        return NULL;

    QnCachingTimePeriodLoader *result = new QnCachingTimePeriodLoader(parent);
    result->setLoader(loader);
    loader->setParent(result);
    return result;
}

QnTimePeriodLoader *QnCachingTimePeriodLoader::loader() {
    return m_loader;
}

void QnCachingTimePeriodLoader::setLoader(QnTimePeriodLoader *loader) {
    if(loader == m_loader)
        return;

    if(m_loader)
        disconnect(m_loader, NULL, this, NULL);

    m_loader = loader;

    if(m_loader) {
        connect(loader, SIGNAL(ready(const QnTimePeriodList &, int)),   this,   SLOT(at_loader_ready(const QnTimePeriodList &, int)));
        connect(loader, SIGNAL(failed(int, int)),                       this,   SLOT(at_loader_failed(int, int)));
        connect(loader, SIGNAL(destroyed()),                            this,   SLOT(at_loader_destroyed()));
    }
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
    if(m_loadedPeriod.containPeriod(targetPeriod)) 
        return;
    
    m_loadedPeriod = addLoadingMargins(targetPeriod);
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++)
        load(static_cast<Qn::TimePeriodType>(i));
}

const QList<QRegion> &QnCachingTimePeriodLoader::motionRegions() const {
    return m_motionRegions;
}

void QnCachingTimePeriodLoader::setMotionRegions(const QList<QRegion> &motionRegions) {
    if(m_motionRegions == motionRegions)
        return;

    m_motionRegions = motionRegions;
    load(Qn::MotionTimePeriod);
}

QnTimePeriodList QnCachingTimePeriodLoader::periods(Qn::TimePeriodType type) {
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

void QnCachingTimePeriodLoader::load(Qn::TimePeriodType type) {
    QnTimePeriodLoader *loader = this->loader();
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
    } else {
        if(type == Qn::RecordingTimePeriod) {
            m_handles[type] = loader->load(m_loadedPeriod);
        } else { /* type == Qn::MotionTimePeriod */
            if(!m_motionRegions.empty()) {
                m_handles[type] = loader->load(m_loadedPeriod, m_motionRegions);
            } else if(!m_periods[type].isEmpty()) {
                m_periods[type].clear();
                emit periodsChanged(type);
            }
        }
    }
}

void QnCachingTimePeriodLoader::trim(Qn::TimePeriodType type, qint64 trimTime) {
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
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
        if(handle == m_handles[i] && m_periods[i] != timePeriods) {
            m_periods[i] = timePeriods;
            emit periodsChanged(static_cast<Qn::TimePeriodType>(i));
        }
    }
}

void QnCachingTimePeriodLoader::at_loader_failed(int status, int handle) {
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
        if(handle == m_handles[i]) {
            m_handles[i] = -1;
            emit loadingFailed();

            qint64 trimTime = qnSyncTime->currentMSecsSinceEpoch();
            for(int j = 0; j < Qn::TimePeriodTypeCount; j++)
                trim(static_cast<Qn::TimePeriodType>(j), trimTime);
        }
    }
}

void QnCachingTimePeriodLoader::at_loader_destroyed() {
    setLoader(NULL);
}

