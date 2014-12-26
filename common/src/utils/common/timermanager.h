/**********************************************************
* 16 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef TIMERMANAGER_H
#define TIMERMANAGER_H

#include <functional>

#include <QtCore/QThread>

#include <utils/common/singleton.h>

//!Interface of receiver of timer events
class TimerEventHandler
{
public:
    virtual ~TimerEventHandler() {}

    //!Called on timer expire
    /*!
        \param timerID 
    */
    virtual void onTimer( const quint64& timerID ) = 0;
};

class TimerManagerImpl;

//!Timer events scheduler
/*!
    Holds internal thread which calls method TimerEventHandler::onTimer
    \note If some handler executes \a TimerEventHandler::onTimer for long time, some tasks can be called with delay, since all tasks are processed by a single thread
    \note This timer require nor event loop (as QTimer), nor it's thread-dependent (as SystemTimer), but it delivers timer events to internal thread,
        so additional synchronization in timer event handler may be required
*/
class TimerManager : public QThread, public Singleton<TimerManager>
{
public:
    /*!
        Launches internal thread
    */
    TimerManager();
    virtual ~TimerManager();

    /*!
        \param taskHandler
        \param delay Timeout (millisecond) to call taskHandler->onTimer()
        \return ID of created timer. Always non-zero
    */
    quint64 addTimer(
        TimerEventHandler* const taskHandler,
        const unsigned int delay );
    //!Same as above but accepts handler of \a std::function type
    quint64 addTimer(
        std::function<void(quint64)> taskHandler,
        const unsigned int delay );
    /*!
        If task is already running, it can be still running after method return
        If timer handler is being executed at the moment, it can still be executed after return of this method
        \param timerID ID of timer, created by \a addTimer call. If no such timer, nothing is done
    */
    void deleteTimer( const quint64& timerID );
    //!Delete timer and wait for handler execution (if its is being executed)
    /*!
        This method garantees that timer \a timerID handler is not being executed after return of this method.

        It is recommended to use previous method, if appropriate, since this method is some more havier.

        \param timerID ID of timer, created by \a addTimer call. If no such timer, nothing is done
        \note If this method is called from \a TimerEventHandler::onTimer to delete timer being executed, nothing is done
    */
    void joinAndDeleteTimer( const quint64& timerID );

protected:
    virtual void run();

private:
    TimerManagerImpl* m_impl;
};

#endif //TIMERMANAGER_H
