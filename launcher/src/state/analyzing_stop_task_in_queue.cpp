////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "analyzing_stop_task_in_queue.h"

#include "launcher_common_data.h"


AnalyzingTopTaskInQueue::AnalyzingTopTaskInQueue(
    QState* const parent,
    LauncherCommonData* const fsmSharedData,
    BlockingQueue<StartApplicationTask>* const taskQueue )
:
    QState( parent ),
    m_fsmSharedData( fsmSharedData ),
    m_taskQueue( taskQueue )
{
}

void AnalyzingTopTaskInQueue::onEntry( QEvent* _event )
{
    QState::onEntry( _event );

    Q_ASSERT( !m_taskQueue->empty() );
    m_fsmSharedData->currentTask = m_taskQueue->front();
    m_taskQueue->pop();

    emit done();
}
