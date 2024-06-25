// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "acceptor_stub.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace network {
namespace test {

std::atomic<int> AcceptorStub::instanceCount(0);

AcceptorStub::AcceptorStub(
    nx::utils::SyncQueue<std::unique_ptr<AbstractStreamSocket>>* readyConnections)
    :
    m_readyConnections(readyConnections)
{
    ++instanceCount;

    bindToAioThread(getAioThread());
}

AcceptorStub::~AcceptorStub()
{
    --instanceCount;
    if (m_removedAcceptorsQueue)
        m_removedAcceptorsQueue->push(this);

    pleaseStopSync();
}

void AcceptorStub::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_repetitiveTimer.bindToAioThread(aioThread);
}

void AcceptorStub::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_repetitiveTimer.pleaseStopSync();
}

void AcceptorStub::acceptAsync(AcceptCompletionHandler handler)
{
    m_acceptHandler = std::move(handler);

    m_repetitiveTimer.start(
        std::chrono::milliseconds(10),
        [this]() { deliverConnectionIfAvailable(); });
}

void AcceptorStub::cancelIOSync()
{
    m_acceptHandler = nullptr;
}

std::unique_ptr<AbstractStreamSocket> AcceptorStub::getNextSocketIfAny()
{
    return nullptr;
}

void AcceptorStub::setRemovedAcceptorsQueue(
    utils::SyncQueue<AcceptorStub*>* removedAcceptorsQueue)
{
    m_removedAcceptorsQueue = removedAcceptorsQueue;
}

bool AcceptorStub::isAsyncAcceptInProgress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_acceptHandler != nullptr;
}

bool AcceptorStub::reportConnectionEstablishedIfNeeded()
{
    if (!m_connectionEstablishedHandler)
        return false;

    nx::utils::Url nextRemoteAddress;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_connectionsToRemoteAdressesToReport.empty())
            return false;

        nextRemoteAddress = std::move(m_connectionsToRemoteAdressesToReport.front());
        m_connectionsToRemoteAdressesToReport.pop_front();
    }

    m_connectionEstablishedHandler(nextRemoteAddress);

    if (m_connectionEstablishedReportedEvent)
        m_connectionEstablishedReportedEvent->push(std::move(nextRemoteAddress));

    return true;
}

bool AcceptorStub::reportErrorIfNeeded()
{
    if (!m_acceptErrorHandler)
        return false;

    cloud::AcceptorError nextError;
    std::size_t remainingErrors = 0;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_acceptorErrorsToReport.empty())
            return false;

        nextError = std::move(m_acceptorErrorsToReport.front());
        m_acceptorErrorsToReport.pop_front();

        remainingErrors = m_acceptorErrorsToReport.size();
    }

    NX_VERBOSE(this, "Reporting acceptor error: %1, remaining: %2", nextError, remainingErrors);

    m_acceptErrorHandler(nextError);

    if (m_acceptorErrorReportedEvent)
        m_acceptorErrorReportedEvent->push(std::move(nextError));

    return true;
}

void AcceptorStub::deliverConnectionIfAvailable()
{
    if (reportErrorIfNeeded())
        return;

    if (reportConnectionEstablishedIfNeeded())
        return;

    std::optional<std::unique_ptr<AbstractStreamSocket>> connection =
        m_readyConnections->pop(std::chrono::milliseconds::zero());
    if (!connection)
        return;

    m_repetitiveTimer.cancelSync();
    nx::utils::swapAndCall(m_acceptHandler, SystemError::noError, std::move(*connection));
}

void AcceptorStub::setConnectionEstablishedHandler(ConnectionEstablishedHandler handler)
{
    m_connectionEstablishedHandler = std::move(handler);
}

void AcceptorStub::setAcceptErrorHandler(ErrorHandler handler)
{
    m_acceptErrorHandler = std::move(handler);
}

//-------------------------------------------------------------------------------------------------

AcceptorDelegate::AcceptorDelegate(std::shared_ptr<cloud::AbstractConnectionAcceptor> target):
    m_target(target)
{
    bindToAioThread(m_target->getAioThread());
}

void AcceptorDelegate::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_target->bindToAioThread(aioThread);
}

void AcceptorDelegate::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_target->pleaseStopSync();
}

void AcceptorDelegate::acceptAsync(AcceptCompletionHandler handler)
{
    m_target->acceptAsync(std::move(handler));
}

void AcceptorDelegate::cancelIOSync()
{
    m_target->cancelIOSync();
}

void AcceptorStub::setEstablishedConnectionsToReport(
        std::list<nx::utils::Url> remoteAddresses,
        nx::utils::SyncQueue<nx::utils::Url>* connectionEstablishedReported)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_connectionsToRemoteAdressesToReport = std::move(remoteAddresses);
    m_connectionEstablishedReportedEvent = connectionEstablishedReported;
}

void AcceptorStub::setAcceptorErrorsToReport(
        std::list<cloud::AcceptorError> acceptorErrors,
        nx::utils::SyncQueue<cloud::AcceptorError>* acceptorErrorReported)
{
    NX_VERBOSE(this, "Got acceptor errors to report: %1", acceptorErrors.size());
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_acceptorErrorsToReport = std::move(acceptorErrors);
    m_acceptorErrorReportedEvent = acceptorErrorReported;
}

void AcceptorDelegate::setConnectionEstablishedHandler(ConnectionEstablishedHandler handler)
{
    m_target->setConnectionEstablishedHandler(std::move(handler));
}

std::unique_ptr<AbstractStreamSocket> AcceptorDelegate::getNextSocketIfAny()
{
    return m_target->getNextSocketIfAny();
}

void AcceptorDelegate::setAcceptErrorHandler(ErrorHandler handler)
{
    m_target->setAcceptErrorHandler(std::move(handler));
}

} // namespace test
} // namespace network
} // namespace nx
