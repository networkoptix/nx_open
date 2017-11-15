#include "aggregate_acceptor.h"

#include <algorithm>

#include <nx/utils/log/log.h>

namespace nx {
namespace network {

AggregateAcceptor::AcceptorContext::AcceptorContext(
    std::unique_ptr<AbstractStreamSocketAcceptor> acceptor)
    :
    acceptor(std::move(acceptor)),
    isAccepting(false)
{
}

void AggregateAcceptor::AcceptorContext::stopAccepting()
{
    if (!isAccepting)
        return;

    isAccepting = false;
    acceptor->cancelIOSync();
}

//-------------------------------------------------------------------------------------------------

AggregateAcceptor::AggregateAcceptor()
{
    bindToAioThread(getAioThread());
}

void AggregateAcceptor::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_timer.bindToAioThread(aioThread);
    for (auto& acceptor: m_acceptors)
        acceptor.acceptor->bindToAioThread(aioThread);
}

void AggregateAcceptor::acceptAsync(AcceptCompletionHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            NX_ASSERT(!m_acceptHandler, "Concurrent accept call");
            m_acceptHandler = std::move(handler);

            if (m_acceptTimeout && (*m_acceptTimeout > std::chrono::milliseconds::zero()))
            {
                m_timer.start(
                    *m_acceptTimeout,
                    std::bind(
                        &AggregateAcceptor::accepted, this,
                        nullptr, SystemError::timedOut, nullptr));
            }

            m_acceptAsyncIsBeingInvoked = true;
            for (auto& source: m_acceptors)
            {
                if (source.isAccepting)
                    continue;

                source.isAccepting = true;
                NX_LOGX(lm("Accept on source(%1)").arg(&source), cl_logDEBUG2);

                using namespace std::placeholders;
                source.acceptor->acceptAsync(std::bind(
                    &AggregateAcceptor::accepted, this, &source, _1, _2));

                if (!source.isAccepting)
                    break; //< Accept handler has been invoked within acceptAsync call.
            }
            m_acceptAsyncIsBeingInvoked = false;
        });
}

void AggregateAcceptor::cancelIOSync()
{
    NX_LOGX(lm("Cancelling async IO synchronously..."), cl_logDEBUG2);
    nx::utils::promise<void> ioCancelledPromise;
    dispatch(
        [this, &ioCancelledPromise]() mutable
        {
            cancelIoFromAioThread();
            ioCancelledPromise.set_value();
        });

    ioCancelledPromise.get_future().wait();
}

bool AggregateAcceptor::add(std::unique_ptr<AbstractStreamSocketAcceptor> socket)
{
    NX_LOGX(lm("Add socket(%1)").arg(socket), cl_logDEBUG2);

    nx::utils::promise<void> socketAddedPromise;
    dispatch(
        [this, &socketAddedPromise, socket = std::move(socket)]() mutable
        {
            socket->bindToAioThread(getAioThread());
            m_acceptors.push_back(AcceptorContext(std::move(socket)));
            if (m_acceptHandler)
            {
                AcceptorContext& source = m_acceptors.back();
                source.isAccepting = true;
                NX_LOGX(lm("Accept on source(%1) when added").arg(&source), cl_logDEBUG1);

                using namespace std::placeholders;
                source.acceptor->acceptAsync(std::bind(
                    &AggregateAcceptor::accepted, this, &source, _1, _2));
            }

            socketAddedPromise.set_value();
        });

    socketAddedPromise.get_future().wait();
    return true;
}

void AggregateAcceptor::removeAt(size_t pos)
{
    NX_LOGX(lm("Remove socket(%1)").arg(pos), cl_logDEBUG2);

    removeByIterator(
        [this, pos]()
        {
            if (pos >= m_acceptors.size())
            {
                NX_ASSERT(false, lm("pos = %1, m_acceptors.size() = %2")
                    .arg(pos).arg(m_acceptors.size()));
                return m_acceptors.end();
            }
            return std::next(m_acceptors.begin(), pos);
        });
}

void AggregateAcceptor::remove(AbstractStreamSocketAcceptor* acceptor)
{
    removeByIterator(
        [this, acceptor]()
        {
            return std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [acceptor](const AcceptorContext& element)
                {
                    return element.acceptor.get() == acceptor;
                });
        });
}

size_t AggregateAcceptor::count() const
{
    return m_acceptors.size();
}

void AggregateAcceptor::setAcceptTimeout(
    boost::optional<std::chrono::milliseconds> timeout)
{
    m_acceptTimeout = timeout;
}

void AggregateAcceptor::stopWhileInAioThread()
{
    m_acceptors.clear();
    m_timer.cancelSync();
}

void AggregateAcceptor::accepted(
    AcceptorContext* source,
    SystemError::ErrorCode code,
    std::unique_ptr<AbstractStreamSocket> socket)
{
    NX_LOGX(lm("Accepted socket(%1) (%2) from source(%3)")
        .arg(socket.get()).arg(SystemError::toString(code)).arg(source), cl_logDEBUG2);

    if (source)
    {
        NX_CRITICAL(
            std::find_if(
                m_acceptors.begin(), m_acceptors.end(),
                [source](AcceptorContext& item) { return source == &item; }) !=
            m_acceptors.end());

        source->isAccepting = false;
        m_timer.cancelSync(); //< Will not block, we are in an aio thread.
    }

    decltype(m_acceptHandler) handler;
    handler.swap(m_acceptHandler);
    NX_ASSERT(handler, "acceptAsync was not canceled in time");

    // TODO: #ak Better not cancel accepts on every socket accepted.

    NX_LOGX(lm("Cancel all other accepts"), cl_logDEBUG2);
    for (auto& socketContext: m_acceptors)
        socketContext.stopAccepting();

    if (m_acceptAsyncIsBeingInvoked)
    {
        post(
            [handler = std::move(handler), code, socket = std::move(socket)]() mutable
            {
                handler(code, std::move(socket));
            });
    }
    else
    {
        handler(code, std::move(socket));
    }
}

void AggregateAcceptor::cancelIoFromAioThread()
{
    m_acceptHandler = nullptr;
    m_timer.cancelSync();
    for (auto& socketContext: m_acceptors)
        socketContext.stopAccepting();
}

template<typename FindIteratorFunc>
void AggregateAcceptor::removeByIterator(FindIteratorFunc findIterator)
{
    nx::utils::promise<void> socketRemovedPromise;
    dispatch(
        [this, &socketRemovedPromise, &findIterator]()
        {
            auto itemToRemoveIter = findIterator();
            if (itemToRemoveIter == m_acceptors.end())
                return;

            auto serverSocketContext = std::move(*itemToRemoveIter);
            m_acceptors.erase(itemToRemoveIter);
            serverSocketContext.acceptor->pleaseStopSync();

            NX_LOGX(lm("Acceptor(%1) is removed")
                .arg(serverSocketContext.acceptor), cl_logDEBUG1);
            socketRemovedPromise.set_value();
        });

    socketRemovedPromise.get_future().wait();
}

} // namespace network
} // namespace nx
