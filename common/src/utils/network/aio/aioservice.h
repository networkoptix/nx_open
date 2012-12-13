/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOSERVICE_H
#define AIOSERVICE_H

#include <QMutex>
#include <QSharedPointer>

#include "aioeventhandler.h"
#include "pollset.h"


namespace aio
{
    class AIOThread;

    //!Monitors multiple sockets for asynchronous events and triggers handler (\a AIOEventHandler) on event
    /*!
        Holds multiple threads inside, handler triggered from random thread
        \note Internal thread count can be increased dynamically, since this class uses PollSet class which can monitor only limited number of sockets
        \note Suggested use of this class: little add/remove, many notifications
        \note There is no garantee that one socket is listened for read and write by same thread (this is garanteed with linux/epoll, but not with winxp/select)
        \note All methods are thread-safe
        \todo Socket termination???
        \todo add timeouts to i/o operations
    */
    class AIOService
    {
    public:
        /*!
            After object instanciation one must call \a isInitialized to check whether instanciation was a success
            \param threadCount This is minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
                If zero, thread count is choosed automatically
        */
        AIOService( unsigned int threadCount = 0 );
        virtual ~AIOService();

        //!Returns true, if object has been successfully initialized
        bool isInitialized() const;
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

        /*!
            \param threadCount minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
                If zero, thread count is choosed automatically. This value is used only in first call 
        */
        static AIOService* instance( unsigned int threadCount = 0 );

    private:
        class SocketMonitorContext
        {
        public:
            AIOThread* thread;
            unsigned int monitoredEvents;
        };

        std::list<AIOThread*> m_threadPool;
        std::map<std::pair<Socket*, PollSet::EventType>, AIOThread*> m_sockets;
        mutable QMutex m_mutex;
    };
}

#endif  //AIOSERVICE_H
