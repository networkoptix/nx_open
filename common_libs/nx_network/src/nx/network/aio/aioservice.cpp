/**********************************************************
* 21 nov 2012
* a.kolesnikov
***********************************************************/

#include "aioservice.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <QtCore/QThread>
#include <qglobal.h>

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>


using namespace std;

namespace nx {
namespace network {
namespace aio {

typedef AIOThread<Pollable> SystemAIOThread;

static std::atomic<AIOService*> AIOService_instance( nullptr );

AIOService::AIOService( unsigned int threadCount )
{
    if( !threadCount )
        threadCount = QThread::idealThreadCount();
    //threadCount = 1;

    initializeAioThreadPool(&m_systemSocketAIO, threadCount);
}

AIOService::~AIOService()
{
    m_systemSocketAIO.sockets.clear();
    m_systemSocketAIO.aioThreadPool.clear();
}

//!Returns true, if object has been successfully initialized
bool AIOService::isInitialized() const
{
    return !m_systemSocketAIO.aioThreadPool.empty();
}


void AIOService::watchSocket(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler<Pollable>* const eventHandler,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_mutex);
    watchSocketNonSafe(
        &lk,
        sock,
        eventToWatch,
        eventHandler,
        boost::none,
        std::move(socketAddedToPollHandler));
}

void AIOService::removeFromWatch(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    QnMutexLocker lk(&m_mutex);
    removeFromWatchNonSafe(
        &lk,
        sock,
        eventType,
        waitForRunningHandlerCompletion,
        std::move(pollingStoppedHandler));
}

void AIOService::registerTimer(
    Pollable* const sock,
    std::chrono::milliseconds timeoutMillis,
    AIOEventHandler<Pollable>* const eventHandler)
{
    QnMutexLocker lk(&m_mutex);
    watchSocketNonSafe(
        &lk,
        sock,
        aio::etTimedOut,
        eventHandler,
        timeoutMillis);
}

bool AIOService::isSocketBeingWatched(Pollable* sock) const
{
    QnMutexLocker lk(&m_mutex);
    const auto& it = m_systemSocketAIO.sockets.lower_bound(std::make_pair(sock, aio::etNone));
    return it != m_systemSocketAIO.sockets.end() && it->first.first == sock;
}

void AIOService::post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lk(&m_mutex);
    postNonSafe(&lk, sock, std::move(handler));
}

void AIOService::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lk(&m_mutex);
    //if sock is not still bound to aio thread, binding it
    auto& threadToUse = m_systemSocketAIO.aioThreadPool[rand() % m_systemSocketAIO.aioThreadPool.size()];
    NX_ASSERT(threadToUse);
    lk.unlock();
    threadToUse->post(nullptr, std::move(handler));
}

void AIOService::dispatch(
    Pollable* sock,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    auto* aioThread = getSocketAioThread(sock);
    aioThread->dispatch(sock, std::move(handler));
}

QnMutex* AIOService::mutex() const
{
    return &m_mutex;
}

aio::AIOThread<Pollable>* AIOService::getSocketAioThread(Pollable* sock)
{
    QnMutexLocker lk(&m_mutex);
    return getSocketAioThread(&lk, sock);
}

void AIOService::bindSocketToAioThread(Pollable* sock, AbstractAioThread* aioThread)
{
    const auto desired = dynamic_cast<aio::AIOThread<Pollable>*>(aioThread);
    NX_ASSERT(desired, Q_FUNC_INFO, "Inappropriate AIO thread type");

    aio::AIOThread<Pollable>* expected = nullptr;
    if (!sock->impl()->aioThread.compare_exchange_strong(expected, desired))
    {
        NX_ASSERT(false, Q_FUNC_INFO,
            "Socket is already bound to some AIO thread");
    }
}

void AIOService::watchSocketNonSafe(
    QnMutexLockerBase* const lock,
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler<Pollable>* const eventHandler,
    boost::optional<std::chrono::milliseconds> timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    if (!timeoutMillis)
    {
        unsigned int sockTimeoutMS = 0;
        if (eventToWatch == aio::etRead)
        {
            if (!sock->getRecvTimeout(&sockTimeoutMS))
            {
                //reporting error via event thread
                postNonSafe(
                    lock,
                    sock,
                    std::bind(
                        &AIOEventHandler<Pollable>::eventTriggered,
                        eventHandler,
                        sock,
                        aio::etError));
                return;
            }
        }
        else if (eventToWatch == aio::etWrite)
        {
            if (!sock->getSendTimeout(&sockTimeoutMS))
            {
                //reporting error via event thread
                postNonSafe(
                    lock,
                    sock,
                    std::bind(
                        &AIOEventHandler<Pollable>::eventTriggered,
                        eventHandler,
                        sock,
                        aio::etError));
                return;
            }
        }
        else
        {
            NX_ASSERT(false);
        }
        timeoutMillis = std::chrono::milliseconds(sockTimeoutMS);
    }

    //checking, if that socket is already polled
    const std::pair<Pollable*, aio::EventType>& sockCtx = std::make_pair(sock, eventToWatch);
    //checking if sock is already polled (even for another event)
    auto closestSockIter = m_systemSocketAIO.sockets.lower_bound(std::make_pair(sock, (aio::EventType)0));
    auto sameSockAndEventIter = closestSockIter;
    for (; sameSockAndEventIter != m_systemSocketAIO.sockets.end(); ++sameSockAndEventIter)
    {
        if (sameSockAndEventIter->first.first != sock)
        {
            sameSockAndEventIter = m_systemSocketAIO.sockets.end();
            break;
        }
        else if (sameSockAndEventIter->first.second == eventToWatch)
        {
            break;
        }
    }
    if (sameSockAndEventIter != m_systemSocketAIO.sockets.end() &&
        sameSockAndEventIter->second.second == timeoutMillis.get())
    {
        return;    //socket already monitored for eventToWatch
    }

    if ((closestSockIter != m_systemSocketAIO.sockets.end()) && (closestSockIter->first.first == sockCtx.first))  //same socket is already polled
    {
        if (sameSockAndEventIter != m_systemSocketAIO.sockets.end())
        {
            //socket is already polled for this event but with another timeout. Just changing timeout
            sameSockAndEventIter->second.first->changeSocketTimeout(
                sock, eventToWatch, eventHandler, timeoutMillis.get());
            sameSockAndEventIter->second.second = timeoutMillis.get();
            return;
        }
    }

    const auto threadToUse = getSocketAioThread(lock, sock);
    if (!m_systemSocketAIO.sockets.emplace(
        sockCtx,
        std::make_pair(threadToUse, timeoutMillis.get())).second)
    {
        NX_ASSERT(false);
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
void AIOService::removeFromWatchNonSafe(
    QnMutexLockerBase* const lock,
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    const std::pair<Pollable*, aio::EventType>& sockCtx = std::make_pair(sock, eventType);
    auto it = m_systemSocketAIO.sockets.find(sockCtx);
    if (it == m_systemSocketAIO.sockets.end())
        return;

    auto aioThread = it->second.first;
    m_systemSocketAIO.sockets.erase(it);
    lock->unlock();
    aioThread->removeFromWatch(
        sock,
        eventType,
        waitForRunningHandlerCompletion,
        std::move(pollingStoppedHandler));
    lock->relock();
}

void AIOService::cancelPostedCalls(
    Pollable* const sock,
    bool waitForRunningHandlerCompletion)
{
    QnMutexLocker lk(&m_mutex);
    cancelPostedCallsNonSafe(&lk, sock, waitForRunningHandlerCompletion);
}

void AIOService::initializeAioThreadPool(
    SocketAIOContext* aioCtx,
    unsigned int threadCount)
{
    typedef typename SocketAIOContext::AIOThreadType AIOThreadType;

    for (unsigned int i = 0; i < threadCount; ++i)
    {
        std::unique_ptr<AIOThreadType> thread(new AIOThreadType());
        thread->start();
        if (!thread->isRunning())
            continue;
        aioCtx->aioThreadPool.push_back(std::move(thread));
    }
}

aio::AIOThread<Pollable>* AIOService::getSocketAioThread(
    QnMutexLockerBase* const lock,
    Pollable* sock)
{
    auto thread = sock->impl()->aioThread.load(std::memory_order_relaxed);

    if (!thread) // socket has not been bound to aio thread yet
        thread = bindSocketToAioThread(lock, sock);

    NX_ASSERT(thread);
    return thread;
}

void AIOService::cancelPostedCallsNonSafe(
    QnMutexLockerBase* const lock,
    Pollable* const sock,
    bool waitForRunningHandlerCompletion)
{
    typename SocketAIOContext::AIOThreadType* aioThread = sock->impl()->aioThread.load(std::memory_order_relaxed);
    if (!aioThread)
        return;
    lock->unlock();
    aioThread->cancelPostedCalls(sock, waitForRunningHandlerCompletion);
    lock->relock();
}

aio::AIOThread<Pollable>* AIOService::bindSocketToAioThread(
    QnMutexLockerBase* const /*lock*/,
    Pollable* const sock)
{
    aio::AIOThread<Pollable>* threadToUse = nullptr;

    //searching for a least-used thread, which is ready to accept
    for (auto
        threadIter = m_systemSocketAIO.aioThreadPool.cbegin();
        threadIter != m_systemSocketAIO.aioThreadPool.cend();
        ++threadIter)
    {
        if (threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled())
            continue;
        threadToUse = threadIter->get();
    }

    //assigning thread to socket once and for all
    aio::AIOThread<Pollable>* expectedSocketThread = nullptr;
    if (!sock->impl()->aioThread.compare_exchange_strong(expectedSocketThread, threadToUse))
    {
        //if created new thread than just leaving it for future use
        threadToUse = expectedSocketThread;     //socket is already bound to some thread
    }

    return threadToUse;
}

void AIOService::postNonSafe(
    QnMutexLockerBase* const lock,
    Pollable* sock,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    auto* aioThread = getSocketAioThread(lock, sock);
    lock->unlock();
    aioThread->post(sock, std::move(handler));
    lock->relock();
}

}   //aio
}   //network
}   //nx
