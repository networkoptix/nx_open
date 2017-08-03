#include "aio_service.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <QtCore/QThread>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include "pollable.h"

//#define USE_SOCKET_EVENT_DICTIONARY

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
#ifdef USE_SOCKET_EVENT_DICTIONARY
    m_systemSocketAIO.sockets.clear();
#endif
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
    if (!timeoutMillis)
    {
        timeoutMillis = std::chrono::milliseconds::zero();
        if (!getSocketTimeout(sock, eventToWatch, &(*timeoutMillis)))
        {
            post(
                sock,
                std::bind(&AIOEventHandler::eventTriggered, eventHandler, sock, aio::etError));
            return;
        }
    }

    auto aioThread = getSocketAioThread(sock);

#ifdef USE_SOCKET_EVENT_DICTIONARY
    {
        QnMutexLocker lock(&m_mutex);

        boost::optional<MonitoringContext> monitoringContext =
            getSocketMonitoringContext(lock, sock, eventToWatch);
        if (monitoringContext)
        {
            if (monitoringContext->timeout != timeoutMillis)
            {
                // Socket is already polled for this event but with another timeout. Just changing timeout.
                aioThread->changeSocketTimeout(
                    sock, eventToWatch, eventHandler, timeoutMillis.get());
                monitoringContext->timeout = timeoutMillis.get();
            }
            return;
        }

        if (!m_systemSocketAIO.sockets.emplace(
                std::make_pair(sock, eventToWatch),
                MonitoringContext(timeoutMillis.get())).second)
        {
            NX_ASSERT(false);
        }
    }

    aioThread->startMonitoring(
        sock,
        eventToWatch,
        eventHandler,
        timeoutMillis.get(),
        std::move(socketAddedToPollHandler));
#else
    if (sock->impl()->monitoredEvents[eventToWatch].isUsed)
    {
        if (sock->impl()->monitoredEvents[eventToWatch].timeout == timeoutMillis)
            return;
        sock->impl()->monitoredEvents[eventToWatch].timeout = timeoutMillis;
        aioThread->changeSocketTimeout(
            sock,
            eventToWatch,
            eventHandler,
            timeoutMillis.get());
    }
    else
    {
        sock->impl()->monitoredEvents[eventToWatch].isUsed = true;
        aioThread->startMonitoring(
            sock,
            eventToWatch,
            eventHandler,
            timeoutMillis.get(),
            std::move(socketAddedToPollHandler));
    }
#endif
}

void AIOService::stopMonitoring(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
#ifdef USE_SOCKET_EVENT_DICTIONARY
    {
        QnMutexLocker lock(&m_mutex);

        const std::pair<Pollable*, aio::EventType>& sockCtx = std::make_pair(sock, eventType);
        auto it = m_systemSocketAIO.sockets.find(sockCtx);
        if (it == m_systemSocketAIO.sockets.end())
            return;
        m_systemSocketAIO.sockets.erase(it);
    }
#else
    if (sock->impl()->monitoredEvents[eventType].isUsed)
        sock->impl()->monitoredEvents[eventType].isUsed = false;
    else
        return;
#endif

    auto aioThread = getSocketAioThread(sock);
    aioThread->stopMonitoring(
        sock,
        eventType,
        waitForRunningHandlerCompletion,
        std::move(pollingStoppedHandler));
}

void AIOService::registerTimer(
    Pollable* const sock,
    std::chrono::milliseconds timeoutMillis,
    AIOEventHandler* const eventHandler)
{
    startMonitoring(
        sock,
        aio::etTimedOut,
        eventHandler,
        timeoutMillis);
}

bool AIOService::isSocketBeingWatched(Pollable* sock) const
{
#ifdef USE_SOCKET_EVENT_DICTIONARY
    QnMutexLocker lock(&m_mutex);
    const auto& it = m_systemSocketAIO.sockets.lower_bound(std::make_pair(sock, aio::etNone));
    return it != m_systemSocketAIO.sockets.end() && it->first.first == sock;
#else
    for (const auto& monitoringContext: sock->impl()->monitoredEvents)
    {
        if (monitoringContext.isUsed)
            return true;
    }

    return false;
#endif
}

void AIOService::post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler)
{
    auto* aioThread = getSocketAioThread(sock);
    aioThread->post(sock, std::move(handler));
}

void AIOService::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    auto& threadToUse = nx::utils::random::choice(m_systemSocketAIO.aioThreadPool);
    NX_ASSERT(threadToUse);
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
    auto thread = sock->impl()->aioThread.load(std::memory_order_relaxed);

    if (!thread) //< Socket has not been bound to aio thread yet.
        thread = bindSocketToAioThread(sock);

    NX_ASSERT(thread);
    return thread;
}

AbstractAioThread* AIOService::getRandomAioThread() const
{
    return nx::utils::random::choice(m_systemSocketAIO.aioThreadPool).get();
}

AbstractAioThread* AIOService::getCurrentAioThread() const
{
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

    // Socket can be bound to another aio thread, if it is not used at the moment.
    sock->impl()->aioThread.exchange(static_cast<aio::AIOThread*>(desiredThread));
}

aio::AIOThread* AIOService::bindSocketToAioThread(Pollable* const sock)
{
    aio::AIOThread* threadToUse = findLeastUsedAioThread();

    // Assigning thread to socket once and for all.
    aio::AIOThread* expectedSocketThread = nullptr;
    if (!sock->impl()->aioThread.compare_exchange_strong(expectedSocketThread, threadToUse))
        threadToUse = expectedSocketThread;     //< Socket is already bound to some thread.

    return threadToUse;
}

void AIOService::cancelPostedCalls(
    Pollable* const sock,
    bool waitForRunningHandlerCompletion)
{
    AIOThread* aioThread = sock->impl()->aioThread.load(std::memory_order_relaxed);
    if (!aioThread)
        return;
    aioThread->cancelPostedCalls(sock, waitForRunningHandlerCompletion);
}

void AIOService::initializeAioThreadPool(
    SocketAIOContext* aioCtx,
    unsigned int threadCount)
{
    for (unsigned int i = 0; i < threadCount; ++i)
    {
        auto thread = std::make_unique<AIOThread>();
        thread->start();
        if (!thread->isRunning())
            continue;
        aioCtx->aioThreadPool.push_back(std::move(thread));
    }
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

#ifdef USE_SOCKET_EVENT_DICTIONARY
boost::optional<AIOService::MonitoringContext> AIOService::getSocketMonitoringContext(
    const QnMutexLockerBase& /*lock*/,
    Pollable* const sock,
    aio::EventType eventToWatch)
{
    // Checking, if that socket is already monitored.
    auto sameSockAndEventIter = 
        m_systemSocketAIO.sockets.lower_bound(std::make_pair(sock, (aio::EventType)0));
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
#endif

AIOThread* AIOService::findLeastUsedAioThread() const
{
    aio::AIOThread* threadToUse = nullptr;

    // Searching for a least-used thread, which is ready to accept.
    for (auto
        threadIter = m_systemSocketAIO.aioThreadPool.cbegin();
        threadIter != m_systemSocketAIO.aioThreadPool.cend();
        ++threadIter)
    {
        if (threadToUse && threadToUse->socketsHandled() < (*threadIter)->socketsHandled())
            continue;
        threadToUse = threadIter->get();
    }

    return threadToUse;
}

} // namespace aio
} // namespace network
} // namespace nx
