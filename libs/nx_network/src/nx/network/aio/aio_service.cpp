// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "aio_service.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

#include <QtCore/QThread>

#include <nx/network/nx_network_ini.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>

#include "pollable.h"
#include "../common_socket_impl.h"

namespace nx::network::aio {

AIOService::~AIOService()
{
    pleaseStopSync();
}

void AIOService::pleaseStopSync()
{
    for (auto& thread: m_aioThreadPool)
        thread->pleaseStop();

    m_aioThreadPool.clear();
}

bool AIOService::initialize(unsigned int aioThreadPoolSize)
{
    if (!aioThreadPoolSize)
        aioThreadPoolSize = nx::network::ini().aioThreadCount;
    if (!aioThreadPoolSize)
        aioThreadPoolSize = QThread::idealThreadCount();
    initializeAioThreadPool(aioThreadPoolSize);
    return !m_aioThreadPool.empty();
}

void AIOService::startMonitoring(
    Pollable* const sock,
    aio::EventType eventToWatch,
    AIOEventHandler* const eventHandler,
    std::optional<std::chrono::milliseconds> timeoutMillis,
    nx::utils::MoveOnlyFunc<void()> socketAddedToPollHandler)
{
    sock->impl()->aioThread->load()->startMonitoring(
        sock,
        eventToWatch,
        eventHandler,
        timeoutMillis,
        std::move(socketAddedToPollHandler));
}

void AIOService::stopMonitoring(Pollable* const sock, aio::EventType eventType)
{
    auto aioThread = sock->impl()->aioThread->load(std::memory_order_relaxed);
    if (!aioThread)
        return; //< Not bound to a thread. Socket is definitely not monitored.

    aioThread->stopMonitoring(
        sock,
        eventType);
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

bool AIOService::isSocketBeingMonitored(Pollable* sock)
{
    auto aioThread = sock->impl()->aioThread->load(std::memory_order_relaxed);
    if (!aioThread)
        return false;

    return aioThread->isSocketBeingMonitored(sock);
}

void AIOService::post(Pollable* sock, nx::utils::MoveOnlyFunc<void()> handler)
{
    sock->impl()->aioThread->load()->post(sock, std::move(handler));
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
    sock->impl()->aioThread->load()->dispatch(sock, std::move(handler));
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
        [thisThread](const std::unique_ptr<AioThread>& aioThread)
        {
            return thisThread == aioThread.get();
        });

    return (it != m_aioThreadPool.end()) ? it->get() : nullptr;
}

std::vector<AbstractAioThread*> AIOService::getAllAioThreads() const
{
    std::vector<AbstractAioThread*> result;
    std::transform(
        m_aioThreadPool.begin(), m_aioThreadPool.end(),
        std::back_inserter(result),
        [](const auto& thread) { return thread.get(); });
    return result;
}

bool AIOService::isInAnyAioThread() const
{
    return getCurrentAioThread() != nullptr;
}

void AIOService::initializeAioThreadPool(unsigned int threadCount)
{
    for (unsigned int i = 0; i < threadCount; ++i)
    {
        auto thread = std::make_unique<AioThread>();
        thread->start();
        if (!thread->isRunning())
            continue;
        m_aioThreadPool.push_back(std::move(thread));
    }
}

AbstractAioThread* AIOService::findLeastUsedAioThread() const
{
    aio::AioThread* threadToUse = nullptr;

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

} // namespace nx::network::aio
