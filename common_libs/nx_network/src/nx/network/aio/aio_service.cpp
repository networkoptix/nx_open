#include "aio_service.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <QtCore/QThread>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/mutex.h>

#include "pollable.h"

namespace nx {
namespace network {
namespace aio {

AIOService::AIOService(unsigned int threadCount)
{
    if (!threadCount)
        threadCount = QThread::idealThreadCount();

    initializeAioThreadPool(&m_systemSocketAIO, threadCount);
}

AIOService::~AIOService()
{
    pleaseStopSync();
}

void AIOService::pleaseStopSync()
{
    m_systemSocketAIO.sockets.clear();
    m_systemSocketAIO.aioThreadPool.clear();
}

bool AIOService::isInitialized() const
{
    return !m_systemSocketAIO.aioThreadPool.empty();
}

void AIOService::startMonitoring(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    boost::optional<std::chrono::milliseconds> timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    QnMutexLocker lk(&m_mutex);
    startMonitoringNonSafe(
        &lk,
        sock,
        eventToWatch,
        eventHandler,
        timeoutMillis,
        std::move(socketAddedToPollHandler));
}

void AIOService::stopMonitoring(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    QnMutexLocker lock(&m_mutex);

    const std::pair<Pollable*, aio::EventType>& sockCtx = std::make_pair(sock, eventType);
    auto it = m_systemSocketAIO.sockets.find(sockCtx);
    if (it == m_systemSocketAIO.sockets.end())
        return;

    auto aioThread = it->second.first;
    m_systemSocketAIO.sockets.erase(it);
    lock.unlock();
    aioThread->stopMonitoring(
        sock,
        eventType,
        waitForRunningHandlerCompletion,
        std::move(pollingStoppedHandler));
    lock.relock();
}

void AIOService::registerTimer(
    Pollable* const sock,
    std::chrono::milliseconds timeoutMillis,
    AIOEventHandler* const eventHandler)
{
    QnMutexLocker lock(&m_mutex);
    startMonitoringNonSafe(
        &lock,
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
    auto& threadToUse = nx::utils::random::choice(m_systemSocketAIO.aioThreadPool);
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

aio::AIOThread* AIOService::getSocketAioThread(Pollable* sock)
{
    QnMutexLocker lk(&m_mutex);
    return getSocketAioThread(&lk, sock);
}

AbstractAioThread* AIOService::getRandomAioThread() const
{
    QnMutexLocker lk(&m_mutex);
    return nx::utils::random::choice(m_systemSocketAIO.aioThreadPool).get();
}

AbstractAioThread* AIOService::getCurrentAioThread() const
{
    QnMutexLocker lk(&m_mutex);

    const auto thisThread = QThread::currentThread();
    const auto it = std::find_if(
        m_systemSocketAIO.aioThreadPool.begin(),
        m_systemSocketAIO.aioThreadPool.end(),
        [thisThread](const std::unique_ptr<AIOThread>& aioThread)
        {
            return thisThread == aioThread.get();
        });

    return (it != m_systemSocketAIO.aioThreadPool.end()) ? it->get() : nullptr;
}

bool AIOService::isInAnyAioThread() const
{
    return getCurrentAioThread() != nullptr;
}

void AIOService::bindSocketToAioThread(Pollable* sock, AbstractAioThread* aioThread)
{
    const auto desiredThread = static_cast<aio::AIOThread*>(aioThread);
    if (sock->impl()->aioThread.load() == desiredThread)
        return;

    NX_ASSERT(!isSocketBeingWatched(sock));

    //socket can be bound to another aio thread, if it is not used at the moment
    sock->impl()->aioThread.exchange(static_cast<aio::AIOThread*>(desiredThread));
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

void AIOService::startMonitoringNonSafe(
    QnMutexLockerBase* const lock,
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    boost::optional<std::chrono::milliseconds> timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    if (!timeoutMillis)
    {
        timeoutMillis = std::chrono::milliseconds::zero();
        if (!getSocketTimeout(sock, eventToWatch, &(*timeoutMillis)))
        {
            postNonSafe(
                lock,
                sock,
                std::bind(&AIOEventHandler::eventTriggered, eventHandler, sock, aio::etError));
            return;
        }
    }

    boost::optional<MonitoringContext> monitoringContext =
        getSocketMonitoringContext(sock, eventToWatch);
    if (monitoringContext)
    {
        if (monitoringContext->second != timeoutMillis)
        {
            // Socket is already polled for this event but with another timeout. Just changing timeout.
            monitoringContext->first->changeSocketTimeout(
                sock, eventToWatch, eventHandler, timeoutMillis.get());
            monitoringContext->second = timeoutMillis.get();
        }
        return;
    }

    const auto threadToUse = getSocketAioThread(lock, sock);
    if (!m_systemSocketAIO.sockets.emplace(
        std::make_pair(sock, eventToWatch),
        std::make_pair(threadToUse, timeoutMillis.get())).second)
    {
        NX_ASSERT(false);
    }

    lock->unlock();
    threadToUse->startMonitoring(
        sock,
        eventToWatch,
        eventHandler,
        timeoutMillis.get(),
        std::move(socketAddedToPollHandler));
    lock->relock();
}

aio::AIOThread* AIOService::getSocketAioThread(
    QnMutexLockerBase* const lock,
    Pollable* sock)
{
    auto thread = sock->impl()->aioThread.load(std::memory_order_relaxed);

    if (!thread) // socket has not been bound to aio thread yet
        thread = bindSocketToAioThread(lock, sock);

    NX_ASSERT(thread);
    return thread;
}

bool AIOService::getSocketTimeout(
    Pollable* const sock,
    aio::EventType eventToWatch,
    std::chrono::milliseconds* timeout)
{
    unsigned int sockTimeoutMS = 0;
    if (eventToWatch == aio::etRead)
    {
        if (!sock->getRecvTimeout(&sockTimeoutMS))
            return false;
    }
    else if (eventToWatch == aio::etWrite)
    {
        if (!sock->getSendTimeout(&sockTimeoutMS))
            return false;
    }
    else
    {
        NX_ASSERT(false);
        return false;
    }
    *timeout = std::chrono::milliseconds(sockTimeoutMS);
    return true;
}

boost::optional<AIOService::MonitoringContext> AIOService::getSocketMonitoringContext(
    Pollable* const sock,
    aio::EventType eventToWatch)
{
    // Checking, if that socket is already polled
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

    if (sameSockAndEventIter != m_systemSocketAIO.sockets.end())
        return sameSockAndEventIter->second;

    return boost::none;
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

aio::AIOThread* AIOService::bindSocketToAioThread(
    QnMutexLockerBase* const /*lock*/,
    Pollable* const sock)
{
    aio::AIOThread* threadToUse = nullptr;

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
    aio::AIOThread* expectedSocketThread = nullptr;
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

} // namespace aio
} // namespace network
} // namespace nx
