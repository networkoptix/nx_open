////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ANALYZING_SETTINGS_H
#define ANALYZING_SETTINGS_H

#include <QState>

#include "../blocking_queue.h"

#include <api/start_application_task.h>


class LauncherCommonData;

class AnalyzingTopTaskInQueue
:
    public QState
{
    Q_OBJECT

public:
    AnalyzingTopTaskInQueue(
        QState* const parent,
        LauncherCommonData* const fsmSharedData,
        BlockingQueue<StartApplicationTask>* const taskQueue );

signals:
    void done();

protected:
    virtual void onEntry( QEvent* _event ) override;

private:
    LauncherCommonData* const m_fsmSharedData;
    BlockingQueue<StartApplicationTask>* const m_taskQueue;
};

#endif  //ANALYZING_SETTINGS_H
