#include "threaded_chunks_merge_tool.h"

#include <utils/common/synctime.h>

QnThreadedChunksMergeTool::QnThreadedChunksMergeTool():
    m_startTimeMs(0),
    m_handle(-1),
    m_queuedData(false)
{

}

void QnThreadedChunksMergeTool::run() {
    while (!needToStop())
    {
        bool queuedData = false;
        {
            QnMutexLocker lock(&m_mutex);
            queuedData = m_queuedData;
        }

        if (queuedData)
            processData();
        else
            msleep(10);
    }
}

QnThreadedChunksMergeTool::~QnThreadedChunksMergeTool()
{
    stop();
}

void QnThreadedChunksMergeTool::queueMerge(const std::vector<QnTimePeriodList> &periodsList, const QnTimePeriodList &syncedPeriods, qint64 startTimeMs, int handle) {
    QnMutexLocker lock(&m_mutex);
    m_periodsList = periodsList;
    m_syncedPeriods = syncedPeriods;

    /* If we are loading chunks for the same layout, make sure we do not skip already queued startTimeMs. */
    if (m_queuedData && m_handle == handle)
        m_startTimeMs = std::min(startTimeMs, m_startTimeMs);
    else
        m_startTimeMs = startTimeMs;

    m_handle = handle;
    m_queuedData = true;
}

void QnThreadedChunksMergeTool::processData() {

    /* Synchronization block */
    QnMutexLocker lock(&m_mutex);
    const std::vector<QnTimePeriodList> periodsList(m_periodsList);
    const QnTimePeriodList syncedPeriods(m_syncedPeriods);
    const qint64 startTimeMs(m_startTimeMs);
    const int handle(m_handle);
    m_queuedData = false;
    lock.unlock();
    /* End of synchronization block */

    /* Using current time to estimate update length */
    qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    QnTimePeriod syncedPeriodsBounding = syncedPeriods.boundingPeriod(currentTimeMs);

    qreal relativeDuration = (startTimeMs > 0 && syncedPeriodsBounding.isValid())
        ? (currentTimeMs - startTimeMs) / syncedPeriodsBounding.durationMs
        : 1.0;
    /* Here cost of intersecting and union becomes too high. */
    const auto magicCoeff = 0.5;

    if (startTimeMs == 0 || relativeDuration > magicCoeff) {
        auto result = QnTimePeriodList::mergeTimePeriods(periodsList);
        emit finished(handle, result);
    } else {
        auto result = syncedPeriods;

        std::vector<QnTimePeriodList> intersectedPeriods;
        for(const auto& list: periodsList)
            intersectedPeriods.push_back(list.intersectedPeriods(QnTimePeriod(startTimeMs, QnTimePeriod::infiniteDuration())));

        auto syncedAppending = QnTimePeriodList::mergeTimePeriods(intersectedPeriods);
        QnTimePeriodList::overwriteTail(result, syncedAppending, startTimeMs);
      
        emit finished(handle, result);
    }
}

