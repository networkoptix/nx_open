/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOTHREAD_H
#define AIOTHREAD_H

#include "aioeventhandler.h"
#include "pollset.h"
#include "../../common/long_runnable.h"


namespace aio
{
    class AIOThreadImpl;

    /*!
        This class is intended for use only with aio::AIOService
        \todo make it nested in aio::AIOService?
        \note All methods, except for \a pleaseStop(), must be called with \a mutex locked
    */
    class AIOThread
    :
        public QnLongRunnable
    {
    public:
        /*!
            \param mutex Mutex to use for exclusive access to internal data
        */
        AIOThread( QMutex* const mutex );
        virtual ~AIOThread();

        //!Implementation of QnLongRunnable::pleaseStop
        virtual void pleaseStop() override;

        //!Monitor socket \a sock for event \a eventToWatch occurence and trigger \a eventHandler on event
        /*!
            \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
        */
        bool watchSocket(
            const QSharedPointer<Socket>& sock,
            PollSet::EventType eventToWatch,
            AIOEventHandler* const eventHandler );
        //!Do not monitor \a sock for event \a eventType
        /*!
            Garantees that no \a eventTriggered will be called after return of this method
        */
        void removeFromWatch( const QSharedPointer<Socket>& sock, PollSet::EventType eventType );
        //!Returns number of sockets monitored for \a eventToWatch event
        size_t size( PollSet::EventType eventToWatch ) const;
        //!Returns true, if can monitor one more socket for \a eventToWatch
        bool canAcceptSocket( PollSet::EventType eventToWatch ) const;

    protected:
        //!Implementation of QThread::run
        virtual void run() override;

    private:
        AIOThreadImpl* m_impl;
    };
}

#endif  //AIOTHREAD_H
