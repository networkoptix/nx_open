// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/network/aio/repetitive_timer.h>
#include <nx/network/cloud/cloud_abstract_connection_acceptor.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace test {

class NX_NETWORK_API AcceptorStub:
    public cloud::AbstractConnectionAcceptor
{
    using base_type = cloud::AbstractConnectionAcceptor;

public:
    AcceptorStub(
        nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>>* readyConnections);

    ~AcceptorStub();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;
    virtual void setAcceptErrorHandler(ErrorHandler handler) override
    {
        m_acceptErrorHandler = std::move(handler);
    }

    void setAcceptorErrorsToReport(
        std::list<cloud::AcceptorError> acceptorErrors,
        nx::utils::SyncQueue<void>* allErrorsReportedEvent = nullptr)
    {
        m_acceptorErrorsToReport = std::move(acceptorErrors);
        m_allErrorsReportedEvent = allErrorsReportedEvent;
    }

    void setRemovedAcceptorsQueue(utils::SyncQueue<AcceptorStub*>* removedAcceptorsQueue);

    bool isAsyncAcceptInProgress() const;

    static std::atomic<int> instanceCount;

private:
    bool reportErrorIfNeeded();

private:
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>>* m_readyConnections = nullptr;
    utils::SyncQueue<AcceptorStub*>* m_removedAcceptorsQueue = nullptr;
    AcceptCompletionHandler m_acceptHandler;
    ErrorHandler m_acceptErrorHandler;
    mutable nx::Mutex m_mutex;
    nx::network::aio::RepetitiveTimer m_repetitiveTimer;
    std::list<cloud::AcceptorError> m_acceptorErrorsToReport;
    nx::utils::SyncQueue<void>* m_allErrorsReportedEvent = nullptr;

    void deliverConnectionIfAvailable();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API AcceptorDelegate:
    public cloud::AbstractConnectionAcceptor
{
    using base_type = cloud::AbstractConnectionAcceptor;

public:
    AcceptorDelegate(std::shared_ptr<cloud::AbstractConnectionAcceptor> target);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    virtual void acceptAsync(AcceptCompletionHandler handler) override;
    virtual void cancelIOSync() override;
    virtual std::unique_ptr<AbstractStreamSocket> getNextSocketIfAny() override;
    virtual void setAcceptErrorHandler(ErrorHandler) override;

private:
    std::shared_ptr<cloud::AbstractConnectionAcceptor> m_target;
};

} // namespace test
} // namespace network
} // namespace nx
