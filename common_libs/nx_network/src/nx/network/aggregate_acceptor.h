#pragma once

#include <chrono>
#include <list>

#include <boost/optional.hpp>

#include "abstract_acceptor.h"
#include "aio/timer.h"

namespace nx {
namespace network {

class NX_NETWORK_API AggregateAcceptor:
    public AbstractAcceptor
{
    using base_type = AbstractAcceptor;

public:
    AggregateAcceptor();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;

    /**
     * These methods can be called concurrently with AggregateAcceptor::accept.
     * NOTE: Blocks until completion.
     */
    bool addSocket(std::unique_ptr<AbstractAcceptor> acceptor);
    void removeSocket(size_t pos);
    void remove(AbstractAcceptor* acceptor);
    size_t count() const;

    void setAcceptTimeout(boost::optional<std::chrono::milliseconds> timeout);

private:
    struct AcceptorContext
    {
        std::unique_ptr<AbstractAcceptor> acceptor;
        bool isAccepting;

        AcceptorContext(std::unique_ptr<AbstractAcceptor> acceptor);
        AcceptorContext(AcceptorContext&&) = default;
        AcceptorContext& operator=(AcceptorContext&&) = default;

        void stopAccepting();
    };

    AcceptCompletionHandler m_acceptHandler;
    aio::Timer m_timer;
    std::list<AcceptorContext> m_acceptors;
    boost::optional<std::chrono::milliseconds> m_acceptTimeout;

    virtual void stopWhileInAioThread() override;

    void accepted(
        AcceptorContext* source,
        SystemError::ErrorCode code,
        std::unique_ptr<AbstractStreamSocket> socket);

    void cancelIoFromAioThread();

    template<typename FindIteratorFunc>
    void removeByIterator(FindIteratorFunc findIterator);
};

} // namespace network
} // namespace nx
