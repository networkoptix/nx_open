/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef AIOSERVICE_H
#define AIOSERVICE_H

#include <atomic>
#include <memory>
#include <atomic>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/singleton.h>

#include "aioeventhandler.h"
#include "aiothread.h"
#include "pollset.h"
#include "../udt/udt_socket.h"

namespace nx {
namespace network {

class SocketGlobals;

namespace aio {
    //TODO #ak think about removing sockets dictionary and mutex (only in release, probably). some data from dictionary can be moved to socket

    //!Monitors multiple sockets for asynchronous events and triggers handler (\a AIOEventHandler) on event
    /*!
        Holds multiple threads inside, handler triggered from random thread
        \note Internal thread count can be increased dynamically, since this class uses PollSet class which can monitor only limited number of sockets
        \note Suggested use of this class: little add/remove, many notifications
        \note Single socket is always listened for all requested events by the same thread
        \note Currently, win32 implementation uses select, so it is far from being efficient. Linux implementation uses epoll, BSD - kqueue
        \note All methods are thread-safe
        \note All methods are not blocking except \a AIOService::removeFromWatch called with \a waitForRunningHandlerCompletion set to \a true
    */
    class NX_NETWORK_API AIOService
    {
        /*!
            After object instanciation one must call \a isInitialized to check whether instanciation was a success
            \param threadCount This is minimal thread count. Actual thread poll may exceed this value because PollSet can monitor limited number of sockets.
            If zero, thread count is choosed automatically
        */
        AIOService( unsigned int threadCount = 0 );
        virtual ~AIOService();
        friend class ::nx::network::SocketGlobals;

    public:
        //!Returns true, if object has been successfully initialized
        bool isInitialized() const;

        //!Monitor socket \a sock for event \a eventToWatch occurrence and trigger \a eventHandler on event
        /*!
            \return true, if added successfully. If \a false, error can be read by \a SystemError::getLastOSErrorCode() function
            \note if no event in corresponding socket timeout (if not 0), then aio::etTimedOut event will be reported
            \note If not called from aio thread \a sock can be added to event loop with some delay
        */
        template<class SocketType>
        void watchSocket(
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            std::function<void()> socketAddedToPollHandler = std::function<void()>())
        {
            QnMutexLocker lk( &m_mutex );
            watchSocketNonSafe(
                &lk,
                sock,
                eventToWatch,
                eventHandler,
                std::move(socketAddedToPollHandler));
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
            bool waitForRunningHandlerCompletion = true,
            std::function<void()> pollingStoppedHandler = std::function<void()>())
        {
            QnMutexLocker lk( &m_mutex );
            removeFromWatchNonSafe(
                &lk,
                sock,
                eventType,
                waitForRunningHandlerCompletion, 
                std::move(pollingStoppedHandler));
        }

        //!Register timeout, associated with socket \a sock
        /*!
            \a eventHandler->eventTriggered( \a sock, aio::etTimedOut ) will be called every \a timeoutMillis milliseconds until cancelled with \a aio::AIOService::removeFromWatch( \a sock, \a aio::etTimedOut )
        */
        template<class SocketType, class HandlerType>
        void registerTimer(
            SocketType* const sock,
            std::chrono::milliseconds timeoutMillis,
            AIOEventHandler<SocketType>* const eventHandler )
        {
            QnMutexLocker lk( &m_mutex );
            watchSocketNonSafe(
                &lk,
                sock,
                aio::etTimedOut,
                eventHandler,
                timeoutMillis.count());
        }

        //!Returns \a true, if socket is still listened for state changes
        template<class SocketType>
        bool isSocketBeingWatched( SocketType* sock ) const
        {
            const SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            QnMutexLocker lk( &m_mutex );
            const auto& it = aioHandlingContext.sockets.lower_bound( std::make_pair( sock, aio::etNone ) );
            return it != aioHandlingContext.sockets.end() && it->first.first == sock;
        }

        //!Call \a handler from within aio thread \a sock is bound to
        /*!
            \note Call will always be queued. I.e., if called from handler running in aio thread, it will be called after handler has returned
            \warning Currently, there is no way to find out whether call has been posted or being executed currently
        */
        template<class SocketType, class Handler>
        void post(SocketType* sock, Handler handler)
        {
            QnMutexLocker lk( &m_mutex );
            postNonSafe(&lk, sock, std::move(handler));
        }

        //!Calls \a handler in random aio thread
        template<class Handler>
        void post(Handler handler)
        {
            QnMutexLocker lk(&m_mutex);
            //if sock is not still bound to aio thread, binding it
            auto& threadToUse = m_systemSocketAIO.aioThreadPool[rand() % m_systemSocketAIO.aioThreadPool.size()];
            assert(threadToUse);
            lk.unlock();
            threadToUse->post(nullptr, std::move(handler));
        }

        //!Call \a handler from within aio thread \a sock is bound to
        /*!
            \note If called in aio thread, handler will be called from within this method, otherwise - queued like \a aio::AIOService::post does
        */
        template<class SocketType, class Handler>
        void dispatch( SocketType* sock, Handler handler )
        {
            auto* aioThread = getSocketAioThread(sock);
            aioThread->dispatch(sock, std::move(handler));
        }

        //TODO #ak better remove this method, violates encapsulation
        QnMutex* mutex() const { return &m_mutex; }

        template<class SocketType>
        aio::AIOThread<SocketType>* getSocketAioThread(SocketType* sock)
        {
            QnMutexLocker lk(&m_mutex);
            return getSocketAioThread(&lk, sock);
        }

        template<class SocketType>
        void bindSocketToAioThread(SocketType* sock, AbstractAioThread* aioThread)
        {
            const auto desired = dynamic_cast<aio::AIOThread<SocketType>*>(aioThread);
            Q_ASSERT_X(desired, Q_FUNC_INFO, "Inappropriate AIO thread type");

            aio::AIOThread<SocketType>* expected = nullptr;
            if(!sock->impl()->aioThread.compare_exchange_strong(expected, desired))
            {
                Q_ASSERT_X(false, Q_FUNC_INFO,
                           "Socket is already bound to some AIO thread");
            }
        }

        //!Same as \a AIOService::watchSocket, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        /*!
            \param socketAddedToPollHandler Called after socket has been added to pollset but before pollset.poll has been called
        */
        template<class SocketType>
        void watchSocketNonSafe(
            QnMutexLockerBase* const lock,
            SocketType* const sock,
            aio::EventType eventToWatch,
            AIOEventHandler<SocketType>* const eventHandler,
            boost::optional<std::chrono::milliseconds> timeoutMillis = boost::optional<std::chrono::milliseconds>(),
            std::function<void()> socketAddedToPollHandler = std::function<void()>() )
        {
            SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            if( !timeoutMillis )
            {
                unsigned int sockTimeoutMS = 0;
                if( eventToWatch == aio::etRead )
                {
                    if( !sock->getRecvTimeout( &sockTimeoutMS ) )
                    {
                        //reporting error via event thread
                        postNonSafe(
                            lock,
                            sock,
                            std::bind(
                                &AIOEventHandler<SocketType>::eventTriggered,
                                eventHandler,
                                sock,
                                aio::etError));
                        return;
                    }
                }
                else if( eventToWatch == aio::etWrite )
                {
                    if( !sock->getSendTimeout( &sockTimeoutMS ) )
                    {
                        //reporting error via event thread
                        postNonSafe(
                            lock,
                            sock,
                            std::bind(
                                &AIOEventHandler<SocketType>::eventTriggered,
                                eventHandler,
                                sock,
                                aio::etError));
                        return;
                    }
                }
                else
                {
                    assert( false );
                }
                timeoutMillis = std::chrono::milliseconds(sockTimeoutMS);
            }

            //checking, if that socket is already polled
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
            if( sameSockAndEventIter != aioHandlingContext.sockets.end() &&
                sameSockAndEventIter->second.second == timeoutMillis.get() )
            {
                return;    //socket already monitored for eventToWatch
            }

            if( (closestSockIter != aioHandlingContext.sockets.end()) && (closestSockIter->first.first == sockCtx.first) )  //same socket is already polled
            {
                if( sameSockAndEventIter != aioHandlingContext.sockets.end() )
                {
                    //socket is already polled for this event but with another timeout. Just changing timeout
                    sameSockAndEventIter->second.first->changeSocketTimeout(
                        sock, eventToWatch, eventHandler, timeoutMillis.get() );
                    sameSockAndEventIter->second.second = timeoutMillis.get();
                    return;
                }
            }

            const auto threadToUse = getSocketAioThread(lock, sock);
            if (!aioHandlingContext.sockets.emplace(
                sockCtx,
                std::make_pair(threadToUse, timeoutMillis.get())).second)
            {
                assert(false);
            }
            lock->unlock();
            threadToUse->watchSocket(
                sock,
                eventToWatch,
                eventHandler,
                timeoutMillis.get(),
                std::move(socketAddedToPollHandler));
            lock->relock();
        }

        //!Same as \a AIOService::removeFromWatch, but does not lock mutex. Calling entity MUST lock \a AIOService::mutex() before calling this method
        template<class SocketType>
        void removeFromWatchNonSafe(
            QnMutexLockerBase* const lock,
            SocketType* const sock,
            aio::EventType eventType,
            bool waitForRunningHandlerCompletion = true,
            std::function<void()> pollingStoppedHandler = std::function<void()>())
        {
            SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            const std::pair<SocketType*, aio::EventType>& sockCtx = std::make_pair( sock, eventType );
            auto it = aioHandlingContext.sockets.find( sockCtx );
            if( it == aioHandlingContext.sockets.end() )
                return;

            auto aioThread = it->second.first;
            aioHandlingContext.sockets.erase( it );
            lock->unlock();
            aioThread->removeFromWatch(
                sock,
                eventType,
                waitForRunningHandlerCompletion,
                std::move(pollingStoppedHandler));
            lock->relock();
        }

        template<class SocketType>
        void cancelPostedCalls(
            SocketType* const sock,
            bool waitForRunningHandlerCompletion = true )
        {
            QnMutexLocker lk( &m_mutex );
            cancelPostedCallsNonSafe(&lk, sock, waitForRunningHandlerCompletion);
        }

    private:
        template<class SocketType>
        struct SocketAIOContext
        {
            typedef AIOThread<SocketType> AIOThreadType;

            std::vector<std::unique_ptr<AIOThreadType>> aioThreadPool;
            //!map<pair<socket, event_type>, pair<thread, socket timeout>>
            std::map<
                std::pair<SocketType*, aio::EventType>,
                std::pair<AIOThreadType*, std::chrono::milliseconds> > sockets;
        };

        SocketAIOContext<Pollable> m_systemSocketAIO;
        SocketAIOContext<UdtSocket> m_udtSocketAIO;
        mutable QnMutex m_mutex;

        template<class SocketType> SocketAIOContext<SocketType>& getAIOHandlingContext();
        template<class SocketType> const SocketAIOContext<SocketType>& getAIOHandlingContext() const;// { static_assert( false, "Bad socket type" ); }

        template<class SocketType> void initializeAioThreadPool(SocketAIOContext<SocketType>* aioCtx, unsigned int threadCount)
        {
            typedef typename SocketAIOContext<SocketType>::AIOThreadType AIOThreadType;

            for( unsigned int i = 0; i < threadCount; ++i )
            {
                std::unique_ptr<AIOThreadType> thread(new AIOThreadType());
                thread->start();
                if( !thread->isRunning() )
                    continue;
                aioCtx->aioThreadPool.push_back(std::move(thread));
            }
        }

        template<class SocketType>
        aio::AIOThread<SocketType>* getSocketAioThread(
            QnMutexLockerBase* const lock,
            SocketType* sock)
        {
            auto thread = sock->impl()->aioThread.load(std::memory_order_relaxed);

            if (!thread) // socket has not been bound to aio thread yet
                thread = bindSocketToAioThread(lock, sock);

            Q_ASSERT(thread);
            return thread;
        }

        template<class SocketType>
        void cancelPostedCallsNonSafe(
            QnMutexLockerBase* const lock,
            SocketType* const sock,
            bool waitForRunningHandlerCompletion = true )
        {
            typename SocketAIOContext<SocketType>::AIOThreadType* aioThread = sock->impl()->aioThread.load( std::memory_order_relaxed );
            if( !aioThread )
                return;
            lock->unlock();
            aioThread->cancelPostedCalls( sock, waitForRunningHandlerCompletion );
            lock->relock();
        }

        template<class SocketType>
        aio::AIOThread<SocketType>* bindSocketToAioThread(
            QnMutexLockerBase* const /*lock*/,
            SocketType* const sock )
        {
            aio::AIOThread<SocketType>* threadToUse = nullptr;
            SocketAIOContext<SocketType>& aioHandlingContext = getAIOHandlingContext<SocketType>();

            //searching for a least-used thread, which is ready to accept
            for( auto
                threadIter = aioHandlingContext.aioThreadPool.cbegin();
                threadIter != aioHandlingContext.aioThreadPool.cend();
                ++threadIter )
            {
                if( threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled() )
                    continue;
                threadToUse = threadIter->get();
            }

            //assigning thread to socket once and for all
            aio::AIOThread<SocketType>* expectedSocketThread = nullptr;
            if( !sock->impl()->aioThread.compare_exchange_strong( expectedSocketThread, threadToUse ) )
            {
                //if created new thread than just leaving it for future use
                threadToUse = expectedSocketThread;     //socket is already bound to some thread
            }

            return threadToUse;
        }

        template<class SocketType, class Handler>
        void postNonSafe(QnMutexLockerBase* const lock, SocketType* sock, Handler handler)
        {
            auto* aioThread = getSocketAioThread(lock, sock);
            lock->unlock();
            aioThread->post(sock, std::move(handler));
            lock->relock();
        }
    };

}   //aio
}   //network
}   //nx

#endif  //AIOSERVICE_H
