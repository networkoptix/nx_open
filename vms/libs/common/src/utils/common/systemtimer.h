/**********************************************************
* 15 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef SYSTEMTIMER_H
#define SYSTEMTIMER_H


class AbstractSystemTimerImpl;

//!Executes callback in some timeout
/*!
    \note Does not require event loop, so it can be used in any application
    \note Timer is single-shot
    \note \a onTimeout method is executed in thread, timer has been created in (thread, \a start has been called in). So, if thread terminated, onTimeout will never be called.
        So, usage of this timer from thread pool is unreliable
*/
class AbstractSystemTimer
{
    friend class AbstractSystemTimerImpl;

public:
    /*!
        Initially, timer not started
    */
    AbstractSystemTimer();
    virtual ~AbstractSystemTimer();

    /*!
        \return true, if timer started. If false, one can get error description with SystemError::getLastOSErrorCode()
    */
    bool start( unsigned int timeoutMillis );
    //!Cancel timer. \a onTimeout is not called
    /*!
        \return true, if timer has been cancelled. false, otherwise
    */
    bool cancel();

protected:
    //!Called on timer triggered
    virtual void onTimeout() = 0;

private:
    AbstractSystemTimerImpl* m_impl;
};

class SystemTimer
:
    public AbstractSystemTimer
{
public:
    SystemTimer()
    :
        m_handler( 0 )
    {
    }

    virtual ~SystemTimer()
    {
        delete m_handler;
        m_handler = 0;
    }

    /*!
        \param func Functor to be executed on timer
    */
    template<class Func>
        bool start( Func func, unsigned int timeoutMillis )
    {
        typedef TimerHandler<Func> NewHandlerType;

        cancel();
        delete m_handler;
        m_handler = new NewHandlerType( func );
        return AbstractSystemTimer::start( timeoutMillis );
    }

protected:
    virtual void onTimeout() override
    {
        if( !m_handler )
            return;
        m_handler->onTimer();
    }

private:
    class AbstractTimerHandler
    {
    public:
        AbstractTimerHandler() {}
        virtual ~AbstractTimerHandler() {}
        virtual void onTimer() = 0;
    };

    template<class Func>
    class TimerHandler
    :
        public AbstractTimerHandler
    {
    public:
        TimerHandler( Func func )
        :
            m_func( func )
        {
        }

        virtual void onTimer() override
        {
            m_func();
        }

    private:
        Func m_func;
    };

    AbstractTimerHandler* m_handler;
};

#endif  //SYSTEMTIMER_H
