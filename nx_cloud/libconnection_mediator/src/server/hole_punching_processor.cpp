/**********************************************************
* Jan 13, 2016
* akolesnikov
***********************************************************/

#include "hole_punching_processor.h"

#include <nx/network/stun/message_dispatcher.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {

HolePunchingProcessor::HolePunchingProcessor(
    AbstractCloudDataProvider* cloudData,
    nx::stun::MessageDispatcher* dispatcher,
    ListeningPeerPool* const listeningPeerPool)
:
    RequestProcessor(cloudData),
    m_listeningPeerPool(listeningPeerPool)
{
    dispatcher->registerRequestProcessor(
        stun::cc::methods::connect,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithOutput(
                &HolePunchingProcessor::connect,
                this,
                std::move(connection),
                std::move(message));
        });

    dispatcher->registerRequestProcessor(
        stun::cc::methods::connectionAck,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithNoOutput(
                &HolePunchingProcessor::onConnectionAckRequest,
                this,
                std::move(connection),
                std::move(message));
        });

    dispatcher->registerRequestProcessor(
        stun::cc::methods::connectionResult,
        [this](const ConnectionStrongRef& connection, stun::Message message)
        {
            processRequestWithNoOutput(
                &HolePunchingProcessor::connectionResult,
                this,
                std::move(connection),
                std::move(message));
        });
}

HolePunchingProcessor::~HolePunchingProcessor()
{
    ConnectSessionsDictionary localSessions;
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(localSessions, m_activeConnectSessions);
    }

    if (localSessions.empty())
        return;

    std::promise<void> allSessionsStoppedPromise;
    {
        nx::BarrierHandler barrier(
            [&allSessionsStoppedPromise]() { allSessionsStoppedPromise.set_value(); });
        for (const auto& connectSession: localSessions)
            connectSession.second->pleaseStop(barrier.fork());
    }

    //waiting for all sessions to stop...
    allSessionsStoppedPromise.get_future().wait();
}

void HolePunchingProcessor::connect(
    const ConnectionStrongRef& connection,
    api::ConnectRequest request,
    stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler)
{
    NX_LOGX(lm("connect request. from %1 to host %2, connection id %3").
        arg(connection->getSourceAddress().toString()).
        arg(request.destinationHostName).arg(request.connectSessionId),
        cl_logDEBUG2);

    api::ResultCode validationResult = api::ResultCode::ok;
    boost::optional<ListeningPeerPool::ConstDataLocker> targetPeerDataLocker;
    std::tie(validationResult, targetPeerDataLocker) = validateConnectRequest(
        connection,
        request);
    if (validationResult != api::ResultCode::ok)
        return completionHandler(validationResult, api::ConnectResponse());
    assert(static_cast<bool>(targetPeerDataLocker));

    //preparing and saving connection context
    QnMutexLocker lk(&m_mutex);
    const auto connectionFsmIterAndFlag = m_activeConnectSessions.emplace(
        request.connectSessionId,
        nullptr);
    if (!connectionFsmIterAndFlag.second)
    {
        NX_LOGX(lm("connect request retransmit: from %1, to %2, connection id %3").
            arg(connection->getSourceAddress().toString()).
            arg(request.destinationHostName).arg(request.connectSessionId),
            cl_logDEBUG1);
        //ignoring request, response will be sent by fsm
        return;
    }

    connectionFsmIterAndFlag.first->second = 
        std::make_unique<UDPHolePunchingConnectionInitiationFsm>(
            request.connectSessionId,
            targetPeerDataLocker.get(),
            std::bind(
                &HolePunchingProcessor::connectSessionFinished,
                this,
                std::move(connectionFsmIterAndFlag.first),
                std::placeholders::_1));

    //launching connect FSM
    connectionFsmIterAndFlag.first->second->onConnectRequest(
        connection,
        std::move(request),
        std::move(completionHandler));
}

void HolePunchingProcessor::onConnectionAckRequest(
    const ConnectionStrongRef& connection,
    api::ConnectionAckRequest request,
    stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_activeConnectSessions.find(request.connectSessionId);
    if (connectionIter == m_activeConnectSessions.end())
    {
        completionHandler(api::ResultCode::notFound);
        return;
    }
    connectionIter->second->onConnectionAckRequest(
        connection,
        std::move(request),
        std::move(completionHandler));
}

void HolePunchingProcessor::connectionResult(
    const ConnectionStrongRef& /*connection*/,
    api::ConnectionResultRequest request,
    stun::Message /*requestMessage*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    QnMutexLocker lk(&m_mutex);
    auto connectionIter = m_activeConnectSessions.find(request.connectSessionId);
    if (connectionIter == m_activeConnectSessions.end())
    {
        completionHandler(api::ResultCode::notFound);
        return;
    }
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
        NX_LOGX(lm("connect request from %1: only UDP hole punching is supported for now").
            arg(connection->getSourceAddress().toString()),
            cl_logDEBUG1);
        return std::make_tuple(api::ResultCode::badRequest, boost::none);
    }

    auto targetPeerDataLocker = m_listeningPeerPool->
        findAndLockPeerDataByHostName(request.destinationHostName);
    if (!targetPeerDataLocker)
    {
        NX_LOGX(lm("connect request from %1 to unknown host %2").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }
    const auto& targetPeerData = targetPeerDataLocker->value();

    if (!targetPeerData.isListening)
    {
        NX_LOGX(lm("connect request from %1 to host %2 not listening for connections").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    if (((targetPeerData.connectionMethods & request.connectionMethods) &
        api::ConnectionMethod::udpHolePunching) == 0)
    {
        NX_LOGX(lm("connect request from %1 (connection methods %2) to host %3 "
            "(connection methods %4). No suitable connection method").
            arg(connection->getSourceAddress().toString()).arg(request.connectionMethods).
            arg(request.destinationHostName).arg(targetPeerData.connectionMethods),
            cl_logDEBUG1);
        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    const auto peerConnectionStrongRef = targetPeerData.peerConnection.lock();
    if (!peerConnectionStrongRef)
    {
        NX_LOGX(lm("connect request from %1 to host %2. No connection to the target peer...").
            arg(connection->getSourceAddress().toString()).arg(request.destinationHostName),
            cl_logDEBUG1);
        return std::make_tuple(api::ResultCode::notFound, boost::none);
    }

    return std::make_tuple(api::ResultCode::ok, std::move(targetPeerDataLocker));
}

void HolePunchingProcessor::connectSessionFinished(
    ConnectSessionsDictionary::iterator sessionIter,
    api::ResultCode connectionResult)
{
    QnMutexLocker lk(&m_mutex);

    if (m_activeConnectSessions.empty())
        return; //HolePunchingProcessor is being stopped currently?

    NX_LOGX(lm("connect session %1 finished with result %2").
        arg(sessionIter->first).arg(QnLexical::serialized(connectionResult)),
        cl_logDEBUG2);

    m_activeConnectSessions.erase(sessionIter);
}

} // namespace hpm
} // namespace nx
