/**********************************************************
* 16 dec 2014
* a.kolesnikov
***********************************************************/

#ifndef ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
#define ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QWaitCondition>


//!QObject's successors which allow using Qt::DirectConnection to connect object living in different thread should inherit this class
template<class Derived>
class EnableMultiThreadDirectConnection
{
public:
    class WhileExecutingDirectCall
    {
    public:
        WhileExecutingDirectCall( EnableMultiThreadDirectConnection* const caller )
        :
            m_caller( caller )
        {
            m_caller->beforeDirectCall();
        }

        ~WhileExecutingDirectCall()
        {
            m_caller->afterDirectCall();
        }

    private:
        EnableMultiThreadDirectConnection* const m_caller;
    };

    EnableMultiThreadDirectConnection()
    :
        m_ongoingCalls( 0 )
    {
    }

    virtual ~EnableMultiThreadDirectConnection() {}

    //!Disconnects \a receiver from all signals of this class and waits for issued \a emit calls connected to \a receiver to return
    /*!
        Can be called from any thread
    */
    void disconnectAndJoin( QObject* receiver )
    {
        QObject::disconnect( static_cast<Derived*>(this), nullptr, receiver, nullptr );
        QMutexLocker lk( &m_signalEmitMutex );  //waiting for signals to be emitted in other threads to be processed
        while( m_ongoingCalls > 0 )
            m_cond.wait( lk.mutex() );
    }

private:
    mutable QMutex m_signalEmitMutex;
    QWaitCondition m_cond;
    mutable size_t m_ongoingCalls;

    void beforeDirectCall()
    {
        QMutexLocker lk( &m_signalEmitMutex );
        ++m_ongoingCalls;
    }

    void afterDirectCall()
    {
        QMutexLocker lk( &m_signalEmitMutex );
        --m_ongoingCalls;
        m_cond.wakeAll();
    }
};

#endif  //ENABLE_MULTI_THREAD_DIRECT_CONNECTION_H
