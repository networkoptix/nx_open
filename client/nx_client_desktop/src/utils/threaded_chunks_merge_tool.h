#pragma once

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/mutex.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>


class QnThreadedChunksMergeTool: public QnLongRunnable {
    Q_OBJECT
public:
    QnThreadedChunksMergeTool();
    virtual ~QnThreadedChunksMergeTool();

    void queueMerge(const std::vector<QnTimePeriodList> &periodsList, const QnTimePeriodList &syncedPeriods, qint64 startTimeMs, int handle);
signals:
    void finished(int handle, const QnTimePeriodList &result);

protected:
    virtual void run() override;
    
private:
    void processData();

private:
    std::vector<QnTimePeriodList> m_periodsList;
    QnTimePeriodList m_syncedPeriods;
    qint64 m_startTimeMs;
    int m_handle;
    bool m_queuedData;

    QnMutex m_mutex;
};
