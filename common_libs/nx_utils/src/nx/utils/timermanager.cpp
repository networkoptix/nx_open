/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#include "timermanager.h"

#include <map>
#include <string>

#include <QtCore/QAtomicInt>
#include <QtCore/QElapsedTimer>

#include "log/log.h"
#include "thread/mutex.h"
#include "thread/wait_condition.h"


using namespace std;

struct TaskContext
{
    nx::utils::MoveOnlyFunc<void(quint64)> func;
    bool singleShot;
    std::chrono::milliseconds delay;
};

class TimerManagerImpl
{
public:
    QnWaitCondition cond;
    mutable QnMutex mtx;
    //!map<pair<time, timerID>, handler>
    std::map<std::pair<qint64, quint64>, TaskContext> timeToTask;
    //!map<timerID, time>
    std::map<quint64, qint64> taskToTime;
    bool terminated;
    //!ID of task, being executed. 0, if no running task
    quint64 runningTaskID;
    QElapsedTimer monotonicClock;

    TimerManagerImpl()
    :
        terminated( false ),
        runningTaskID( 0 )
    {
        monotonicClock.restart();
    }

    void addTaskNonSafe(
        const quint64 timerID,
        nx::utils::MoveOnlyFunc<void( quint64 )> taskHandler,
        std::chrono::milliseconds delay,
        std::chrono::milliseconds firstShotDelay,
        bool singleShot)
    {
        const qint64 taskTime = monotonicClock.elapsed() + firstShotDelay.count();

        if( !timeToTask.insert( make_pair(
                make_pair( taskTime, timerID ),
                TaskContext{std::move(taskHandler), singleShot, delay} ) ).second )
        {
            //ASSERT( false );
        }
        if( !taskToTime.insert( make_pair( timerID, taskTime ) ).second )
        {
            //ASSERT( false );
        }

        cond.wakeOne();
    }

    void deleteTaskNonSafe( const quint64 timerID )
    {
        const map<quint64, qint64>::iterator it = taskToTime.find( timerID );
        if( it == taskToTime.end() )
            return;

        timeToTask.erase( make_pair( it->second, it->first ) );
        taskToTime.erase( it );
    }

    uint64_t generateNextTimerId()
    {
        static QAtomicInt lastTaskID = 0;

        quint64 timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
        if (timerID == 0)
            timerID = lastTaskID.fetchAndAddOrdered(1) + 1;
        return timerID;
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
    stop();
    delete m_impl;
    m_impl = NULL;
}

void TimerManager::stop()
{
    {
        QnMutexLocker lk( &m_impl->mtx );
        m_impl->terminated = true;
        m_impl->cond.wakeAll();
    }

    wait();
}

quint64 TimerManager::addTimer(
    TimerEventHandler* const taskManager,
    const unsigned int delay )
{
    using namespace std::placeholders;
    return addTimer( std::bind( &TimerEventHandler::onTimer, taskManager, _1 ), delay );
}

quint64 TimerManager::addTimer(
    nx::utils::MoveOnlyFunc<void(quint64)> taskHandler,
    const unsigned int delay )
{
    auto timerId = m_impl->generateNextTimerId();

    QnMutexLocker lk( &m_impl->mtx );
    m_impl->addTaskNonSafe(
        timerId,
        std::move(taskHandler),
        std::chrono::milliseconds(delay),
        std::chrono::milliseconds(delay),
        true);
    NX_LOGX(lm("Added timer %1, delay %2 ms").arg(timerId).arg(delay), cl_logDEBUG2);
    return timerId;
}

quint64 TimerManager::addTimer(
    nx::utils::MoveOnlyFunc<void(quint64)> taskHandler,
    std::chrono::milliseconds delay)
{
    return addTimer(
        std::move(taskHandler),
        delay.count());
}

quint64 TimerManager::addNonStopTimer(
    nx::utils::MoveOnlyFunc<void(quint64)> taskHandler,
    TimerDuration delay,
    TimerDuration firstShotDelay)
{
    auto timerId = m_impl->generateNextTimerId();

    QnMutexLocker lk(&m_impl->mtx);
    m_impl->addTaskNonSafe(
        timerId,
        std::move(taskHandler),
        delay,
        firstShotDelay,
        false);
    NX_LOGX(lm("Added non stop timer %1, delay %2 ms, first shot delay %3")
        .arg(timerId).arg(delay).arg(firstShotDelay), cl_logDEBUG2);
    return timerId;
}

bool TimerManager::modifyTimerDelay(
    quint64 timerID,
    const unsigned int newDelayMillis )
{
    NX_LOGX(lm("Modifying timer %1, new delay %2 ms")
        .arg(timerID).arg(newDelayMillis), cl_logDEBUG2);

    QnMutexLocker lk( &m_impl->mtx );
    if( m_impl->runningTaskID == timerID )
        return false; //timer being executed at the moment

    //fetching handler
    auto taskIter = m_impl->taskToTime.find( timerID );
    if( taskIter == m_impl->taskToTime.end() )
        return false;   //no timer with requested id
    auto handlerIter = m_impl->timeToTask.find( std::make_pair( taskIter->second, timerID ) );
    NX_ASSERT( handlerIter != m_impl->timeToTask.end() );
    auto taskHandler = std::move(handlerIter->second.func);
    const bool singleShot = handlerIter->second.singleShot;

    m_impl->taskToTime.erase( taskIter );
    m_impl->timeToTask.erase( handlerIter );

    NX_LOGX(lm("Modifyed timer %1, new delay %2 ms")
        .arg(timerID).arg(newDelayMillis), cl_logDEBUG2);

    m_impl->addTaskNonSafe(
        timerID,
        std::move(taskHandler),
        std::chrono::milliseconds(newDelayMillis),
        std::chrono::milliseconds(newDelayMillis),
        singleShot);
    return true;
}

bool TimerManager::modifyTimerDelay(quint64 timerID, TimerDuration delay)
{
    return modifyTimerDelay(timerID, delay.count());
}

void TimerManager::deleteTimer( const quint64& timerID )
{
    QnMutexLocker lk( &m_impl->mtx );

    NX_LOGX(lm("Deleting timer %1").arg(timerID), cl_logDEBUG2);

    m_impl->deleteTaskNonSafe( timerID );
}

void TimerManager::joinAndDeleteTimer( const quint64& timerID )
{
    QnMutexLocker lk( &m_impl->mtx );
    //having locked \a m_impl->mtx we garantee, that execution of timer timerID will not start

    if( QThread::currentThread() != this )
    {
        NX_LOGX(lm("Waiting for timer %1 to complete").arg(timerID), cl_logDEBUG2);

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
    QnMutexLocker lk( &m_impl->mtx );

    NX_LOG( lit("TimerManager started"), cl_logDEBUG1 );

    while( !m_impl->terminated )
    {
        try
        {
            qint64 currentTime = m_impl->monotonicClock.elapsed();
            for( ;; )
            {
                if( m_impl->timeToTask.empty() )
                    break;

                auto taskIter = m_impl->timeToTask.begin();
                if( currentTime < taskIter->first.first )
                    break;

                const quint64 timerID = taskIter->first.second;
                auto taskContext = std::move(taskIter->second);

                m_impl->taskToTime.erase( timerID );
                m_impl->timeToTask.erase( taskIter );
                m_impl->runningTaskID = timerID;
                lk.unlock();

                NX_LOGX(lm("Executing task %1").arg(timerID), cl_logDEBUG2);
                taskContext.func( timerID );
                NX_LOGX(lm("Done task %1").arg(timerID), cl_logDEBUG2);

                lk.relock();

                if (!taskContext.singleShot)
                    m_impl->addTaskNonSafe(
                        timerID,
                        std::move(taskContext.func),
                        taskContext.delay,
                        taskContext.delay,
                        false);

                m_impl->runningTaskID = 0;
                m_impl->cond.wakeAll();    //notifying threads, waiting on joinAndDeleteTimer

                lk.unlock();
                //giving chance to another thread to remove task
                lk.relock();

                if( m_impl->terminated )
                    break;
            }

            currentTime = m_impl->monotonicClock.elapsed();
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




TimerManager::TimerGuard::TimerGuard()
:
    m_timerID( 0 )
{
}

TimerManager::TimerGuard::TimerGuard( quint64 timerID )
:
    m_timerID( timerID )
{
}

TimerManager::TimerGuard::TimerGuard( TimerGuard&& right )
:
    m_timerID( right.m_timerID )
{
    right.m_timerID = 0;
}

TimerManager::TimerGuard::~TimerGuard()
{
    reset();
}

TimerManager::TimerGuard& TimerManager::TimerGuard::operator=( TimerManager::TimerGuard&& right )
{
    if( &right == this )
        return *this;

    reset();

    m_timerID = right.m_timerID;
    right.m_timerID = 0;
    return *this;
}

//!Cancels timer and blocks until running handler returns
void TimerManager::TimerGuard::reset()
{
    if( !m_timerID )
        return;
    TimerManager::instance()->joinAndDeleteTimer( m_timerID );
    m_timerID = 0;
}

quint64 TimerManager::TimerGuard::get() const
{
    return m_timerID;
}

quint64 TimerManager::TimerGuard::release()
{
    const auto result = m_timerID;
    m_timerID = 0;
    return result;
}

TimerManager::TimerGuard::operator bool_type() const
{
    return m_timerID ? &TimerGuard::this_type_does_not_support_comparisons : 0;
}

bool TimerManager::TimerGuard::operator==( const TimerManager::TimerGuard& right ) const
{
    return m_timerID == right.m_timerID;
}

bool TimerManager::TimerGuard::operator!=( const TimerManager::TimerGuard& right ) const
{
    return m_timerID != right.m_timerID;
}

TimerDuration parseTimerDuration( const QString& duration,
                                  TimerDuration defaultValue )
{
    std::chrono::milliseconds res;
    bool ok(true);
    const auto toUInt = [ & ]( int len )
    {
        return len ? duration.left( duration.length() - len ).toULongLong( &ok )
                   : duration.toULongLong( &ok );
    };

    if ( duration.endsWith( lit("ms"), Qt::CaseInsensitive ) )
        res = std::chrono::milliseconds( toUInt( 2 ) );
    else
    if ( duration.endsWith( lit("s"), Qt::CaseInsensitive ) )
        res = std::chrono::seconds( toUInt( 1 ) );
    else
    if ( duration.endsWith( lit("m"), Qt::CaseInsensitive ) )
        res = std::chrono::minutes( toUInt( 1 ) );
    else
    if ( duration.endsWith( lit("h"), Qt::CaseInsensitive ) )
        res = std::chrono::hours( toUInt( 1 ) );
    else
    if ( duration.endsWith( lit("d"), Qt::CaseInsensitive ) )
        res = std::chrono::hours( toUInt( 1 ) ) * 24;
    else
        res = std::chrono::seconds( toUInt( 0 ) );

    return ( ok && res.count() ) ? res : defaultValue;
}
