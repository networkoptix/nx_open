#include "hole_punching_processor.h"

#include <nx/network/stun/message_dispatcher.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/utils/time.h>
#include <nx/utils/thread/barrier_handler.h>

#include "listening_peer_pool.h"
#include "settings.h"
#include "statistics/collector.h"

namespace nx {
namespace hpm {

static QString logRequest(
    const ConnectionStrongRef& connection,
    const api::ConnectRequest& request)
{
    return lm("from %1(%2) to %3, session id %4")
        .args(request.originatingPeerId, connection->getSourceAddress(),
            request.destinationHostName, request.connectSessionId);
}

HolePunchingProcessor::HolePunchingProcessor(
    const conf::Settings& settings,
    AbstractCloudDataProvider* cloudData,
    ListeningPeerPool* listeningPeerPool,
    AbstractRelayClusterClient* relayClusterClient,
    stats::AbstractCollector* statisticsCollector)
:
    RequestProcessor(cloudData),
    m_settings(settings),
    m_listeningPeerPool(listeningPeerPool),
    m_relayClusterClient(relayClusterClient),
    m_statisticsCollector(statisticsCollector)
{
}

HolePunchingProcessor::~HolePunchingProcessor()
{
    stop();
}

void HolePunchingProcessor::stop()
{
    ConnectSessionsDictionary localSessions;
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(localSessions, m_activeConnectSessions);
    }

    if (localSessions.empty())
        return;

    nx::utils::promise<void> allSessionsStoppedPromise;
    {
        nx::utils::BarrierHandler barrier(
            [&allSessionsStoppedPromise]() { allSessionsStoppedPromise.set_value(); });
        for (const auto& connectSession : localSessions)
            connectSession.second->pleaseStop(barrier.fork());
    }

    // Waiting for all sessions to stop...
    allSessionsStoppedPromise.get_future().wait();
}

void HolePunchingProcessor::connect(
    const ConnectionStrongRef& connection,
    api::ConnectRequest request,
    network::stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    api::ResultCode validationResult = api::ResultCode::ok;
    boost::optional<ListeningPeerPool::ConstDataLocker> targetPeerDataLocker;
    std::tie(validationResult, targetPeerDataLocker) = validateConnectRequest(
        connection,
        request);
    if (validationResult != api::ResultCode::ok)
        return completionHandler(validationResult, api::ConnectResponse());
    NX_ASSERT(static_cast<bool>(targetPeerDataLocker));

    // Preparing and saving connection context.
    QnMutexLocker lk(&m_mutex);
    const auto connectionFsmIterAndFlag = m_activeConnectSessions.emplace(
        request.connectSessionId,
        nullptr);
    if (connectionFsmIterAndFlag.second)
    {
        NX_LOGX(lm("Connect request %1").arg(logRequest(connection, request)), cl_logDEBUG2);

        connectionFsmIterAndFlag.first->second =
            std::make_unique<UDPHolePunchingConnectionInitiationFsm>(
                request.connectSessionId,
                targetPeerDataLocker->value(),
                std::bind(
                    &HolePunchingProcessor::connectSessionFinished,
                    this,
                    std::move(connectionFsmIterAndFlag.first),
                    std::placeholders::_1),
                m_settings,
                m_relayClusterClient);
    }
    else
    {
        NX_LOGX(lm("Connect request retransmit %1")
            .arg(logRequest(connection, request)), cl_logDEBUG2);
    }

    // Launching connect FSM.
    connectionFsmIterAndFlag.first->second->onConnectRequest(
        connection,
        std::move(request),
        std::move(completionHandler));
}

void HolePunchingProcessor::onConnectionAckRequest(
    const ConnectionStrongRef& connection,
    api::ConnectionAckRequest request,
    network::stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_activeConnectSessions.find(request.connectSessionId);
    if (connectionIter == m_activeConnectSessions.end())
    {
        NX_LOGX(lm("Connect ACK from %1, connection id %2 is unknown")
            .args(connection->getSourceAddress(), request.connectSessionId), cl_logDEBUG1);

        return completionHandler(api::ResultCode::notFound);
    }

    NX_VERBOSE(this, lm("Connect ACK from %1, connection id %2")
        .args(connection->getSourceAddress(), request.connectSessionId));

    connectionIter->second->onConnectionAckRequest(
        connection,
        std::move(request),
        std::move(completionHandler));
}

void HolePunchingProcessor::connectionResult(
    const ConnectionStrongRef& connection,
    api::ConnectionResultRequest request,
    network::stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_activeConnectSessions.find(request.connectSessionId);
    if (connectionIter == m_activeConnectSessions.end())
    {
        NX_LOGX(lm("Connect result from %1, connection id %2 is unknown")
            .args(connection->getSourceAddress(), request.connectSessionId), cl_logDEBUG1);

        return completionHandler(api::ResultCode::notFound);
    }

    NX_LOGX(lm("Connect result from %1, connection id %2, result: %3")
        .args(connection->getSourceAddress(), request.connectSessionId,
            QnLexical::serialized(request.resultCode)), cl_logDEBUG2);

    auto statisticsInfo = connectionIter->second->statisticsInfo();
    statisticsInfo.resultCode = request.resultCode;
    statisticsInfo.endTime = nx::utils::utcTime();
    m_statisticsCollector->saveConnectSessionStatistics(std::move(statisticsInfo));

    connectionIter->second->onConnectionResultRequest(
        std::move(request),
        std::move(completionHandler));
}

std::tuple<api::ResultCode, boost::optional<ListeningPeerPool::ConstDataLocker>>
    HolePunchingProcessor::validateConnectRequest(
        const ConnectionStrongRef& connection,
        const api::ConnectRequest& request)
{
    if (connection->transportProtocol() != nx::network::TransportProtocol::udp)
    {
        NX_LOGX(lm("Failed connect request %1: only UDP hole punching is supported for now")
            .arg(logRequest(connection, request)), cl_logDEBUG1);

        return std::make_tuple(api::ResultCode::badRequest, boost::none);
    }

    auto targetPeerDataLocker = m_listeningPeerPool->
        findAndLockPeerDataByHostName(request.destinationHostName);
    if (!targetPeerDataLocker)
    {
        NX_LOGX(lm("Failed connect request %1: host is unknown")
            .arg(logRequest(connection, request)), cl_logDEBUG1);

        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }
    const auto& targetPeerData = targetPeerDataLocker->value();

    if (!targetPeerData.isListening)
    {
        NX_LOGX(lm("Failed connect request %1: host is not listening")
            .arg(logRequest(connection, request)), cl_logDEBUG1);

        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    if (((targetPeerData.connectionMethods & request.connectionMethods) &
        api::ConnectionMethod::udpHolePunching) == 0)
    {
        NX_LOGX(lm("Failed connect request %1: no suitable connection method")
            .arg(logRequest(connection, request)), cl_logDEBUG1);

        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    if (!targetPeerData.peerConnection)
    {
        NX_LOGX(lm("Failed connect request %1: no connection to the target peer")
            .arg(logRequest(connection, request)), cl_logDEBUG1);

        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    return std::make_tuple(api::ResultCode::ok, std::move(targetPeerDataLocker));
}

void HolePunchingProcessor::connectSessionFinished(
    ConnectSessionsDictionary::iterator sessionIter,
    api::NatTraversalResultCode connectionResult)
{
    QnMutexLocker lk(&m_mutex);

    if (m_activeConnectSessions.empty())
        return; //< HolePunchingProcessor is being stopped currently?

    NX_LOGX(lm("Connect session %1 finished with result %2").
        arg(sessionIter->first).arg(QnLexical::serialized(connectionResult)),
        cl_logDEBUG2);

    m_activeConnectSessions.erase(sessionIter);
}

} // namespace hpm
} // namespace nx
