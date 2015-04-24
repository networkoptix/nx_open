#include "threaded_chunks_merge_tool.h"

#include <utils/common/synctime.h>

QnThreadedChunksMergeTool::QnThreadedChunksMergeTool()
{

}

void QnThreadedChunksMergeTool::run() {
    while (!needToStop())
    {
         if (!m_periodsList.isEmpty()) {
 
            processData();
         }
         else {
            msleep(10);
        }
    }
}

void QnThreadedChunksMergeTool::queueMerge(const QVector<QnTimePeriodList> &periodsList, const QnTimePeriodList &syncedPeriods, const QnTimePeriod &updatedPeriod, int handle) {
    //QMutexLocker lock(&m_mutex);
    m_periodsList = periodsList;
    m_syncedPeriods = syncedPeriods;
    m_updatedPeriod = updatedPeriod;
    m_handle = handle;
}

void QnThreadedChunksMergeTool::processData() {
    QVector<QnTimePeriodList> periodsList = m_periodsList;
    m_periodsList = QVector<QnTimePeriodList>();
    int handle = m_handle;

    QnTimePeriod syncedPeriodsBounding = m_syncedPeriods.boundingPeriod(qnSyncTime->currentMSecsSinceEpoch());

    qreal relativeDuration = (!m_updatedPeriod.isNull() && syncedPeriodsBounding.isValid())
        ? m_updatedPeriod.durationMs / syncedPeriodsBounding.durationMs
        : 1.0;
    /* Here cost of intersecting and union becomes too high. */
    const qreal magicCoeff = 0.5;

    if (m_updatedPeriod.isNull() || relativeDuration > magicCoeff) {
        QnTimePeriodList result = QnTimePeriodList::mergeTimePeriods(periodsList);
        emit finished(handle, result);
    } else {
        QnTimePeriodList result = m_syncedPeriods;

        QVector<QnTimePeriodList> intersectedPeriods;
        for(const QnTimePeriodList &list: periodsList)
            intersectedPeriods.push_back(list.intersectedPeriods(m_updatedPeriod));


        QnTimePeriodList syncedAppending = QnTimePeriodList::mergeTimePeriods(intersectedPeriods);
        QnTimePeriodList::unionTimePeriods(result, syncedAppending);

        emit finished(handle, result);
    }
}

