#include "basic_pollable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/future.h>

#include "../socket_global.h"

namespace nx {
namespace network {
namespace aio {

BasicPollable::BasicPollable(aio::AbstractAioThread* aioThread):
    BasicPollable(&SocketGlobals::aioService(), aioThread)
{
}

BasicPollable::BasicPollable(
    aio::AIOService* aioService,
    aio::AbstractAioThread* aioThread)
    :
    m_pollable(-1, std::make_unique<CommonSocketImpl>()),
    m_aioService(aioService)
{
    if (!aioThread)
        aioThread = m_aioService->getCurrentAioThread();
    if (!aioThread)
        aioThread = m_aioService->getRandomAioThread();

    m_aioService->bindSocketToAioThread(&m_pollable, aioThread);
}

BasicPollable::~BasicPollable()
{
    if (isInSelfAioThread())
    {
        m_aioService->cancelPostedCalls(&m_pollable);
    }
    else
    {
        NX_CRITICAL(!m_aioService->isSocketBeingMonitored(&m_pollable),
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
        m_aioService->cancelPostedCalls(&m_pollable);

        nx::utils::InterruptionFlag::Watcher watcher(&m_interruptionFlag);
        stopWhileInAioThread();
        if (watcher.interrupted())
            return;

        m_aioService->cancelPostedCalls(&m_pollable);
    }
    else
    {
        NX_ASSERT(!m_aioService->isInAnyAioThread());
        QnStoppableAsync::pleaseStopSync(m_aioService);
    }
}

aio::AbstractAioThread* BasicPollable::getAioThread() const
{
    return m_aioService->getSocketAioThread(&m_pollable);
}

void BasicPollable::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    if (m_pollable.impl()->aioThread.load() != aioThread)
        m_interruptionFlag.interrupt();

    m_aioService->bindSocketToAioThread(&m_pollable, aioThread);
}

void BasicPollable::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioService->post(&m_pollable, std::move(func));
}

void BasicPollable::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_aioService->dispatch(&m_pollable, std::move(func));
}

bool BasicPollable::isInSelfAioThread() const
{
    getAioThread(); //< This call will assign AIO thread, if not assigned yet.
    return m_pollable.isInSelfAioThread();
}

void BasicPollable::cancelPostedCalls(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_aioService->cancelPostedCalls(&m_pollable);
            completionHandler();
        });
}

void BasicPollable::cancelPostedCallsSync()
{
    executeInAioThreadSync(
        [this]() { m_aioService->cancelPostedCalls(&m_pollable); });
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

} // namespace aio
} // namespace network
} // namespace nx
