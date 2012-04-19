#include "caching_time_period_loader.h"

#include <utils/common/warnings.h>

#include "time_period_loader.h"


QnCachingTimePeriodLoader::QnCachingTimePeriodLoader(QObject *parent):
    QObject(parent),
    m_loader(NULL),
    m_loadingMargin(1.0)
{
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++)
        m_handles[i] = -1;
}

QnCachingTimePeriodLoader::~QnCachingTimePeriodLoader() {
    return;
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

const QnTimePeriod &QnCachingTimePeriodLoader::loadedPeriod() const {
    return m_loadedPeriod;
}

QnTimePeriod QnCachingTimePeriodLoader::addLoadingMargins(const QnTimePeriod &targetPeriod) const {
    qint64 margin;
    if(targetPeriod.durationMs == -1) {
        margin = 0;
    } else {
        margin = targetPeriod.durationMs * m_loadingMargin;
    }

    return QnTimePeriod(
        targetPeriod.startTimeMs - margin,
        targetPeriod.durationMs + 2 * margin
    );
}

void QnCachingTimePeriodLoader::load(Qn::TimePeriodType type) {
    QnTimePeriodLoader *loader = this->loader();
    if(!loader) {
        qnWarning("No valid loader in scope.");
        emit loadingFailed();
    } else {
        if(type == Qn::RecordingTimePeriod) {
            m_handles[type] = loader->load(m_loadedPeriod);
        } else {
            m_handles[type] = loader->load(m_loadedPeriod, m_motionRegions);
        }
    }
}

QnTimePeriodList QnCachingTimePeriodLoader::periods(const QnTimePeriod &targetPeriod, const QList<QRegion> &motionRegions) {
    if(!m_loadedPeriod.containPeriod(targetPeriod)) {
        m_loadedPeriod = addLoadingMargins(targetPeriod);
        if(!motionRegions.isEmpty()) {
            m_motionRegions = motionRegions;
            m_periods[Qn::MotionTimePeriod].clear();
        }

        load(Qn::RecordingTimePeriod);
        load(Qn::MotionTimePeriod);
    } else if(!motionRegions.isEmpty() && m_motionRegions != motionRegions) {
        m_motionRegions = motionRegions;
        m_periods[Qn::MotionTimePeriod].clear();

        load(Qn::MotionTimePeriod);
    }

    if(motionRegions.isEmpty()) {
        return m_periods[Qn::RecordingTimePeriod];
    } else {
        return m_periods[Qn::MotionTimePeriod];
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingTimePeriodLoader::at_loader_ready(const QnTimePeriodList &timePeriods, int handle) {
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++) {
        if(handle == m_handles[i]) {
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
        }
    }
}

void QnCachingTimePeriodLoader::at_loader_destroyed() {
    setLoader(NULL);
}

