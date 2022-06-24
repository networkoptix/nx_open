// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>


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
        //waiting for signals to be emitted in other threads to be processed
        NX_MUTEX_LOCKER lk( &m_signalEmitMutex );
        while( m_ongoingCalls > 0 )
            m_cond.wait( lk.mutex() );
    }

private:
    mutable nx::Mutex m_signalEmitMutex;
    nx::WaitCondition m_cond;
    mutable size_t m_ongoingCalls;

    void beforeDirectCall()
    {
        NX_MUTEX_LOCKER lk( &m_signalEmitMutex );
        ++m_ongoingCalls;
    }

    void afterDirectCall()
    {
        NX_MUTEX_LOCKER lk( &m_signalEmitMutex );
        --m_ongoingCalls;
        m_cond.wakeAll();
    }
};
