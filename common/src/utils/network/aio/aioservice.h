/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOSERVICE_H
#define AIOSERVICE_H

#include <memory>

#include <boost/optional.hpp>

#include "aioeventhandler.h"
#include "aiothread.h"
#include "pollset.h"
#include "../udt_socket.h"


namespace aio
{
    //!Monitors multiple sockets for asynchronous events and triggers handler (\a AIOEventHandler) on event
    /*!
        Holds multiple threads inside, handler triggered from random thread
        \note Internal thread count can be increased dynamically, since this class uses PollSet class which can monitor only limited number of sockets
        \note Suggested use of this class: little add/remove, many notifications
        \note Single socket is always listened for all requested events by the same thread
        \note Currently, win32 implementation uses select, so it is far from being efficient. Linux implementation uses epoll, BSD - kqueue
        \note All methods are thread-safe
        \note All methods are not blocking except \a AIOService::removeFromWatch called with \a waitForRunningHandlerCompletion set to \a true
        \todo Socket termination???
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

        //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
        /*!
            \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
            \note if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported
            \note If not called from aio thread \a sock can be added to event loop with some delay
        */
        template<class SocketType>
        bool watchSocket(
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            std::function<void()> socketAddedToPollHandler = std::function<void()>() )
        {
            QMutexLocker lk( &m_mutex );
            return watchSocketNonSafe( sock, eventToWatch, eventHandler, socketAddedToPollHandler );
        }

        //!Cancel monitoring \a sock for event \a eventType
        /*!
            \param waitForRunningHandlerCompletion \a true garantees that no \a aio::AIOEventHandler::eventTriggered will be called after return of this method
            and all running handlers have returned. But this MAKES METHOD BLOCKING and, as a result, this MUST NOT be called from aio thread
            It is strongly recommended to set this parameter to \a false.

            \note If \a waitForRunningHandlerCompletion is \a false events that are already posted to the queue can be called \b after return of this method
            \note If this method is called from asio thread, \a sock is processed in (e.g., from event handler associated with \a sock),
            this method does not block and always works like \a waitForRunningHandlerCompletion has been set to \a true
        */
        template<class SocketType>
        void removeFromWatch(
            SocketType* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion = true )
        {
            QMutexLocker lk( &m_mutex );
            return removeFromWatchNonSafe( sock, eventType, waitForRunningHandlerCompletion );
        }

        //!Register timeout, associated with socket \a sock
        /*!
            \a eventHandler->eventTriggered( \a sock, aio::etTimedOut ) will be called every \a timeoutMillis milliseconds until cancelled with \a aio::AIOService::removeFromWatch( \a sock, \a aio::etTimedOut )
        */
        template<class SocketType, class HandlerType>
        bool registerTimer(
            SocketType* const sock,
            unsigned int timeoutMillis,
            AIOEventHandler<SocketType>* const eventHandler )
        {
            QMutexLocker lk( &m_mutex );
            return watchSocketNonSafe( sock, aio::etTimedOut, eventHandler, timeoutMillis );
        }

        //!Returns \a true, if socket is still listened for state changes
        template<class SocketType>
        bool isSocketBeingWatched( SocketType* sock ) const
        {
            const SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            QMutexLocker lk( &m_mutex );
            const auto& it = aioHandlingContext.sockets.lower_bound( std::make_pair( sock, aio::etNone ) );
            return it != aioHandlingContext.sockets.end() && it->first.first == sock;
        }

        QMutex* mutex() const { return &m_mutex; }

        //!Same as \a AIOService::watchSocket, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        /*!
            \param socketAddedToPollHandler Called after socket has been added to pollset but before pollset.poll has been called
        */
        template<class SocketType>
        bool watchSocketNonSafe(
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            boost::optional<unsigned int> timeoutMillis = boost::optional<unsigned int>(),
            std::function<void()> socketAddedToPollHandler = std::function<void()>() )
        {
            SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            if( !timeoutMillis )
            {
                unsigned int sockTimeoutMS = 0;
                if( eventToWatch == aio::etRead )
                {
                    if( !sock->getRecvTimeout( &sockTimeoutMS ) )
                        return false;
                }
                else if( eventToWatch == aio::etWrite )
                {
                    if( !sock->getSendTimeout( &sockTimeoutMS ) )
                        return false;
                }
                else
                {
                    assert( false );
                }
                timeoutMillis = sockTimeoutMS;
            }

            //checking, if that socket is already monitored
            const std::pair<SocketType*, aio::EventType>& sockCtx = std::make_pair( sock, eventToWatch );
            //checking if sock is already polled (even for another event)
            auto closestSockIter = aioHandlingContext.sockets.lower_bound( std::make_pair( sock, (aio::EventType)0 ) );
            auto sameSockAndEventIter = closestSockIter;
            for( ; sameSockAndEventIter != aioHandlingContext.sockets.end(); ++sameSockAndEventIter )
            {
                if( sameSockAndEventIter->first.first != sock )
                {
                    sameSockAndEventIter = aioHandlingContext.sockets.end();
                    break;
                }
                else if( sameSockAndEventIter->first.second == eventToWatch )
                {
                    break;
                }
            }
            if( sameSockAndEventIter != aioHandlingContext.sockets.end() && sameSockAndEventIter->second.second == timeoutMillis )
                return true;    //socket already monitored for eventToWatch

            if( (closestSockIter != aioHandlingContext.sockets.end()) && (closestSockIter->first.first == sockCtx.first) )  //same socket is already polled
            {
                if( sameSockAndEventIter != aioHandlingContext.sockets.end() )
                {
                    //socket is already polled for this event but with another timeout. Just changing timeout
                    if( sameSockAndEventIter->second.first->changeSocketTimeout( sock, eventToWatch, eventHandler, timeoutMillis.get() ) )
                    {
                        sameSockAndEventIter->second.second = timeoutMillis.get();
                        return true;
                    }
                }
                else
                {
                    //socket is already polled for other event. Trying to use same thread
                    if( closestSockIter->second.first->watchSocket( sock, eventToWatch, eventHandler, timeoutMillis.get() ) )
                    {
#ifdef _DEBUG
                        if( !aioHandlingContext.sockets.insert( std::make_pair( sockCtx, std::make_pair( closestSockIter->second.first, timeoutMillis.get() ) ) ).second )
                            assert( false );
#else
                        aioHandlingContext.sockets.insert( closestSockIter, std::make_pair( sockCtx, std::make_pair( closestSockIter->second.first, timeoutMillis.get() ) ) );
#endif
                        return true;
                    }
                }
                assert( false );    //we MUST use same thread for polleding all events on single socket
                SystemError::setLastErrorCode( SystemError::connectionAbort );
                return false;
            }

#ifdef _DEBUG
            auto checkIter = aioHandlingContext.sockets.lower_bound( std::make_pair( sock, (aio::EventType)0 ) );
            assert( checkIter == aioHandlingContext.sockets.end() || checkIter->first.first != sock );
#endif

            //searching for a least-used thread, which is ready to accept
            typename SocketAIOContext<SocketType>::AIOThreadType* threadToUse = nullptr;
            for( auto
                threadIter = aioHandlingContext.aioThreadPool.cbegin();
                threadIter != aioHandlingContext.aioThreadPool.cend();
                ++threadIter )
            {
                if( !(*threadIter)->canAcceptSocket( sock ) )
                    continue;
                if( threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled() )
                    continue;
                threadToUse = *threadIter;
            }

            if( !threadToUse )
            {
                //creating new thread
                std::unique_ptr<typename SocketAIOContext<SocketType>::AIOThreadType> newThread( new typename SocketAIOContext<SocketType>::AIOThreadType( &m_mutex ) );
                newThread->start();
                if( !newThread->isRunning() )
                    return false;
                threadToUse = newThread.get();
                aioHandlingContext.aioThreadPool.push_back( newThread.release() );
            }

            if( threadToUse->watchSocket( sock, eventToWatch, eventHandler, timeoutMillis.get(), socketAddedToPollHandler ) )
            {
                if( !aioHandlingContext.sockets.insert( std::make_pair( sockCtx, std::make_pair( threadToUse, timeoutMillis.get() ) ) ).second )
                    assert( false );
                return true;
            }

            return false;
        }

        //!Same as \a AIOService::removeFromWatch, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        template<class SocketType>
        void removeFromWatchNonSafe(
            SocketType* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion = true )
        {
            SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            const std::pair<SocketType*, aio::EventType>& sockCtx = std::make_pair( sock, eventType );
            auto it = aioHandlingContext.sockets.find( sockCtx );
            if( it != aioHandlingContext.sockets.end() )
            {
                if( it->second.first->removeFromWatch( sock, eventType, waitForRunningHandlerCompletion ) )
                    aioHandlingContext.sockets.erase( it );
            }
        }

        /*!
            \param threadCount minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
                If zero, thread count is choosed automatically. This value is used only in first call 
        */
        static AIOService* instance( unsigned int threadCount = 0 );

    private:
        template<class SocketType>
        struct SocketAIOContext
        {
            typedef AIOThread<SocketType> AIOThreadType;

            std::list<AIOThreadType*> aioThreadPool;
            //!map<pair<socket, event_type>, pair<thread, socket timeout>>
            std::map<std::pair<SocketType*, aio::EventType>, std::pair<AIOThreadType*, unsigned int> > sockets;
        };

        SocketAIOContext<Socket> m_systemSocketAIO;
        SocketAIOContext<UdtSocket> m_udtSocketAIO;
        mutable QMutex m_mutex;

        template<class SocketType> SocketAIOContext<SocketType>& getAIOHandlingContext();
        template<class SocketType> const SocketAIOContext<SocketType>& getAIOHandlingContext() const;// { static_assert( false, "Bad socket type" ); }
        template<class SocketType> void initializeAioThreadPool(SocketAIOContext<SocketType>* aioCtx, unsigned int threadCount)
        {
            typedef typename SocketAIOContext<SocketType>::AIOThreadType AIOThreadType;

            for( unsigned int i = 0; i < threadCount; ++i )
            {
                std::unique_ptr<AIOThreadType> thread(new AIOThreadType(&m_mutex));
                thread->start();
                if( !thread->isRunning() )
                    continue;
                aioCtx->aioThreadPool.push_back(thread.release());
            }
        }
    };
}

#endif  //AIOSERVICE_H
