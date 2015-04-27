#include "threaded_chunks_merge_tool.h"

#include <utils/common/synctime.h>

QnThreadedChunksMergeTool::QnThreadedChunksMergeTool():
    m_handle(-1),
    m_queuedData(false)
{

}

void QnThreadedChunksMergeTool::run() {
    while (!needToStop())
    {
        bool queuedData = false;
        {
            QMutexLocker lock(&m_mutex);
            queuedData = m_queuedData;
        }

        if (queuedData)
            processData();
        else
            msleep(10);
    }
}

void QnThreadedChunksMergeTool::queueMerge(const QVector<QnTimePeriodList> &periodsList, const QnTimePeriodList &syncedPeriods, const QnTimePeriod &updatedPeriod, int handle) {
    QMutexLocker lock(&m_mutex);
    m_periodsList = periodsList;
    m_syncedPeriods = syncedPeriods;
    m_updatedPeriod = updatedPeriod;
    m_handle = handle;
    m_queuedData = true;
}

void QnThreadedChunksMergeTool::processData() {

    QVector<QnTimePeriodList> periodsList;
    QnTimePeriodList syncedPeriods;
    QnTimePeriod updatedPeriod;
    int handle;

    {
        QMutexLocker lock(&m_mutex);
        periodsList = m_periodsList;
        syncedPeriods = m_syncedPeriods;
        updatedPeriod = m_updatedPeriod;
        handle = m_handle;
        m_queuedData = false;
    }

    QnTimePeriod syncedPeriodsBounding = syncedPeriods.boundingPeriod(qnSyncTime->currentMSecsSinceEpoch());

    qreal relativeDuration = (!updatedPeriod.isNull() && syncedPeriodsBounding.isValid())
        ? updatedPeriod.durationMs / syncedPeriodsBounding.durationMs
        : 1.0;
    /* Here cost of intersecting and union becomes too high. */
    const auto magicCoeff = 0.5;

    if (updatedPeriod.isNull() || relativeDuration > magicCoeff) {
        QnTimePeriodList result = QnTimePeriodList::mergeTimePeriods(periodsList);
        emit finished(handle, result);
    } else {
        auto result = syncedPeriods;

        QVector<QnTimePeriodList> intersectedPeriods;
        for(const auto& list: periodsList)
            intersectedPeriods.push_back(list.intersectedPeriods(updatedPeriod));

        auto syncedAppending = QnTimePeriodList::mergeTimePeriods(intersectedPeriods);
        QnTimePeriodList::unionTimePeriods(result, syncedAppending);

        /* Trim live if recording was stopped. */
        if (!result.isEmpty() && !syncedAppending.isEmpty() && result.last().isInfinite() && !syncedAppending.last().isInfinite())
        {
            auto last = result.takeLast();
            last.durationMs = syncedAppending.last().endTimeMs() - last.startTimeMs;
            if (last.durationMs > 0)
                result.push_back(last);
        }

        emit finished(handle, result);
    }
}

