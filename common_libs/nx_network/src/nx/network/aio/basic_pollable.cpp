#include "basic_pollable.h"

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
        m_aioService->cancelPostedCalls(&m_pollable, true);
    }
    else
    {
        NX_CRITICAL(!m_aioService->isSocketBeingWatched(&m_pollable),
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

void BasicPollable::pleaseStopSync(bool checkForLocks)
{
    if (isInSelfAioThread())
    {
        m_aioService->cancelPostedCalls(&m_pollable, true);

        nx::utils::ObjectDestructionFlag::Watcher watcher(&m_destructionFlag);
        stopWhileInAioThread();
        if (!watcher.objectDestroyed())
            m_aioService->cancelPostedCalls(&m_pollable, true);
    }
    else
    {
        NX_ASSERT(!m_aioService->isInAnyAioThread());
        QnStoppableAsync::pleaseStopSync(checkForLocks);
    }
}

aio::AbstractAioThread* BasicPollable::getAioThread() const
{
    return m_aioService->getSocketAioThread(&m_pollable);
}

void BasicPollable::bindToAioThread(aio::AbstractAioThread* aioThread)
{
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
    return getAioThread() == m_aioService->getCurrentAioThread();
}

void BasicPollable::cancelPostedCalls(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_aioService->cancelPostedCalls(&m_pollable, true);
            completionHandler();
        });
}

void BasicPollable::cancelPostedCallsSync()
{
    executeInAioThreadSync(
        [this]() { m_aioService->cancelPostedCalls(&m_pollable, true); });
}

void BasicPollable::stopWhileInAioThread()
{
}

Pollable& BasicPollable::pollable()
{
    return m_pollable;
}

} // namespace aio
} // namespace network
} // namespace nx
