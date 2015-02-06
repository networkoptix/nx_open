/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#include "timermanager.h"

#include <map>
#include <string>

#include <QtCore/QAtomicInt>
#include <QtCore/QDateTime>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>

#include <utils/common/log.h>


using namespace std;

class TimerManagerImpl
{
public:
    QWaitCondition cond;
    mutable QMutex mtx;
    //!map<pair<time, timerID>, handler>
    std::map<std::pair<qint64, quint64>, std::function<void(quint64)> > timeToTask;
    //!map<timerID, time>
    std::map<quint64, qint64> taskToTime;
    bool terminated;
    //!ID of task, being executed. 0, if no running task
    quint64 runningTaskID;

    TimerManagerImpl()
    :
        terminated( false ),
        runningTaskID( 0 )
    {
    }

    void deleteTaskNonSafe( const quint64 timerID )
    {
        const map<quint64, qint64>::iterator it = taskToTime.find( timerID );
        if( it == taskToTime.end() )
            return;

        /*size_t ret =*/ timeToTask.erase( make_pair( it->second, it->first ) );
        //ASSERT2( ret == 1, "deleteTimer", "ret = "<<ret<<", task time (UTC) "<<it->second<<", current UTC time "<<QDateTime::currentMSecsSinceEpoch() );

        /*map<quint64, qint64>::size_type s1 =*/ taskToTime.size();
        taskToTime.erase( it );
        //ASSERT2( taskToTime.size() == s1-1, "s1 = "<<s1<<", taskToTime.size() = "<<taskToTime.size() );
    }
};

TimerManager::TimerManager()
:
    m_impl( new TimerManagerImpl() )
{
    start();

    setObjectName( lit("TimerManager") );
}

TimerManager::~TimerManager()
{
    {
        QMutexLocker lk( &m_impl->mtx );
        m_impl->terminated = true;
        m_impl->cond.wakeAll();
    }

    wait();

    delete m_impl;
    m_impl = NULL;
}

quint64 TimerManager::addTimer(
    TimerEventHandler* const taskManager,
    const unsigned int delay )
{
    using namespace std::placeholders;
    return addTimer( std::bind( &TimerEventHandler::onTimer, taskManager, _1 ), delay );
}

quint64 TimerManager::addTimer(
    std::function<void(quint64)> taskHandler,
    const unsigned int delay )
{
    static QAtomicInt lastTaskID = 0;

    QMutexLocker lk( &m_impl->mtx );

    const qint64 taskTime = QDateTime::currentMSecsSinceEpoch() + delay;
    quint64 timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
    if( timerID == 0 )
        lastTaskID.fetchAndAddOrdered(1);     //0 is a reserved value

    if( !m_impl->timeToTask.insert( make_pair(
            make_pair( taskTime, timerID ),
            taskHandler ) ).second )
    {
        //ASSERT( false );
    }
    if( !m_impl->taskToTime.insert( make_pair( timerID, taskTime ) ).second )
    {
        //ASSERT( false );
    }

    m_impl->cond.wakeOne();

    return timerID;
}

void TimerManager::deleteTimer( const quint64& timerID )
{
    QMutexLocker lk( &m_impl->mtx );

    m_impl->deleteTaskNonSafe( timerID );
}

void TimerManager::joinAndDeleteTimer( const quint64& timerID )
{
    QMutexLocker lk( &m_impl->mtx );
    //having locked \a m_impl->mtx we garantee, that execution of timer timerID will not start

    if( QThread::currentThread() != this )
    {
        while( m_impl->runningTaskID == timerID )
            m_impl->cond.wait( lk.mutex() );      //waiting for timer handler execution finish
    }
    else
    {
        //method called from scheduler thread (there is TimerManagerImpl::run upper in stack). There is no sense to wait task completion
    }

    //since mutex is locked and m_impl->runningTaskID != timerID, timer handler is not running at the moment
    m_impl->deleteTaskNonSafe( timerID );
}

static const unsigned int ERROR_SKIP_TIMEOUT_MS = 3000;

void TimerManager::run()
{
    QMutexLocker lk( &m_impl->mtx );

    NX_LOG( lit("TimerManager started"), cl_logDEBUG1 );

    while( !m_impl->terminated )
    {
        try
        {
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            for( ;; )
            {
                if( m_impl->timeToTask.empty() )
                    break;

                auto taskIter = m_impl->timeToTask.begin();
                if( currentTime < taskIter->first.first )
                    break;

                auto taskManager = taskIter->second;
                const quint64 timerID = taskIter->first.second;

                /*std::map<quint64, qint64>::size_type erasedCnt =*/ m_impl->taskToTime.erase( timerID );
                //ASSERT2( erasedCnt == 1, "erasedCnt = "<<erasedCnt<<", timerID = "<<timerID<<", task time (UTC) = "<<taskIter->first.first );
                /*map<pair<qint64, quint64>, TimerEventHandler* const>::size_type s1 =*/ m_impl->timeToTask.size();
                m_impl->timeToTask.erase( taskIter );
                //ASSERT2( m_impl->timeToTask.size() == s1-1, "s1 = "<<s1<<", m_impl->timeToTask.size() = "<<m_timeToTask.size() );
                m_impl->runningTaskID = timerID;
                lk.unlock();
                taskManager( timerID );

                lk.relock();

                m_impl->runningTaskID = 0;
                m_impl->cond.wakeAll();    //notifying threads, waiting on joinAndDeleteTimer

                if( m_impl->terminated )
                    break;
            }

            currentTime = QDateTime::currentMSecsSinceEpoch();
            if( m_impl->timeToTask.empty() )
                m_impl->cond.wait( lk.mutex() );
            else if( m_impl->timeToTask.begin()->first.first > currentTime )
                m_impl->cond.wait( lk.mutex(), m_impl->timeToTask.begin()->first.first - currentTime );

            continue;
        }
        catch( exception& e )
        {
            NX_LOG( lit("TimerManager. Error. Exception in %1:%2. %3").arg(QLatin1String(__FILE__)).arg(__LINE__).arg(QLatin1String(e.what())), cl_logERROR );
        }

        m_impl->cond.wait( lk.mutex(), ERROR_SKIP_TIMEOUT_MS );
    }

    NX_LOG( lit("TimerManager stopped"), cl_logDEBUG1 );
}
