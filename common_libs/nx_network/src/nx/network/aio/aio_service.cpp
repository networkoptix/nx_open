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

namespace nx {
namespace network {
namespace aio {

AIOService::AIOService(unsigned int threadCount)
{
    if (!threadCount)
        threadCount = QThread::idealThreadCount();

    initializeAioThreadPool(threadCount);
}

AIOService::~AIOService()
{
    pleaseStopSync();
}

void AIOService::pleaseStopSync()
{
    m_aioThreadPool.clear();
}

bool AIOService::isInitialized() const
{
    return !m_aioThreadPool.empty();
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
}

void AIOService::stopMonitoring(
    Pollable* const sock,
    aio::EventType eventType,
    bool waitForRunningHandlerCompletion,
    nx::utils::MoveOnlyFunc<void()> pollingStoppedHandler)
{
    if (sock->impl()->monitoredEvents[eventType].isUsed)
        sock->impl()->monitoredEvents[eventType].isUsed = false;
    else
        return;

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
    for (const auto& monitoringContext: sock->impl()->monitoredEvents)
    {
        if (monitoringContext.isUsed)
            return true;
    }

    return false;
}

void AIOService::post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler)
{
    auto* aioThread = getSocketAioThread(sock);
    aioThread->post(sock, std::move(handler));
}

void AIOService::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    auto& threadToUse = nx::utils::random::choice(m_aioThreadPool);
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
    return nx::utils::random::choice(m_aioThreadPool).get();
}

AbstractAioThread* AIOService::getCurrentAioThread() const
{
    const auto thisThread = QThread::currentThread();
    const auto it = std::find_if(
        m_aioThreadPool.begin(),
        m_aioThreadPool.end(),
        [thisThread](const std::unique_ptr<AIOThread>& aioThread)
        {
            return thisThread == aioThread.get();
        });

    return (it != m_aioThreadPool.end()) ? it->get() : nullptr;
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

void AIOService::initializeAioThreadPool(unsigned int threadCount)
{
    for (unsigned int i = 0; i < threadCount; ++i)
    {
        auto thread = std::make_unique<AIOThread>();
        thread->start();
        if (!thread->isRunning())
            continue;
        m_aioThreadPool.push_back(std::move(thread));
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

AIOThread* AIOService::findLeastUsedAioThread() const
{
    aio::AIOThread* threadToUse = nullptr;

    for (auto
        threadIter = m_aioThreadPool.cbegin();
        threadIter != m_aioThreadPool.cend();
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
