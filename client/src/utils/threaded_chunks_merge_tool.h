#pragma once

#include <utils/common/long_runnable.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

class QnThreadedChunksMergeTool: public QnLongRunnable {
    Q_OBJECT
public:
    QnThreadedChunksMergeTool();

    void queueMerge(const QVector<QnTimePeriodList> &periodsList, const QnTimePeriodList &syncedPeriods, const QnTimePeriod &updatedPeriod, int handle);
signals:
    void finished(int handle, const QnTimePeriodList &result);

protected:
    virtual void run() override;
    
private:
    void processData();

private:
    QVector<QnTimePeriodList> m_periodsList;
    QnTimePeriodList m_syncedPeriods;
    QnTimePeriod m_updatedPeriod;
    int m_handle;
    QMutex m_mutex;
};
