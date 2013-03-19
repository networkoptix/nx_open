////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef TASK_QUEUE_WATCHER_H
#define TASK_QUEUE_WATCHER_H

#include <QMutex>
#include <QWaitCondition>
#include <QThread>

#include <utils/common/long_runnable.h>

#include "blocking_queue.h"
#include <api/start_application_task.h>


/*!
    Waits for \a taskQueue to become non-empty and sends signal \a taskReceived()
*/
class TaskQueueWatcher
:
    public QnLongRunnable
{
    Q_OBJECT

public:
    TaskQueueWatcher( BlockingQueue<StartApplicationTask>* const taskQueue );
    ~TaskQueueWatcher();

    virtual void pleaseStop() override;

public slots:
    void startMonitoring();
    void stopMonitoring();

signals:
    void taskReceived();

protected:
    virtual void run();

private:
    BlockingQueue<StartApplicationTask>* const m_taskQueue;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_started;
};

#endif  //TASK_QUEUE_WATCHER_H
