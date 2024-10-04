// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "basic_pollable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/future.h>

#include "../common_socket_impl.h"
#include "../socket_global.h"

namespace nx::network::aio {

BasicPollable::BasicPollable(aio::AbstractAioThread* aioThread):
    BasicPollable(&SocketGlobals::aioService(), aioThread)
{
}

BasicPollable::BasicPollable(
    aio::AIOService* aioService,
    aio::AbstractAioThread* aioThread)
    :
    m_pollable(
        [aioService, aioThread]() mutable
        {
            if (!aioThread)
                aioThread = aioService->getCurrentAioThread();
            if (!aioThread)
                aioThread = aioService->findLeastUsedAioThread();
            return aioThread;
        }(),
        AbstractSocket::kInvalidSocket),
    m_aioService(aioService)
{
}

BasicPollable::~BasicPollable()
{
    if (isInSelfAioThread())
    {
        m_pollable.getAioThread()->cancelPostedCalls(&m_pollable);
    }
    else
    {
        NX_CRITICAL(!m_pollable.getAioThread()->isSocketBeingMonitored(&m_pollable),
            "You MUST cancel running async operation before deleting pollable "
            "if you delete it from non-aio thread");
    }
}

void BasicPollable::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]
        {
            pleaseStopSync();
            completionHandler();
        });
}

void BasicPollable::pleaseStopSync()
{
    if (isInSelfAioThread())
    {
        m_pollable.getAioThread()->cancelPostedCalls(&m_pollable);

        nx::utils::InterruptionFlag::Watcher watcher(&m_interruptionFlag);
        stopWhileInAioThread();
        if (watcher.interrupted())
            return;

        m_pollable.getAioThread()->cancelPostedCalls(&m_pollable);
    }
    else
    {
        NX_ASSERT(!m_aioService->isInAnyAioThread());
        QnStoppableAsync::pleaseStopSync(m_aioService);
    }
}

aio::AbstractAioThread* BasicPollable::getAioThread() const
{
    return m_pollable.getAioThread();
}

void BasicPollable::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    if (m_pollable.impl()->aioThread->load() != aioThread)
        m_interruptionFlag.interrupt();

    m_pollable.bindToAioThread(aioThread);
}

void BasicPollable::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_pollable.getAioThread()->post(&m_pollable, std::move(func));
}

void BasicPollable::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_pollable.getAioThread()->dispatch(&m_pollable, std::move(func));
}

bool BasicPollable::isInSelfAioThread() const
{
    return m_pollable.isInSelfAioThread();
}

void BasicPollable::cancelPostedCalls(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_pollable.getAioThread()->cancelPostedCalls(&m_pollable);
            completionHandler();
        });
}

void BasicPollable::cancelPostedCallsSync()
{
    executeInAioThreadSync(
        [this]() { m_pollable.getAioThread()->cancelPostedCalls(&m_pollable); });
}

Pollable& BasicPollable::pollable()
{
    return m_pollable;
}

const Pollable& BasicPollable::pollable() const
{
    return m_pollable;
}

void BasicPollable::stopWhileInAioThread()
{
}

} // namespace nx::network::aio
